#include <string.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <thread>

#include <stdio.h>

#include "flow_control.h"
#include "trade_api.h"
#include "trade_spi.h"
#include "trade_inf.h"
#include "connect_mgr.h"
#include "package.h"

long long g_instrument_cnt = 0;

struct event_base *g_base = 0;
std::thread net_thread;


FILE *g_log_fp = 0;

int g_base_run = 0;

void log_msg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_fp, fmt, args);
    fflush(g_log_fp);

    va_end(args);
}

void net_worker(struct event_base *base)
{
    log_msg("net_worker start\n");
    event_base_dispatch(base);
}

char g_version_buffer[64] = {"sandbox"};

#ifdef _MSC_VER
#include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
    {
        int a = CTP_VER / 1000000;
        int b = (CTP_VER / 1000) % 1000;
        int c = CTP_VER % 1000;
        sprintf(g_version_buffer, "v%d.%d.%d-sandbox", a, b, c);

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        g_log_fp = fopen("trade_api.log", "a");
        // VS Windows平台固定调用，初始化临界区锁
        evthread_use_windows_threads();
        g_base = event_base_new();
		log_msg("DllMain start event_base_new\n");
//        net_thread.detach();
    }
		break;
	case DLL_PROCESS_DETACH:
    {
        struct timeval tv = {0, 0};
        event_base_loopexit(g_base, &tv);
    }
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	default:
		break;
	}
	return TRUE;
}
#else
__attribute__((constructor)) void trade_api_onstart(void)
{
    int a = CTP_VER / 1000000;
    int b = (CTP_VER / 1000) % 1000;
    int c = CTP_VER % 1000;
    sprintf(g_version_buffer, "v%d.%d.%d-sandbox", a, b, c);

	evthread_use_pthreads();
	g_base = event_base_new();
    // net_thread = std::thread(net_worker, g_base);
    // net_thread.detach();    
}

__attribute__((destructor)) void trade_api_onstop(void)
{
    struct timeval tv = {0, 0};
    event_base_loopexit(g_base, &tv);

}

#endif


struct TradeHelperData
{
    unsigned int TradingDay;
    char TradingDayStr[16];
    struct event_base *base;
    MultiConnCtx * netCtx;
    CThostFtdcTraderSpi * spi;

    TradeApi_Package package; //接收数据缓存
    int package_len; //接收数据长度

    TradeApi_Param param[8];

    CThostFtdcRspAuthenticateField RspAuthenticateField;
    int auth_requestID;

    FlowControl_Data flow_control;
};


void mgr_on_connected(MultiConnCtx * ctx, TradeHelperData *data)
{
    data->package_len = 0;
    evutil_socket_t fd = bufferevent_getfd(ctx->success_item->bev);
    struct sockaddr_in peer;
    socklen_t sl = sizeof(peer);
    getpeername(fd, (struct sockaddr*)&peer, &sl);
    char ip[INET_ADDRSTRLEN];
    evutil_inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    data->package_len = 0;
    data->package.header.total_len = 0;
    printf("active server: %s:%d\n", ip, ntohs(peer.sin_port));
    data->spi->OnFrontConnected();

}

void mgr_on_disconnect(MultiConnCtx * ctx, TradeHelperData *data)
{
    evutil_socket_t fd = bufferevent_getfd(ctx->success_item->bev);
    struct sockaddr_in peer;
    socklen_t sl = sizeof(peer);
    getpeername(fd, (struct sockaddr*)&peer, &sl);
    char ip[INET_ADDRSTRLEN];
    evutil_inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    printf("disconenct from server: %s:%d\n", ip, ntohs(peer.sin_port));
    data->package_len = 0;
    data->package.header.total_len = 0;
    data->spi->OnFrontDisconnected(4097);

}

void mgr_on_connect_fail(MultiConnCtx * ctx,TradeHelperData *data)
{
    printf("all targets connect failed, wait retry...\n");
    data->package_len = 0;
    data->package.header.total_len = 0;
    data->spi->OnFrontDisconnected(4097);
}

static CThostFtdcRspInfoField l_rsp_info = {0};

void (*funcArr[])(TradeHelperData * data,TradeSpi_CallbackInfo *callback) = 
{
    [](TradeHelperData * data,TradeSpi_CallbackInfo *callback) {(data->spi->*callback->func.func_0)(); },
    [](TradeHelperData * data,TradeSpi_CallbackInfo *callback) {(data->spi->*callback->func.func_1)(data->param[0].ptr);},
    [](TradeHelperData * data,TradeSpi_CallbackInfo *callback) { (data->spi->*callback->func.func_2)(data->param[0].ptr, data->param[1].ptr);},
    [](TradeHelperData * data,TradeSpi_CallbackInfo *callback) {
        int request_id = *((int *)data->param[2].ptr);
        (data->spi->*callback->func.func_3)(data->param[0].ptr, data->param[1].ptr, request_id);
    },
    [](TradeHelperData * data,TradeSpi_CallbackInfo *callback) {
        int request_id = *((int *)data->param[2].ptr);
        short flag = *((short *)data->param[3].ptr);
        bool is_last = flag != 0;
        (data->spi->*callback->func.func_4)(data->param[0].ptr, data->param[1].ptr, request_id, is_last);
    }
};

