#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <signal.h>

#ifdef WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
    #define JAN1970 116444736000000000LL
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include <string>

#include <sqlite3.h>

#include "trade_session.h"
#include "trade_mgr.h"

// ===================== 全局常量 =====================
#define CMD_SOCK_PATH "/tmp/ctp_sandbox.sock"
#define LOG_FILE "./server.log"
#define SHUTDOWN_CMD "shutdown"

sqlite3 *  g_cfg_db = 0;

// ===================== 配置结构体 =====================
typedef struct {
    char * listen_ip;
    char listen_ip_buffer[128];
    uint16_t listen_port;
} ServerConfig;

struct event_base *g_base = NULL;
static struct evconnlistener *g_listener = NULL;
static struct evconnlistener *g_cmd_listener = NULL;
static int g_server_running = 1;

bool g_bIsClose = false;    
bool g_bIsPause = false;    

long long g_alert_order = 0;
long long g_alert_cancel = 0;

std::string g_app_id = "test";
std::string g_auth_code = "test";
bool g_bIsProductionMode = true;

ServerConfig g_cfg = {0};

FILE *g_log_fp = 0;

struct my_timeval
{
    long long    tv_sec;         /* seconds */
    long long    tv_usec;        /* and microseconds */
};


int	   my_gettimeofday(struct my_timeval * tv)
{
#ifdef WIN32
    union {
	FILETIME ft;
	long long since1601;	/* time since 1 Jan 1601 in 100ns units */
    } data;

    GetSystemTimeAsFileTime(&data.ft);
    tv->tv_usec =((data.since1601 / 10LL) % 1000000LL);
    tv->tv_sec = ((data.since1601 - JAN1970) / 10000000LL);
    return (0);
#else
	struct timeval tv1;
	int ret;
	ret = gettimeofday(&tv1,NULL);
	tv->tv_sec = tv1.tv_sec;
	tv->tv_usec = tv1.tv_usec;
	return ret;
#endif
}

struct tm * my_localtime(const long long *timep,struct tm * ptm)
{
	struct tm * pret = NULL;
#if defined(WIN32)
	localtime_s(ptm,(time_t*)timep);
	pret = ptm;
#else
	if( ptm)
		pret = localtime_r((time_t*)timep,ptm);
	else
		pret = localtime((time_t*)timep);
#endif
	return pret;
};

unsigned int my_get_day()
{
	struct tm tm_buf = {0};
	struct tm *pcurt;
    my_timeval tv_buffer;
    my_gettimeofday((struct my_timeval*)&tv_buffer);
	my_timeval *tv = &tv_buffer;
    pcurt = my_localtime(&tv->tv_sec,&tm_buf);
    return (pcurt->tm_year + 1900) * 10000 + (pcurt->tm_mon + 1) * 100 + pcurt->tm_mday;
}

void log_msg(const char *fmt, ...)
{
    if( !g_log_fp )
        return;
	struct tm tm_buf = {0};
	struct tm *pcurt;
    my_timeval tv_buffer;
    my_gettimeofday((struct my_timeval*)&tv_buffer);
	my_timeval *tv = &tv_buffer;
    pcurt = my_localtime(&tv->tv_sec,&tm_buf);

    fprintf(g_log_fp, "[%4d-%02d-%02d %02d:%02d:%02d %03d] ",pcurt->tm_year + 1900,pcurt->tm_mon+1,pcurt->tm_mday,pcurt->tm_hour,pcurt->tm_min,pcurt->tm_sec,(tv->tv_usec / 1000) %1000);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_fp, fmt, args);
    fflush(g_log_fp);

    va_end(args);
}

#define GBK_BUF_SIZE  4096
#define UTF8_BUF_SIZE (GBK_BUF_SIZE * 2)

#ifndef WIN32

#include <iconv.h>


/**
 * @brief GBK -> UTF8 转换封装
 * @param in_gbk 输入GBK缓冲区
 * @param in_len 输入长度
 * @param out_utf8 输出UTF8缓冲区
 * @param out_cap 输出缓冲区容量
 * @return 成功返回0，失败-1
 */
static int gbk_to_utf8_buf(const char *in_gbk, size_t in_len,
                           char *out_utf8, size_t out_cap)
{
    iconv_t cd = iconv_open("UTF-8", "GBK");
    if (cd == (iconv_t)-1)
    {
        return -1;
    }

    char *in_ptr  = (char *)in_gbk;
    char *out_ptr = out_utf8;
    size_t ilen   = in_len;
    size_t olen   = out_cap;

    size_t ret = iconv(cd, &in_ptr, &ilen, &out_ptr, &olen);
    iconv_close(cd);

    if (ret == (size_t)-1)
    {
        return -1;
    }
    *out_ptr = '\0';
    return 0;
}

