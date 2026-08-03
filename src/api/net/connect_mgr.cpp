#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "connect_mgr.h"


const char * protocal_prefix[] = 
{
	"tcp://",
	"tcp6://",
	"udp://",
	"udp6://",
	"sms://",
	"smk://",
	"smi://",
	"sma://",
	"sms6://",
	"smk6://",
	"smi6://",
	"sma6://"
};

const char * check_broker_addr(const char * addr ,long len,long *port_out,const char ** ip_out,int *ip_len)
{
	if( !addr )
		return 0;
	if( len < 0 )
		len = strlen(addr );

	if( len < 6 )
		return 0;

	int i = 0;
	int cnt = sizeof(protocal_prefix) / sizeof(const char *);
	const char * ip = 0;
	const char * port_start_ptr = 0;

	int index = 0;
	for( i=0; i< cnt; i++ )
	{
		ip = strstr(addr,protocal_prefix[i]);
		if( ip )
		{
			port_start_ptr = ip + strlen(protocal_prefix[i]);
			ip = port_start_ptr;
			index = i;
			break;
		}
	}

	if( !ip )
		return 0;

	if( !port_start_ptr )
		return 0;

	const char * port_end = port_start_ptr + (strlen(port_start_ptr) - 1);

	if( index > 3 )
	{
		while( port_end > port_start_ptr )
		{
			if( *port_end == '/')
			{
				port_end--;
				break;
			}
			port_end--;
		}
		//商密格式
	}

	const char * port_ptr = port_end;
	while( port_ptr > port_start_ptr )
	{
		if( *port_ptr == ':')
			break;
		port_ptr--;
	}
	if( port_ptr == port_start_ptr )
		return 0;

	if( ip_len )
	{
		*ip_len = port_ptr - ip;
	}

	if( port_ptr[0] == ':')
		port_ptr++;

	if( port_out )
	{
		*port_out = atol(port_ptr);
	}
	if( ip_out )
	{
		*ip_out = ip;
	}

	return addr;
}


static void single_conn_event_cb(struct bufferevent *bev, short events, void *arg);
static void retry_connect_cb(evutil_socket_t fd, short what, void *arg);

void reset_addr(MultiConnCtx *ctx)
{
	memset(ctx->targets,0,sizeof(TargetAddr) * ctx->max_cnt);
	ctx->target_cnt = 0;
}

void register_addr(MultiConnCtx *ctx,const char *ip, uint16_t port)
{
	if( !ctx )
		return;
	if( ctx->target_cnt >= ctx->max_cnt)
		return;
	TargetAddr * t = &ctx->targets[ctx->target_cnt];
    ctx->target_cnt++;
    memset( t, 0, sizeof(TargetAddr));
    t->addr.sin_family = AF_INET;
    t->addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &t->addr.sin_addr);
}

void register_front(MultiConnCtx *ctx,char *pszFrontAddress)
{
	const char *ip = 0;
	int ip_len = 0;
	long port = 0;

	if( !check_broker_addr(pszFrontAddress ,-1,&port,&ip,&ip_len) )
	{
		//无效的地址格式
		printf("register_front invalid addr %s\n",pszFrontAddress);
		return;
	}
    printf("register_front %s %d,ip_len = %d\n",ip,port,ip_len);
	if( ip_len < 0 )
		return;
	if( ip_len > 127 )
		return;
	char ip_buffer[128];
	memcpy(ip_buffer,ip,ip_len);
	ip_buffer[ip_len] = 0;
	register_addr(ctx,ip_buffer,port);
}

MultiConnCtx* multi_conn_create(struct event_base *base)
{
    if (!base ) return NULL;
    MultiConnCtx *ctx = (MultiConnCtx *)calloc(1, sizeof(MultiConnCtx));
    ctx->base = base;
    ctx->max_cnt = 10;
	ctx->target_cnt = 0;
	ctx->item_index = -1;
    ctx->targets = (TargetAddr *)malloc(sizeof(TargetAddr) * ctx->max_cnt);
	memset(ctx->targets,0,sizeof(TargetAddr) * ctx->max_cnt);

    ctx->conn_items = (ConnItem*)calloc(ctx->max_cnt, sizeof(struct ConnItem));

    //创建重连定时器（默认不启用）
    ctx->retry_ev = event_new(base, -1, EV_TIMEOUT, retry_connect_cb, ctx);
    timerclear(&ctx->retry_tv);
    return ctx;
}