#if WIN32
    #define do_Sleep(ms) Sleep(ms)
#else
    #define do_Sleep(ms) usleep((ms)*1000)
#endif

CThostFtdcInstrumentField g_instrument_field = {0};

int mgr_data_read_package(struct bufferevent *bev, TradeHelperData * data)
{
    int read_len;
    struct evbuffer *in_buf = bufferevent_get_input(bev);
    size_t readable = evbuffer_get_length(in_buf);
    if( readable <= 0 )
    {
        printf("mgr_data_read_cb error 01\n");
        return 0;
    }

    if( (readable + data->package_len) < TradeApi_Header_Size )
    {
        read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, readable);
        data->package_len += read_len;
        return 0;
    }

    if( data->package_len < TradeApi_Header_Size )
    {
        //先读取头信息
        read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, TradeApi_Header_Size - data->package_len);
        data->package_len += read_len;
        readable -= read_len;
        if( data->package_len < TradeApi_Header_Size )
        {
            printf("mgr_data_read_cb error 01\n");
            return readable; //这里一定出错了
        }
    }

    if( data->package.header.total_len > TradeApi_Buffer )
    {
        printf("package_len: %d, total_len: %d\n", data->package_len, data->package.header.total_len);
        if( (readable + data->package_len) < data->package.header.total_len )
        {
            evbuffer_drain(in_buf, readable);
            data->package_len += readable;
            return 0;
        }
        read_len = evbuffer_drain(in_buf, data->package.header.total_len - data->package_len);
        data->package_len = 0;
        readable -= read_len;
        return readable;
    }

    //正常数据，直接读取
    if( (readable + data->package_len) < data->package.header.total_len )
    {
        read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, readable);
        data->package_len += read_len;
        //printf("data->package_len: %d ,total_len =%d\n", data->package_len, data->package.header.total_len);
        return 0;
    }

    //可以获取到完整数据，处理数据
    read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, data->package.header.total_len - data->package_len);
    readable -= read_len;
    //已经读取完整数据，处理数据 ,data->package.header.total_len 为数据长度
    data->package_len = 0;
    data->package.data[data->package.header.total_len] = 0;

    if( data->package.header.func_id >= Spi_CallbackCount )
    {
        printf("mgr_data_read_cb error data->package.header.func_id = %d\n",data->package.header.func_id);
        return readable; //这里一定出错了
    }
    if( data->package.header.p_cnt > 8 )
    {
        printf("mgr_data_read_cb error data->package.header.p_cnt = %d\n", data->package.header.p_cnt );
        return readable; //这里一定出错了
    }

    TradeSpi_CallbackInfo * callback = &g_spi_callback_list[data->package.header.func_id];

    if( callback->p_cnt != data->package.header.p_cnt )
    {
        log_msg("<== %s 不支持 01\n",callback->name);
        return readable; //参数数量不匹配，这里一定出错了
    }
    if( !callback->func.func_0 )
    {
        log_msg("<== %s 不支持 02\n",callback->name);
        return readable; //函数为0,可能是新旧版本差异导致
    }

    //解析返回数据，调用回调函数
    //目前最多只有4个参数

    int offset = TradeApi_Header_Size;
    char * ptr = data->package.data + offset;
    for(int i=0; i< data->package.header.p_cnt; i++)
    {
        unsigned short param_len = *((unsigned short *)ptr);
        offset += sizeof(unsigned short);
        if( (param_len + offset) > data->package.header.total_len )
        {
            return readable; //这里一定出错了
        }
        if( param_len > 0 && callback->psize[i] > param_len )
        {
            return readable; //参数长度不匹配，这里一定出错了
        }

        ptr += sizeof(unsigned short);

        data->param[i].len = param_len;
        data->param[i].ptr = ptr;
        if( param_len == 0 )
            data->param[i].ptr = nullptr;
        
        offset += param_len;
        ptr += param_len;
    }

    if( data->package.header.p_cnt == 4 )
    {
        if( data->param[3].ptr && *((short *)data->param[3].ptr) )
        {
            log_msg("<== %s finish\n",callback->name);
        }
    }
    else
    {
        log_msg("<== %s func_id = %d\n",callback->name,data->package.header.func_id);
    }
    //调用回调函数
    funcArr[callback->p_cnt](data, callback);
    return readable;
}

