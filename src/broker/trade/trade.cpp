#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <signal.h>

#ifdef WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
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


std::string g_app_id = "test";
std::string g_auth_code = "test";

ServerConfig g_cfg = {0};


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



// ===================== JSON配置加载 =====================
static int load_config_from_db(const char *dbfile)
{
    int nRet = sqlite3_open_v2(dbfile, &g_cfg_db,SQLITE_OPEN_READWRITE ,NULL);
    if (nRet != SQLITE_OK)
	{
		printf("配置数据库 %s 打开失败,ret = %d\n",dbfile,nRet);
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

    TradeSession * session = new TradeSession();
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
static void cmd_read_cb(struct bufferevent *bev, void *arg)
{
    char buf[256] = {0};
    size_t n = bufferevent_read(bev, buf, sizeof(buf)-1);
    if (n <= 0) return;

    // 匹配shutdown指令
    if (strstr(buf, SHUTDOWN_CMD) != NULL) {
        // 给命令行返回提示
        struct evbuffer *out = bufferevent_get_output(bev);
        evbuffer_add_printf(out, "Receive shutdown command, server exiting...\n");
        bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);
        graceful_shutdown();
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

void net_worker(void * arg)
{
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
    event_base_dispatch(g_base);
    printf("dispatch exited\n");
    event_free(sig_int);
    event_free(sig_term);    
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

    if (load_config_from_db("ctp_sandbox.db") != 0) return 1;
    printf("Load config: %s:%d log:%s daemon:%s\n",g_cfg.listen_ip, g_cfg.listen_port, LOG_FILE, run_daemon ? "yes" : "no");
    //加载ctp的dll
    g_ctp_mgr = new TradeMgr ;
    if( !g_ctp_mgr->Init() )
    {
        delete g_ctp_mgr;
        g_ctp_mgr = 0;
        printf("trade mgr init failed\n");
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
    return 0;
}