void multi_conn_set_retry(MultiConnCtx *ctx, unsigned long ms)
{
    if (!ctx) return;
    if ( ms > 0 ) {
        ctx->retry_tv.tv_sec = ms/1000;
		ctx->retry_tv.tv_usec = (ms %1000) * 1000;
    } else {
        timerclear(&ctx->retry_tv);
    }
}

void multi_conn_set_read_cb(MultiConnCtx *ctx, DataReadCb read_cb)
{
    if (!ctx) return;
    ctx->data_read_cb = read_cb;
}

static void close_all_other_bev(MultiConnCtx *ctx, struct bufferevent *keep)
{
    for (int i = 0; i < ctx->target_cnt; i++) {
		ConnItem *item = &ctx->conn_items[i];
        if (!item->bev) continue;
        if (item->bev == keep) continue;

        printf("close_all_other_bev close bev %p\n", item->bev);
        item->is_abandoned = true;
        if( item->bev )
            bufferevent_free(item->bev);
        item->bev = nullptr;
    }
}

// 选中连接断开，触发延迟重连
static void trigger_retry(MultiConnCtx *ctx)
{
    // 无重连间隔则不重试
    if (timerisset(&ctx->retry_tv) == 0) {
        printf("auto retry disabled\n");
        return;
    }
    // 取消已有未触发的重连定时
    event_del(ctx->retry_ev);
    // 启动延迟重连
    event_add(ctx->retry_ev, &ctx->retry_tv);
    printf("connection lost, will retry after %lds.%06lds\n",
           ctx->retry_tv.tv_sec, ctx->retry_tv.tv_usec);
}

// 重连定时器回调：重新发起多地址并发连接
static void retry_connect_cb(evutil_socket_t fd, short what, void *arg)
{
    (void)fd; (void)what;
    MultiConnCtx *ctx = (MultiConnCtx *)arg;
    printf("start retry multi connect...\n");

    ctx->selected = false;
    ctx->success_item = NULL;
    ctx->fail_count = 0;

    // 清空旧bev列表
    close_all_other_bev(ctx, NULL);
    memset(ctx->conn_items, 0, sizeof(struct ConnItem) * ctx->max_cnt);

    // 重新发起一轮择优连接
    multi_conn_start(ctx);
}

static void single_conn_event_cb(struct bufferevent *bev, short events, void *arg)
{
    
    MultiConnCtx *ctx = (MultiConnCtx *)arg;
	ConnItem *target_item = NULL;
    printf("single_conn_event_cb bev %p events %d, target_cnt = %d\n", bev, events, ctx->target_cnt);

	// 第一步：查找当前bev对应的item，判断是否已废弃
    for (int i = 0; i < ctx->target_cnt; i++)
    {
        printf("ctx->conn_items[i].bev = %p\n",ctx->conn_items[i].bev);
        if (ctx->conn_items[i].bev == bev)
        {
            target_item = &ctx->conn_items[i];
            break;
        }
    }
    // 找不到 或 已标记废弃：直接退出，不处理任何逻辑
    if (!target_item || target_item->is_abandoned)
    {
        printf("single_conn_event_cb bev not found or abandoned target_item = %p\n", target_item);
        return;
    }

    // 连接成功
    if (events & BEV_EVENT_CONNECTED) {
        if (ctx->selected) {
            bufferevent_free(bev);
            printf("single_conn_event_cb bev %p already selected\n", bev);
            return;
        }
        ctx->selected = true;
        ctx->success_item = target_item;
        close_all_other_bev(ctx, bev);
        // 挂载业务读回调，替换空读回调
        bufferevent_setcb(bev, ctx->data_read_cb, NULL, single_conn_event_cb, ctx);
        bufferevent_enable(bev, EV_READ | EV_WRITE);

        printf("single_conn_event_cb bev %p connected\n", ctx->success_item->bev);

        if (ctx->on_connected) {
            ctx->on_connected(ctx, ctx->user_arg);
        }
        return;
    }

    // 连接断开 / 错误
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bool is_active_conn = (ctx->selected && ctx->success_item == target_item);
        ctx->fail_count++;

        // 当前是正在使用的主连接断开，启动自动重连
        if (is_active_conn) {
            
            ctx->on_disconnected(ctx,ctx->user_arg);
            ctx->selected = false;
            ctx->success_item = nullptr;
            bufferevent_free(bev);
            target_item->bev = nullptr;

            multi_conn_set_retry(ctx, 3000);
            trigger_retry(ctx);
            return;
        }

        printf("single_conn_event_cb bev %p disconnected\n", bev);
        // 全部备选连接同时失败，仅日志，等待重连定时
        if (ctx->fail_count >= ctx->target_cnt && !ctx->selected) 
        {
            printf("single_conn_event_cb all target addr failed\n");
            if (ctx->on_connect_faild) {
                ctx->on_connect_faild(ctx,ctx->user_arg);
            }
            bufferevent_free(bev);
            target_item->bev = nullptr;

            multi_conn_set_retry(ctx, 3000);
            trigger_retry(ctx);
        }
        else{
            bufferevent_free(bev);
            target_item->bev = nullptr;
        }
    }
}