void mgr_data_read_cb(struct bufferevent *bev, MultiConnCtx * ctx)
{  
    TradeHelperData *data = (TradeHelperData *)ctx->user_arg;
    do
    {
        if( mgr_data_read_package(bev, data) <= 0 )
            break;
    }while(1);
}

TraderApiHelper::TraderApiHelper()
{
	log_msg("TraderApiHelper::TraderApiHelper ...\n");
    m_data = new TradeHelperData;
    m_data->base = g_base;

    m_data->netCtx = multi_conn_create(m_data->base);

    m_data->package_len = 0;

    Init_FlowControl(&m_data->flow_control);
    Set_FlowControl(&m_data->flow_control,1000,1);

    multi_conn_set_callback(
        m_data->netCtx,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_connected,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_disconnect,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_connect_fail, 
        m_data);

    multi_conn_set_read_cb(m_data->netCtx, (DataReadCb)mgr_data_read_cb);
	log_msg("TraderApiHelper::TraderApiHelper finish\n");
}

TraderApiHelper::~TraderApiHelper()
{
    multi_conn_destroy(m_data->netCtx);

    m_data->spi = NULL;
    m_data->netCtx = NULL;
    m_data->base = NULL;

    delete m_data;
}

#if CTP_VER >= 6007011
ctp_helper_trade_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath, bool bIsProductionMode )
{
    return new TraderApiHelper;
}
#else
ctp_helper_trade_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath )
{
	log_msg("TraderApiHelper::CreateFtdcTraderApi ...\n");
    return new TraderApiHelper;
}

#endif
	///获取API的版本信息
	///@retrun 获取到的版本号
ctp_helper_trade_API const char *CThostFtdcTraderApi::GetApiVersion()
{
    return g_version_buffer;
}

	///删除接口对象本身
	///@remark 不再使用本接口对象时,调用该函数删除接口对象
void TraderApiHelper::Release()
{
    delete this;
}

	
	///初始化
	///@remark 初始化运行环境,只有调用后,接口才开始工作
void TraderApiHelper::Init()
{
	log_msg("TraderApiHelper::Init ...\n");
    if( g_base_run == 0 )
    {
		log_msg("TraderApiHelper::Init start thread\n");
        g_base_run = 1;
        net_thread = std::thread(net_worker, g_base);
        net_thread.detach();
    }
    //初始化网络
    //初始化其他信息
    multi_conn_start(m_data->netCtx);
	log_msg("TraderApiHelper::Init finish\n");
}
	
	///等待接口线程结束运行
	///@return 线程退出代码
int TraderApiHelper::Join() 
{
    return 0;
}
	
	///获取当前交易日
	///@retrun 获取到的交易日
	///@remark 只有登录成功后,才能得到正确的交易日
const char * TraderApiHelper::GetTradingDay()
{
    return m_data->TradingDayStr;
}

#if CTP_VER >= 6007010	
void TraderApiHelper::GetFrontInfo(CThostFtdcFrontInfoField* pFrontInfo)
{
}
#endif

	///注册前置机网络地址
	///@param pszFrontAddress：前置机网络地址。
	///@remark 网络地址的格式为：“protocol://ipaddress:port”，如：”tcp://127.0.0.1:17001”。 
	///@remark “tcp”代表传输协议，“127.0.0.1”代表服务器地址。”17001”代表服务器端口号。
void TraderApiHelper::RegisterFront(char *pszFrontAddress) 
{
	log_msg("TraderApiHelper::RegisterFront %s\n",pszFrontAddress);
    register_front(m_data->netCtx,pszFrontAddress);
}
	
	///注册名字服务器网络地址
	///@param pszNsAddress：名字服务器网络地址。
	///@remark 网络地址的格式为：“protocol://ipaddress:port”，如：”tcp://127.0.0.1:12001”。 
	///@remark “tcp”代表传输协议，“127.0.0.1”代表服务器地址。”12001”代表服务器端口号。
	///@remark RegisterNameServer优先于RegisterFront
void TraderApiHelper::RegisterNameServer(char *pszNsAddress)
{
    //暂不实现
    return;
}
	
	///注册名字服务器用户信息
	///@param pFensUserInfo：用户信息。
void TraderApiHelper::RegisterFensUserInfo(CThostFtdcFensUserInfoField * pFensUserInfo)
{
    //暂不实现
	log_msg("TraderApiHelper::RegisterFensUserInfo %s\n",pFensUserInfo);
//	register_front(m_data->netCtx,pFensUserInfo);
    return ;
}
	
	///注册回调接口
	///@param pSpi 派生自回调接口类的实例