#endif

void gbk_printf(const char *fmt, ...)
{
	struct tm tm_buf = {0};
	struct tm *pcurt;
    my_timeval tv_buffer;
    my_gettimeofday((struct my_timeval*)&tv_buffer);
	my_timeval *tv = &tv_buffer;
    pcurt = my_localtime(&tv->tv_sec,&tm_buf);

    printf("[%4d-%02d-%02d %02d:%02d:%02d %03d] ",pcurt->tm_year + 1900,pcurt->tm_mon+1,pcurt->tm_mday,pcurt->tm_hour,pcurt->tm_min,pcurt->tm_sec,(tv->tv_usec / 1000) %1000);

#ifdef WIN32
    va_list ap;
    va_start(ap,fmt);
    vprintf(fmt, ap);
    va_end(ap);
#else
    char gbk_buf[GBK_BUF_SIZE];
    char utf8_buf[UTF8_BUF_SIZE];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(gbk_buf, sizeof(gbk_buf), fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= sizeof(gbk_buf))
    {
        // 缓冲区不足，降级：直接原始输出（防止丢数据）
        fputs("[BUF_OVERFLOW]", stdout);
        fputs(gbk_buf, stdout);
        return ;
    }

    if (gbk_to_utf8_buf(gbk_buf, (size_t)n, utf8_buf, sizeof(utf8_buf)) != 0)
    {
        // 转码失败，原样输出GBK
        fputs("[CONV_FAIL]", stdout);
        fputs(gbk_buf, stdout);
        return ;
    }

    fputs(utf8_buf, stdout);
    return ;

#endif    
}

// ===================== 通用优雅关闭入口（统一被信号/命令调用） =====================
static void graceful_shutdown(void)
{
    if (!g_server_running) return;
    g_server_running = 0;
    printf("\n[SHUTDOWN] Start graceful exit\n");

    // 1.销毁TCP监听 + 命令监听
    if (g_listener) {
        evconnlistener_free(g_listener);
        g_listener = NULL;
        printf("[SHUTDOWN] TCP listener closed\n");
    }
    if (g_cmd_listener) {
        evconnlistener_free(g_cmd_listener);
        g_cmd_listener = NULL;
        unlink(CMD_SOCK_PATH);
        printf("[SHUTDOWN] Command socket removed\n");
    }


    printf("[SHUTDOWN] All client connections closed\n");

    // 3.退出事件循环
    event_base_loopexit(g_base, NULL);
}

void gbk_printf(const char *fmt, ...);

// ===================== JSON配置加载 =====================
static int load_config_from_db(const char *dbfile)
{
    int nRet = sqlite3_open_v2(dbfile, &g_cfg_db,SQLITE_OPEN_READWRITE ,NULL);
    if (nRet != SQLITE_OK)
	{
		gbk_printf("配置数据库 %s 打开失败,ret = %d\n",dbfile,nRet);
		return 0;
	}
    nRet = sqlite3_exec(g_cfg_db, "PRAGMA synchronous = OFF;", 0, 0, 0);
    nRet = sqlite3_exec(g_cfg_db, "PRAGMA journal_mode = WAL;", 0, 0, 0);

    sqlite3_stmt *stmt = 0;
    nRet = sqlite3_prepare (g_cfg_db, "select id,vals from t_cfg", -1, &stmt, 0);
    if (nRet != SQLITE_OK)
	{
		return 0;
	}

    const char * id = 0;
    const char * vals = 0;

    do
	{
		nRet = sqlite3_step(stmt);
		if( nRet != SQLITE_ROW )
			break;
		id = (const char *)sqlite3_column_text(stmt,0);
		vals = (const char *)sqlite3_column_text(stmt,1);

        if(strcmp(id,"listen_ip") == 0)
        {
            strncpy(g_cfg.listen_ip_buffer,vals,sizeof(g_cfg.listen_ip_buffer));
            g_cfg.listen_ip = g_cfg.listen_ip_buffer;
        }
        else if(strcmp(id,"listen_port") == 0)
            g_cfg.listen_port = atoi(vals);
        else if(strcmp(id,"app_id") == 0)
            g_app_id = vals;
        else if(strcmp(id,"auth_code") == 0)
            g_auth_code = vals;
        else if(strcmp(id,"ProductionMode") == 0)
        {
            if( atoi(vals) == 0 )
                g_bIsProductionMode = false;
            else
                g_bIsProductionMode = true;
        }
	}while(1);

    sqlite3_finalize(stmt);

    return 0;
}

