#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    struct sockaddr_in addr;
} TargetAddr;

typedef struct ConnItem {
    struct bufferevent *bev;
    bool is_abandoned; // true = 已被主连接成功后抛弃，残留回调直接丢弃
} ConnItem;

// 上层业务读写回调（连接成功后挂载）
typedef void (*DataReadCb)(struct bufferevent *bev, void *arg);

typedef struct MultiConnCtx MultiConnCtx;

struct MultiConnCtx
{
    struct event_base *base;
    TargetAddr *targets;
    int target_cnt;
	int max_cnt;


    ConnItem *conn_items;
    int item_index;

    bool selected;
    ConnItem *success_item;

    int fail_count;

    // 上层回调
    void (*on_connected)(MultiConnCtx * ctx, void *arg);
	void (*on_disconnected)(MultiConnCtx * ctx,void *arg);
    void (*on_connect_faild)(MultiConnCtx * ctx,void *arg);

    void *user_arg;

    // 自动重连相关
    struct event *retry_ev;       // 重连延迟定时器
    struct timeval retry_tv;      // 重连等待间隔
    DataReadCb data_read_cb;      // 业务读回调
} ;

MultiConnCtx* multi_conn_create(struct event_base *base);

// 设置自动重连间隔，tv传NULL关闭自动重连
void multi_conn_set_retry(MultiConnCtx *ctx,unsigned long ms);

// 设置业务数据读回调（连接成功后生效）
void multi_conn_set_read_cb(MultiConnCtx *ctx, DataReadCb read_cb);

int multi_conn_start(MultiConnCtx *ctx);

void multi_conn_set_callback(
	MultiConnCtx *ctx,
	void (*onConneted)(MultiConnCtx * ctx, void*),
	void (*onDisconnected)(MultiConnCtx * ctx, void*),
	void (*on_failed)(MultiConnCtx * ctx,void*), 
	void *arg);

void multi_conn_destroy(MultiConnCtx *ctx);
void register_addr(MultiConnCtx *ctx,const char *ip, uint16_t port);
void register_front(MultiConnCtx *ctx,char *pszFrontAddress);
void reset_addr(MultiConnCtx *ctx);
int  send_data(MultiConnCtx *ctx,const char * data,int len);

struct bufferevent * connect_bev(MultiConnCtx *ctx);