void TraderApiHelper::RegisterSpi(CThostFtdcTraderSpi *pSpi) 
{
	log_msg("TraderApiHelper::RegisterSpi \n");
    m_data->spi = pSpi;
}


	///订阅私有流。
	///@param nResumeType 私有流重传方式  
	///        THOST_TERT_RESTART:从本交易日开始重传
	///        THOST_TERT_RESUME:从上次收到的续传
	///        THOST_TERT_QUICK:只传送登录后私有流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到私有流的数据。

#if CTP_VER >= 6007013
void TraderApiHelper::SubscribePrivateTopic(THOST_TE_RESUME_TYPE nResumeType,int nSeqNo)
{
    TradeApi_Package package;
    short val = nResumeType;
    Init_TradeApi_Package(&package,Api_SubscribePrivateTopic);
    Append_TradeApi_Package_Val(package,val);
    Append_TradeApi_Package_Val(package,nSeqNo);
    //发送数据
    send_data(m_data->netCtx,package.data,package.header.total_len);
}
#else
void TraderApiHelper::SubscribePrivateTopic(THOST_TE_RESUME_TYPE nResumeType)
{
    TradeApi_Package package;
    short val = nResumeType;
    Init_TradeApi_Package(&package,Api_SubscribePrivateTopic);
    Append_TradeApi_Package_Val(package,val);

    //发送数据
    send_data(m_data->netCtx,package.data,package.header.total_len);
}
#endif

	///订阅公共流。
	///@param nResumeType 公共流重传方式  
	///        THOST_TERT_RESTART:从本交易日开始重传
	///        THOST_TERT_RESUME:从上次收到的续传
	///        THOST_TERT_QUICK:只传送登录后公共流的内容
	///        THOST_TERT_NONE:取消订阅公共流
	///@remark 该方法要在Init方法前调用。若不调用则不会收到公共流的数据。
void TraderApiHelper::SubscribePublicTopic(THOST_TE_RESUME_TYPE nResumeType)
{
    TradeApi_Package package;
    short val = nResumeType;
    Init_TradeApi_Package(&package,Api_SubscribePublicTopic);
    Append_TradeApi_Package_Val(package,val);
    //发送数据
    send_data(m_data->netCtx,package.data,package.header.total_len);
}

#define ApiHelper_ReqQry(func,Field) int TraderApiHelper::func(Field *pReqField, int nRequestID) \
{\
    if( Check_FlowControl(&m_data->flow_control) <= 0 )\
        return -1;\
    TradeApi_CallFuncRet(func)\
}\

#define ApiHelper_ReqQryIns(func,Field) int TraderApiHelper::func(Field *pReqField, int nRequestID) \
{\
    if( Check_FlowControl(&m_data->flow_control) <= 0 )\
        return -1;\
    g_instrument_cnt = 0;\
    log_msg("qry ins reqid = %d\n",nRequestID);\
    TradeApi_CallFuncRet(func)\
}\


int TraderApiHelper::ReqAuthenticate(CThostFtdcReqAuthenticateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqAuthenticate) //该函数发起一个

}

	///注册用户终端信息，用于中继服务器多连接模式
	///需要在终端认证成功后，用户登录前调用该接口
int TraderApiHelper::RegisterUserSystemInfo(CThostFtdcUserSystemInfoField *pReqField) 
{
    TradeApi_CallFuncSimpleRet(RegisterUserSystemInfo)
}

	///上报用户终端信息，用于中继服务器操作员登录模式
	///操作员登录后，可以多次调用该接口上报客户信息
int TraderApiHelper::SubmitUserSystemInfo(CThostFtdcUserSystemInfoField *pReqField) 
{
    TradeApi_CallFuncSimpleRet(SubmitUserSystemInfo)
}

#if CTP_VER >= 6007010
///注册用户终端信息，用于中继服务器多连接模式.用于微信小程序等应用上报信息.
int TraderApiHelper::RegisterWechatUserSystemInfo(CThostFtdcWechatUserSystemInfoField *pReqField) 
{
    TradeApi_CallFuncSimpleRet(RegisterWechatUserSystemInfo)
}
///上报用户终端信息，用于中继服务器操作员登录模式.用于微信小程序等应用上报信息.
int TraderApiHelper::SubmitWechatUserSystemInfo(CThostFtdcWechatUserSystemInfoField *pReqField) 
{
    TradeApi_CallFuncSimpleRet(SubmitWechatUserSystemInfo)
}
#endif
	///用户登录请求
int TraderApiHelper::ReqUserLogin(CThostFtdcReqUserLoginField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserLogin)
}

	///登出请求
int TraderApiHelper::ReqUserLogout(CThostFtdcUserLogoutField *pReqField, int nRequestID)
{
    TradeApi_CallFuncRet(ReqUserLogout)
}

	///用户口令更新请求