// ===================== TCP业务回调 =====================
static void client_read_cb(struct bufferevent *bev, void *arg)
{
    TradeSession * session = (TradeSession *)arg;
    session->OnClientData();
}

static void client_event_cb(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_EOF)
        printf("Client closed normally\n");
    else if (events & BEV_EVENT_ERROR)
        printf("Client error errno:%d\n", errno);
    printf("client_event_cb bev = %p\n", bev);
    bufferevent_free(bev);
    delete (TradeSession *)arg;
}

static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                      struct sockaddr *addr, int socklen, void *arg)
{
    if (!g_server_running) {
        evutil_closesocket(fd);
        return;
    }
    evutil_make_socket_nonblocking(fd);
    struct bufferevent *bev = bufferevent_socket_new(g_base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    if (!bev) {
        evutil_closesocket(fd);
        return;
    }

    //这里分配一个客户端节点

    TradeSession * session = new TradeSession(g_bIsProductionMode);
    session->net.netCtx = bev;

    bufferevent_setcb(bev, client_read_cb, NULL, client_event_cb, session);
    bufferevent_enable(bev, EV_READ | EV_WRITE); //需要等服务器端连接成功后，再开始从客户端读取数据，避免数据丢失 | EV_WRITE
    
    char ip[INET_ADDRSTRLEN];
    uint16_t p = ntohs(((struct sockaddr_in *)addr)->sin_port);
    inet_ntop(AF_INET, &((struct sockaddr_in *)addr)->sin_addr, ip, sizeof(ip));
    printf("New client %s:%d ,bev = %p\n", ip, p, bev);
}

static void listener_err_cb(struct evconnlistener *lst,  void *arg)
{
    fprintf(stderr, "TCP listener accept error\n");
}

// ===================== 本地命令Socket处理回调 =====================



//evutil_socket_t fd, short what, void *arg

int handle_cmd(const char * cmd)
{
   if (strstr(cmd, SHUTDOWN_CMD) != NULL) {
        // 给命令行返回提示
        return 1;
    }

    if (strstr(cmd, "disable") != NULL) {
        // 强制账号退出
        g_bIsClose = true;
        printf("禁用账号\n");
        log_msg("禁用账号\n");
        return 0;
    }
    if (strstr(cmd, "enable") != NULL) {
        // 强制账号退出
        g_bIsClose = false;
        printf("启用账号\n");
        log_msg("启用账号\n");
        return 0;
    }

    if (strstr(cmd, "pause") != NULL) {
        // 暂停策略交易
        g_bIsPause = true;
        gbk_printf("暂停策略交易\n");
        log_msg("暂停策略交易\n");
        return 0;
    }
    if (strstr(cmd, "resume") != NULL) {
        // 恢复策略交易
        g_bIsPause = false;
        gbk_printf("恢复策略交易\n");
        log_msg("恢复策略交易\n");
        return 0;
    }

    const char * alert_ptr = strstr(cmd, "alert");
    if( alert_ptr != NULL )
    {
        const char * alert_type = strstr(alert_ptr,"order ");

        if( alert_type != NULL )
        {
            // 插入订单警报
            g_alert_order = atol(alert_type + 6);
            gbk_printf("设置 报单 数量告警阈值 %lld\n",g_alert_order);
            log_msg("设置 报单 数量告警阈值 %lld\n",g_alert_order);
            return 0;
        }
        alert_type = strstr(alert_ptr,"cancel ");
        if( alert_type != NULL )
        {
            // 取消订单警报
            g_alert_cancel = atol(alert_type + 7);
            gbk_printf("设置 撤单 数量告警阈值 %lld\n",g_alert_cancel);
            log_msg("设置 撤单 数量告警阈值 %lld\n",g_alert_cancel);
            return 0;
        }
    }
    return 0;
}

static void cmd_read_cb(struct bufferevent *bev, void *arg)
{
    char buf[512] = {0};
    size_t n = bufferevent_read(bev, buf, sizeof(buf)-1);
    if (n <= 0) return;

    printf("cmd_read_cb !!! ret = %d\n",n);
    if( handle_cmd(buf) )
    {
        struct evbuffer *out = bufferevent_get_output(bev);
        evbuffer_add_printf(out, "Receive shutdown command, server exiting...\n");
        bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);
        graceful_shutdown();
        return;
    }
}