void multi_conn_set_callback(
	MultiConnCtx *ctx,
	void (*onConneted)(MultiConnCtx * ctx, void*),
	void (*onDisconnected)(MultiConnCtx * ctx, void*),
	void (*on_failed)(MultiConnCtx * ctx,void*), 
	void *arg)
{
    if (!ctx || ctx->selected) return ;
    ctx->on_connected = onConneted;
	ctx->on_disconnected = onDisconnected;
    ctx->on_connect_faild = on_failed;
    ctx->user_arg = arg;
}

struct bufferevent * connect_bev(MultiConnCtx *ctx)
{
    if (!ctx || !ctx->success_item) return NULL;
    return ctx->success_item->bev;
}

int multi_conn_start(MultiConnCtx *ctx)
{
    if (!ctx || ctx->selected) return -1;
	if( ctx->target_cnt == 0)
    {
        printf("multi_conn_start no target addr\n");
		return -2;
    }
    ctx->fail_count = 0;


    // 批量并发连接所有地址
    for (int i = 0; i < ctx->target_cnt; i++) {
        struct bufferevent *bev = bufferevent_socket_new(
            ctx->base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE
        );
        if (!bev) continue;

        ctx->conn_items[i].bev = bev;
        printf("multi_conn_start bev %p,single_conn_event_cb\n", bev);
        // 初始读回调为空，连接成功后替换为业务回调
        bufferevent_setcb(bev, ctx->data_read_cb, NULL, single_conn_event_cb, ctx);
        bufferevent_enable(bev, EV_READ | EV_WRITE);

        TargetAddr *t = &ctx->targets[i];
        int ret = bufferevent_socket_connect(
            bev, (struct sockaddr*)&t->addr, sizeof(struct sockaddr_in)
        );
        if (ret < 0) {
            printf("multi_conn_start bev %p connect failed\n", bev);
            bufferevent_free(bev);
            ctx->conn_items[i].bev = NULL;
            ctx->fail_count++;
        }
    }

    if (ctx->fail_count >= ctx->target_cnt) {
        printf("multi_conn_start all target addr failed 2\n");
        if (ctx->on_connect_faild) ctx->on_connect_faild(ctx,ctx->user_arg);

        multi_conn_set_retry(ctx, 3000);
		trigger_retry(ctx);
        return -2;
    }
    return 0;
}

void multi_conn_destroy(MultiConnCtx *ctx)
{
    if (!ctx) return;
    // 停止重连定时器
    event_del(ctx->retry_ev);
    event_free(ctx->retry_ev);

    close_all_other_bev(ctx, NULL);
    free(ctx->conn_items);
    free(ctx->targets);
    free(ctx);
}


// 业务数据读回调（连接正常后接收服务端消息）
// void mgr_data_read_cb(struct bufferevent *bev, void *arg)
// {
//     (void)arg;
//     char buf[1024];
//     size_t n = evbuffer_remove(bufferevent_get_input(bev), buf, sizeof(buf)-1);
//     if (n <= 0) return;
//     buf[n] = 0;
//     printf("recv server data: %s\n", buf);
// }


int  send_data(MultiConnCtx *ctx,const char * data,int len)
{
	if( !ctx->success_item )
		return -1;
	if( bufferevent_write(ctx->success_item->bev, data, len) == 0 )
		return len;
	return 0;
}

// int main()
// {
//     struct event_base *base = event_base_new();

//     TargetAddr addrs[2];
//     fill_target_addr(&addrs[0], "127.0.0.1", 8888);
//     fill_target_addr(&addrs[1], "127.0.0.1", 9999);

//     MultiConnCtx *mctx = multi_conn_create(base, addrs, 2);

//     // 1. 设置业务读回调
//     multi_conn_set_read_cb(mctx, data_read_cb);

//     // 2. 开启自动重连：断开后等待3秒重试
//     struct timeval retry_tv = {3, 0};
//     multi_conn_set_retry(mctx, &retry_tv);

//     // 启动第一轮多地址择优连接
//     multi_conn_start(mctx, on_connect_success, on_connect_fail, NULL);

//     event_base_dispatch(base);

//     multi_conn_destroy(mctx);
//     event_base_free(base);
//     return 0;
// }