int TraderApiHelper::ReqUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserPasswordUpdate)
}

	///资金账户口令更新请求
int TraderApiHelper::ReqTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqTradingAccountPasswordUpdate)
}

	///查询用户当前支持的认证模式
int TraderApiHelper::ReqUserAuthMethod(CThostFtdcReqUserAuthMethodField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserAuthMethod)
}

	///用户发出获取图形验证码请求
int TraderApiHelper::ReqGenUserCaptcha(CThostFtdcReqGenUserCaptchaField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqGenUserCaptcha)
}

	///用户发出获取短信验证码请求
int TraderApiHelper::ReqGenUserText(CThostFtdcReqGenUserTextField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqGenUserText)
}

	///用户发出带有图片验证码的登陆请求
int TraderApiHelper::ReqUserLoginWithCaptcha(CThostFtdcReqUserLoginWithCaptchaField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserLoginWithCaptcha)
}

	///用户发出带有短信验证码的登陆请求
int TraderApiHelper::ReqUserLoginWithText(CThostFtdcReqUserLoginWithTextField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserLoginWithText)
}

	///用户发出带有动态口令的登陆请求
int TraderApiHelper::ReqUserLoginWithOTP(CThostFtdcReqUserLoginWithOTPField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqUserLoginWithOTP)
}

	///报单录入请求
int TraderApiHelper::ReqOrderInsert(CThostFtdcInputOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqOrderInsert)
}

	///预埋单录入请求
int TraderApiHelper::ReqParkedOrderInsert(CThostFtdcParkedOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqParkedOrderInsert)
}

	///预埋撤单录入请求
int TraderApiHelper::ReqParkedOrderAction(CThostFtdcParkedOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqParkedOrderAction)
}


	///报单操作请求
int TraderApiHelper::ReqOrderAction(CThostFtdcInputOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqOrderAction)
}

	///查询最大报单数量请求

ApiHelper_ReqQry(ReqQryMaxOrderVolume,CThostFtdcQryMaxOrderVolumeField)

	///投资者结算结果确认
int TraderApiHelper::ReqSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqSettlementInfoConfirm)
}

	///请求删除预埋单
int TraderApiHelper::ReqRemoveParkedOrder(CThostFtdcRemoveParkedOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqRemoveParkedOrder)
}

	///请求删除预埋撤单
int TraderApiHelper::ReqRemoveParkedOrderAction(CThostFtdcRemoveParkedOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqRemoveParkedOrderAction)
}

	///执行宣告录入请求
int TraderApiHelper::ReqExecOrderInsert(CThostFtdcInputExecOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqExecOrderInsert)
}

	///执行宣告操作请求
int TraderApiHelper::ReqExecOrderAction(CThostFtdcInputExecOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqExecOrderAction)
}

	///询价录入请求
int TraderApiHelper::ReqForQuoteInsert(CThostFtdcInputForQuoteField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqForQuoteInsert)
}

	///报价录入请求
int TraderApiHelper::ReqQuoteInsert(CThostFtdcInputQuoteField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQuoteInsert)
}

	///报价操作请求
int TraderApiHelper::ReqQuoteAction(CThostFtdcInputQuoteActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQuoteAction)
}

	///批量报单操作请求
int TraderApiHelper::ReqBatchOrderAction(CThostFtdcInputBatchOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqBatchOrderAction)
}

	///期权自对冲录入请求
int TraderApiHelper::ReqOptionSelfCloseInsert(CThostFtdcInputOptionSelfCloseField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqOptionSelfCloseInsert)
}

	///期权自对冲操作请求
int TraderApiHelper::ReqOptionSelfCloseAction(CThostFtdcInputOptionSelfCloseActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqOptionSelfCloseAction)
}


	///申请组合录入请求
int TraderApiHelper::ReqCombActionInsert(CThostFtdcInputCombActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqCombActionInsert)
}

	///请求查询报单

ApiHelper_ReqQry(ReqQryOrder,CThostFtdcQryOrderField)

	///请求查询成交

ApiHelper_ReqQry(ReqQryTrade,CThostFtdcQryTradeField)


	///请求查询投资者持仓
ApiHelper_ReqQry(ReqQryInvestorPosition,CThostFtdcQryInvestorPositionField)

	///请求查询资金账户
ApiHelper_ReqQry(ReqQryTradingAccount,CThostFtdcQryTradingAccountField)    


	///请求查询投资者
ApiHelper_ReqQry(ReqQryInvestor,CThostFtdcQryInvestorField)

	///请求查询交易编码
ApiHelper_ReqQry(ReqQryTradingCode,CThostFtdcQryTradingCodeField)

	///请求查询合约保证金率