void stdin_readcb(struct bufferevent *bev, void *arg) {
    struct evbuffer *input = bufferevent_get_input(bev);
    // 从输入缓冲区读取一行
    char *line = evbuffer_readln(input, NULL, EVBUFFER_EOL_LF);
    if (line) {
        // 将读取到的行发送给另一个bufferevent (比如网络连接)
        // 这里假设arg指向网络连接的bufferevent
        struct bufferevent *network_bev = (struct bufferevent*)arg;
        bufferevent_write(network_bev, line, strlen(line));
        bufferevent_write(network_bev, "\n", 1);
        free(line);
    }
}

void read_cb(struct bufferevent *bev, void *ctx) {
    char buffer[1024];
    int n;
    // 从输入缓冲区循环读取所有数据
    while ((n = bufferevent_read(bev, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("收到消息: %s", buffer);
    }
}

// 事件回调：处理连接、错误、超时等
void event_cb(struct bufferevent *bev, short events, void *ctx) {
    if (events & BEV_EVENT_CONNECTED) {
        printf("已连接到服务器！\n");
        // 连接成功后发送一条消息
        bufferevent_write(bev, "Hello, Server!", 14);
    } else if (events & BEV_EVENT_ERROR) {
        printf("发生错误: %s\n", strerror(errno));
    } else if (events & (BEV_EVENT_EOF | BEV_EVENT_TIMEOUT)) {
        printf("连接已关闭或超时\n");
    }
    // 发生错误或连接关闭时，释放bufferevent
    if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF | BEV_EVENT_TIMEOUT)) {
        bufferevent_free(bev);
    }
}

static void cmd_event_cb(struct bufferevent *bev, short events, void *arg)
{
    printf("cmd_event_cb !!!\n");
    bufferevent_free(bev);
}

static void cmd_accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                          struct sockaddr *addr, int socklen, void *arg)
{
    struct bufferevent *bev = bufferevent_socket_new(g_base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    if (!bev) {
        evutil_closesocket(fd);
        return;
    }
    bufferevent_setcb(bev, cmd_read_cb, NULL, cmd_event_cb, NULL);
    bufferevent_enable(bev, EV_READ);
}

#ifndef WIN32
// 初始化本地UNIX域命令监听
static int init_cmd_server(void)
{
    unlink(CMD_SOCK_PATH);
    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, CMD_SOCK_PATH);

    g_cmd_listener = evconnlistener_new_bind(
        g_base, cmd_accept_cb, NULL,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | BEV_OPT_THREADSAFE,
        -1,
        (struct sockaddr *)&sun, sizeof(sun)
    );
    if (!g_cmd_listener) {
        fprintf(stderr, "create cmd socket failed\n");
        return -1;
    }
    return 0;
}
#endif
// ===================== 信号回调（信号触发关闭） =====================
static void sig_handler(evutil_socket_t sig, short events, void *arg)
{
    printf("\nCatch signal %d, trigger shutdown\n", sig);
    graceful_shutdown();
}

// ===================== Daemon后台进程初始化 =====================
static int daemonize(void)
{
#ifndef WIN32    
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0); // 父进程退出

    setsid(); // 新建会话，脱离终端

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    // 修改工作目录
    chdir("/");
    umask(0);

    // 重定向标准输入输出到日志文件
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
#endif
    return 0;
}

static void libevent_log(int severity, const char* msg)
{
    (void)severity;
    printf("[LIBEV] %s\n", msg);
}