ApiHelper_ReqQry(ReqQryInstrumentMarginRate,CThostFtdcQryInstrumentMarginRateField)

	///请求查询合约手续费率
ApiHelper_ReqQry(ReqQryInstrumentCommissionRate,CThostFtdcQryInstrumentCommissionRateField)    

#if CTP_VER >= 6007011
ApiHelper_ReqQry(ReqQryUserSession,CThostFtdcQryUserSessionField)
#endif
	///请求查询交易所

ApiHelper_ReqQry(ReqQryExchange,CThostFtdcQryExchangeField)

//请求查询产品
ApiHelper_ReqQry(ReqQryProduct,CThostFtdcQryProductField)

///请求查询合约
ApiHelper_ReqQryIns(ReqQryInstrument,CThostFtdcQryInstrumentField)

///请求查询行情
ApiHelper_ReqQry(ReqQryDepthMarketData,CThostFtdcQryDepthMarketDataField)

#if CTP_VER >= 6007010
ApiHelper_ReqQry(ReqQryTraderOffer,CThostFtdcQryTraderOfferField)
#endif
///请求查询投资者结算结果
ApiHelper_ReqQry(ReqQrySettlementInfo,CThostFtdcQrySettlementInfoField)

//请求查询转帐银行
ApiHelper_ReqQry(ReqQryTransferBank,CThostFtdcQryTransferBankField)

///请求查询投资者持仓明细
ApiHelper_ReqQry(ReqQryInvestorPositionDetail,CThostFtdcQryInvestorPositionDetailField)

///请求查询客户通知
ApiHelper_ReqQry(ReqQryNotice,CThostFtdcQryNoticeField)

//请求查询结算信息确认
ApiHelper_ReqQry(ReqQrySettlementInfoConfirm,CThostFtdcQrySettlementInfoConfirmField)

//请求查询投资者持仓明细
ApiHelper_ReqQry(ReqQryInvestorPositionCombineDetail,CThostFtdcQryInvestorPositionCombineDetailField)

//请求查询保证金监管系统经纪公司资金账户密钥
ApiHelper_ReqQry(ReqQryCFMMCTradingAccountKey,CThostFtdcQryCFMMCTradingAccountKeyField)

//请求查询仓单折抵信息
ApiHelper_ReqQry(ReqQryEWarrantOffset,CThostFtdcQryEWarrantOffsetField)

///请求查询投资者品种/跨品种保证金
ApiHelper_ReqQry(ReqQryInvestorProductGroupMargin,CThostFtdcQryInvestorProductGroupMarginField)


//请求查询交易所保证金率
ApiHelper_ReqQry(ReqQryExchangeMarginRate,CThostFtdcQryExchangeMarginRateField)

//请求查询交易所调整保证金率
ApiHelper_ReqQry(ReqQryExchangeMarginRateAdjust,CThostFtdcQryExchangeMarginRateAdjustField)


//请求查询汇率
ApiHelper_ReqQry(ReqQryExchangeRate,CThostFtdcQryExchangeRateField)

//请求查询二级代理操作员银期权限
ApiHelper_ReqQry(ReqQryProductExchRate,CThostFtdcQryProductExchRateField)

///请求查询产品报价汇率
ApiHelper_ReqQry(ReqQrySecAgentACIDMap,CThostFtdcQrySecAgentACIDMapField)

	///请求查询产品组
ApiHelper_ReqQry(ReqQryProductGroup,CThostFtdcQryProductGroupField)

///请求查询做市商合约手续费率
ApiHelper_ReqQry(ReqQryMMInstrumentCommissionRate,CThostFtdcQryMMInstrumentCommissionRateField)

///请求查询做市商期权合约手续费
ApiHelper_ReqQry(ReqQryMMOptionInstrCommRate,CThostFtdcQryMMOptionInstrCommRateField)

///请求查询报单手续费
ApiHelper_ReqQry(ReqQryInstrumentOrderCommRate,CThostFtdcQryInstrumentOrderCommRateField)

//请求查询资金账户
ApiHelper_ReqQry(ReqQrySecAgentTradingAccount,CThostFtdcQryTradingAccountField)

///请求查询二级代理商资金校验模式
ApiHelper_ReqQry(ReqQrySecAgentCheckMode,CThostFtdcQrySecAgentCheckModeField)


//请求查询二级代理商信息
ApiHelper_ReqQry(ReqQrySecAgentTradeInfo,CThostFtdcQrySecAgentTradeInfoField)


//请求查询期权交易成本
ApiHelper_ReqQry(ReqQryOptionInstrTradeCost,CThostFtdcQryOptionInstrTradeCostField)

///请求查询期权合约手续费
ApiHelper_ReqQry(ReqQryOptionInstrCommRate,CThostFtdcQryOptionInstrCommRateField)


///请求查询执行宣告
ApiHelper_ReqQry(ReqQryExecOrder,CThostFtdcQryExecOrderField)

//请求查询询价
ApiHelper_ReqQry(ReqQryForQuote,CThostFtdcQryForQuoteField)


//请求查询报价
ApiHelper_ReqQry(ReqQryQuote,CThostFtdcQryQuoteField)

//请求查询期权自对冲
ApiHelper_ReqQry(ReqQryOptionSelfClose,CThostFtdcQryOptionSelfCloseField)

//请求查询投资单元
ApiHelper_ReqQry(ReqQryInvestUnit,CThostFtdcQryInvestUnitField)

//请求查询组合合约安全系数
ApiHelper_ReqQry(ReqQryCombInstrumentGuard,CThostFtdcQryCombInstrumentGuardField)

//请求查询申请组合
ApiHelper_ReqQry(ReqQryCombAction,CThostFtdcQryCombActionField)

//请求查询转帐流水
ApiHelper_ReqQry(ReqQryTransferSerial,CThostFtdcQryTransferSerialField)

//请求查询银期签约关系
ApiHelper_ReqQry(ReqQryAccountregister,CThostFtdcQryAccountregisterField)

//请求查询签约银行
ApiHelper_ReqQry(ReqQryContractBank,CThostFtdcQryContractBankField)

//请求查询预埋单
ApiHelper_ReqQry(ReqQryParkedOrder,CThostFtdcQryParkedOrderField)

//请求查询预埋撤单
ApiHelper_ReqQry(ReqQryParkedOrderAction,CThostFtdcQryParkedOrderActionField)

//请求查询交易通知
ApiHelper_ReqQry(ReqQryTradingNotice,CThostFtdcQryTradingNoticeField)

//请求查询经纪公司交易参数
ApiHelper_ReqQry(ReqQryBrokerTradingParams,CThostFtdcQryBrokerTradingParamsField)

//请求查询经纪公司交易算法
ApiHelper_ReqQry(ReqQryBrokerTradingAlgos,CThostFtdcQryBrokerTradingAlgosField)

//请求查询监控中心用户令牌
ApiHelper_ReqQry(ReqQueryCFMMCTradingAccountToken,CThostFtdcQueryCFMMCTradingAccountTokenField)

//期货发起银行资金转期货请求
ApiHelper_ReqQry(ReqFromBankToFutureByFuture,CThostFtdcReqTransferField)

//期货发起期货资金转银行请求
ApiHelper_ReqQry(ReqFromFutureToBankByFuture,CThostFtdcReqTransferField)

//期货发起查询银行余额请求
ApiHelper_ReqQry(ReqQueryBankAccountMoneyByFuture,CThostFtdcReqQueryAccountField)

//请求查询分类合约
ApiHelper_ReqQry(ReqQryClassifiedInstrument,CThostFtdcQryClassifiedInstrumentField)

//请求组合优惠比例
ApiHelper_ReqQry(ReqQryCombPromotionParam,CThostFtdcQryCombPromotionParamField)

///投资者风险结算持仓查询
ApiHelper_ReqQry(ReqQryRiskSettleInvstPosition,CThostFtdcQryRiskSettleInvstPositionField)

//风险结算产品查询
ApiHelper_ReqQry(ReqQryRiskSettleProductStatus,CThostFtdcQryRiskSettleProductStatusField)


#if CTP_VER >= 6007010

///SPBM期货合约参数查询
ApiHelper_ReqQry(ReqQrySPBMFutureParameter,CThostFtdcQrySPBMFutureParameterField)

//SPBM期权合约参数查询
ApiHelper_ReqQry(ReqQrySPBMOptionParameter,CThostFtdcQrySPBMOptionParameterField)

//SPBM品种内对锁仓折扣参数查询
ApiHelper_ReqQry(ReqQrySPBMIntraParameter,CThostFtdcQrySPBMIntraParameterField)

//SPBM跨品种抵扣参数查询
ApiHelper_ReqQry(ReqQrySPBMInterParameter,CThostFtdcQrySPBMInterParameterField)

//SPBM组合保证金套餐查询
ApiHelper_ReqQry(ReqQrySPBMPortfDefinition,CThostFtdcQrySPBMPortfDefinitionField)

//投资者SPBM套餐选择查询
ApiHelper_ReqQry(ReqQrySPBMInvestorPortfDef,CThostFtdcQrySPBMInvestorPortfDefField)

//投资者新型组合保证金系数查询
ApiHelper_ReqQry(ReqQryInvestorPortfMarginRatio,CThostFtdcQryInvestorPortfMarginRatioField)

//投资者产品SPBM明细查询
ApiHelper_ReqQry(ReqQryInvestorProdSPBMDetail,CThostFtdcQryInvestorProdSPBMDetailField)