#ifdef WIN32
int create_socket_pair(SOCKET socks[2]) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return -1;
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    int len = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*)&addr, &len) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    if (listen(listener, 1) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    socks[1] = socket(AF_INET, SOCK_STREAM, 0);
    if (socks[1] == INVALID_SOCKET) {
        closesocket(listener);
        return -1;
    }
    
    if (connect(socks[1], (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        closesocket(socks[1]);
        return -1;
    }
    
    socks[0] = accept(listener, NULL, NULL);
    closesocket(listener);
    
    if (socks[0] == INVALID_SOCKET) {
        closesocket(socks[1]);
        return -1;
    }
    
    return 0;
}

// 读取控制台的线程函数
DWORD WINAPI console_reader_thread(LPVOID param) {
    SOCKET write_sock = (SOCKET)param;
    char buffer[1024];
    
    while (1) {
        // 从控制台读取输入
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // 发送到 socket
            send(write_sock, buffer, strlen(buffer), 0);
        }
    }
    return 0;
}

// 回调函数
void stdin_read_cb(struct bufferevent *bev, void *ctx) {
    struct evbuffer *input = bufferevent_get_input(bev);
    char *line = evbuffer_readln(input, NULL, EVBUFFER_EOL_LF);

    int ret = 0;
    if (line) {
        ret = handle_cmd(line);
    }
    free(line);

    if( ret )
    {
        graceful_shutdown();
    }
}

#endif

void net_worker(void * arg)
{
    event_set_log_callback(libevent_log);
#ifdef WIN32 
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // VS Windows平台固定调用，初始化临界区锁
    evthread_use_windows_threads();
#else   
    evthread_use_pthreads();
#endif

    g_base = event_base_new();
    if (!g_base)
    {
        printf("event_base_new failed\n");
        return ;
    }

    do
    {
#ifdef WIN32        
        SOCKET socks[2];
        if (create_socket_pair(socks) != 0) {
            printf("无法创建 socket pair\n");
            break;
        }
        
        // 创建线程读取控制台
        CreateThread(NULL, 0, console_reader_thread, (LPVOID)socks[1], 0, NULL);
        
        // 使用 socket 创建 bufferevent
        struct bufferevent *stdin_bev = bufferevent_socket_new(g_base, socks[0], 
                                                                BEV_OPT_CLOSE_ON_FREE);
        if (!stdin_bev) {
            printf("无法创建 bufferevent\n");
            break;
        }
        
        bufferevent_setcb(stdin_bev, stdin_read_cb, NULL, NULL, NULL);
        bufferevent_enable(stdin_bev, EV_READ);
#endif        
    }while(0);

    // 注册系统信号
    struct event *sig_int = evsignal_new(g_base, SIGINT, sig_handler, NULL);
    struct event *sig_term = evsignal_new(g_base, SIGTERM, sig_handler, NULL);
    event_add(sig_int, NULL);
    event_add(sig_term, NULL);

    // 启动TCP服务
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    if( g_cfg.listen_ip )
        inet_pton(AF_INET, g_cfg.listen_ip, &sin.sin_addr);
    sin.sin_port = htons(g_cfg.listen_port);

    g_listener = evconnlistener_new_bind(
        g_base, accept_cb, g_base,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | BEV_OPT_THREADSAFE,
        -1,
        (struct sockaddr *)&sin, sizeof(sin)
    );
    if (!g_listener) {

        event_free(sig_int);
        event_free(sig_term);        
        printf("event listen failed\n");
        return ;
    }
    evconnlistener_set_error_cb(g_listener, listener_err_cb);

#ifndef WIN32    
    // 启动本地命令服务
    if (init_cmd_server() < 0) {
        event_free(sig_int);
        event_free(sig_term);        
        evconnlistener_free(g_listener);
        printf("init_cmd_server failed\n");
        return ;
    }
#endif

    printf("Server started. Command sock: %s\n", CMD_SOCK_PATH);
    int ret = event_base_dispatch(g_base);

    printf("dispatch exited %d\n", ret);
    event_free(sig_int);
    event_free(sig_term);    
#ifdef WIN32
    printf("debug ... cleanup\n");
#endif    
}

int main(int argc, char **argv)
{
    // 屏蔽SIGPIPE
#ifndef WIN32    
    signal(SIGPIPE, SIG_IGN);
#endif
    // 启动参数：加 -d 代表后台daemon运行
    int run_daemon = 0;
    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        run_daemon = 1;
        if (daemonize() != 0) {
            fprintf(stderr, "daemon create fail\n");
            return 1;
        }
    }


    g_log_fp = fopen("trade_api.log", "a");

    if (load_config_from_db("ctp_sandbox.db") != 0) return 1;
    printf("Load config: %s:%d log:%s daemon:%s\n",g_cfg.listen_ip, g_cfg.listen_port, LOG_FILE, run_daemon ? "yes" : "no");
    //加载ctp的dll
    g_ctp_mgr = new TradeMgr ;
    if( !g_ctp_mgr->Init() )
    {
        delete g_ctp_mgr;
        g_ctp_mgr = 0;
        printf("trade mgr init failed\n");
        log_msg("trade mgr init failed\n");
        fclose(g_log_fp);
        sqlite3_close_v2(g_cfg_db);
        return 1;
    }

    net_worker(nullptr);
    //std::thread thread = std::thread(net_worker, nullptr);

    //thread.join();

    // 循环退出后资源释放
    printf("net_worker exit \n");
    event_base_free(g_base);

#ifdef WIN32
//    evthread_free_windows_threads();
    WSACleanup();
#endif
    

    delete g_ctp_mgr;
    g_ctp_mgr = 0;
    unlink(CMD_SOCK_PATH);
    printf("Server exited completely\n");
    log_msg("Server exited completely\n");
    fclose(g_log_fp);
    sqlite3_close_v2(g_cfg_db);
    return 0;
}