//投资者商品组SPMM记录查询
ApiHelper_ReqQry(ReqQryInvestorCommoditySPMMMargin,CThostFtdcQryInvestorCommoditySPMMMarginField)

//投资者商品群SPMM记录查询
ApiHelper_ReqQry(ReqQryInvestorCommodityGroupSPMMMargin,CThostFtdcQryInvestorCommodityGroupSPMMMarginField)

//SPMM合约参数查询
ApiHelper_ReqQry(ReqQrySPMMInstParam,CThostFtdcQrySPMMInstParamField)

//SPMM产品参数查询
ApiHelper_ReqQry(ReqQrySPMMProductParam,CThostFtdcQrySPMMProductParamField)

//SPBM附加跨品种抵扣参数查询
ApiHelper_ReqQry(ReqQrySPBMAddOnInterParameter,CThostFtdcQrySPBMAddOnInterParameterField)

//RCAMS产品组合信息查询
ApiHelper_ReqQry(ReqQryRCAMSCombProductInfo,CThostFtdcQryRCAMSCombProductInfoField)

//RCAMS同合约风险对冲参数查询
ApiHelper_ReqQry(ReqQryRCAMSInstrParameter,CThostFtdcQryRCAMSInstrParameterField)

//RCAMS品种内风险对冲参数查询
ApiHelper_ReqQry(ReqQryRCAMSIntraParameter,CThostFtdcQryRCAMSIntraParameterField)

//RCAMS跨品种风险折抵参数查询
ApiHelper_ReqQry(ReqQryRCAMSInterParameter,CThostFtdcQryRCAMSInterParameterField)

//RCAMS空头期权风险调整参数查询
ApiHelper_ReqQry(ReqQryRCAMSShortOptAdjustParam,CThostFtdcQryRCAMSShortOptAdjustParamField)

//RCAMS策略组合持仓查询
ApiHelper_ReqQry(ReqQryRCAMSInvestorCombPosition,CThostFtdcQryRCAMSInvestorCombPositionField)

//投资者品种RCAMS保证金查询
ApiHelper_ReqQry(ReqQryInvestorProdRCAMSMargin,CThostFtdcQryInvestorProdRCAMSMarginField)

//RULE合约保证金参数查询
ApiHelper_ReqQry(ReqQryRULEInstrParameter,CThostFtdcQryRULEInstrParameterField)

//RULE品种内对锁仓折扣参数查询
ApiHelper_ReqQry(ReqQryRULEIntraParameter,CThostFtdcQryRULEIntraParameterField)

//RULE跨品种抵扣参数查询
ApiHelper_ReqQry(ReqQryRULEInterParameter,CThostFtdcQryRULEInterParameterField)

//投资者产品RULE保证金查询
ApiHelper_ReqQry(ReqQryInvestorProdRULEMargin,CThostFtdcQryInvestorProdRULEMarginField)

//投资者新型组合保证金开关查询
ApiHelper_ReqQry(ReqQryInvestorPortfSetting,CThostFtdcQryInvestorPortfSettingField)

//投资者申报费阶梯收取记录查询
ApiHelper_ReqQry(ReqQryInvestorInfoCommRec,CThostFtdcQryInvestorInfoCommRecField)

//组合腿信息查询
ApiHelper_ReqQry(ReqQryCombLeg,CThostFtdcQryCombLegField)

//对冲设置请求
ApiHelper_ReqQry(ReqOffsetSetting,CThostFtdcInputOffsetSettingField)

//对冲设置撤销请求
ApiHelper_ReqQry(ReqCancelOffsetSetting,CThostFtdcInputOffsetSettingField)

//投资者对冲设置查询
ApiHelper_ReqQry(ReqQryOffsetSetting,CThostFtdcQryOffsetSettingField)

#endif

#if CTP_VER >= 6007013

ApiHelper_ReqQry(ReqGenSMSCode,CThostFtdcReqGenSMSCodeField)

///套利确认请求
ApiHelper_ReqQry(ReqSpdApply,CThostFtdcInputSpdApplyField)

///套利确认撤销请求
ApiHelper_ReqQry(ReqSpdApplyAction,CThostFtdcInputSpdApplyActionField)

///套利确认查询请求
ApiHelper_ReqQry(ReqQrySpdApply,CThostFtdcQrySpdApplyField)

///套保确认请求
ApiHelper_ReqQry(ReqHedgeCfm,CThostFtdcInputHedgeCfmField)

///套保确认撤销请求
ApiHelper_ReqQry(ReqHedgeCfmAction,CThostFtdcInputHedgeCfmActionField)

///套保确认查询请求
ApiHelper_ReqQry(ReqQryHedgeCfm,CThostFtdcQryHedgeCfmField)

#endif