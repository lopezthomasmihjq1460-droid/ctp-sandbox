#include <string.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <thread>

#include <stdio.h>

#include "trade_api.h"
#include "trade_spi.h"
#include "trade_inf.h"
#include "connect_mgr.h"
#include "package.h"


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
    event_base_dispatch(base);
}


#ifdef _MSC_VER
#include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        g_log_fp = fopen("trade_api.log", "a");
        // VS Windows平台固定调用，初始化临界区锁
        evthread_use_windows_threads();
        g_base = event_base_new();

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
	evthread_use_pthreads();
	g_base = event_base_new();
    net_thread = std::thread(net_worker, g_base);
    net_thread.detach();    
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
    data->spi->OnFrontDisconnected(8192);
}

void mgr_on_connect_fail(MultiConnCtx * ctx,TradeHelperData *data)
{
    printf("all targets connect failed, wait retry...\n");
    data->spi->OnFrontDisconnected(8192);
}

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
        log_msg("<== 08.02\n");
        log_msg("<== %s 不支持\n",callback->name);
        return readable; //参数数量不匹配，这里一定出错了
    }
    if( !callback->func.func_0 )
    {
        log_msg("<== %s 不支持\n",callback->name);
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
        log_msg("<== %s\n",callback->name);
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

static void self_event_cb(int fd, short what, TradeHelperData *data)
{
    //data->spi->OnRspAuthenticateField (&data->RspAuthenticateField, nullptr, data->auth_requestID, true);
}



TraderApiHelper::TraderApiHelper()
{
    m_data = new TradeHelperData;
    m_data->base = g_base;

    m_data->netCtx = multi_conn_create(m_data->base);

    m_data->package_len = 0;

    multi_conn_set_callback(
        m_data->netCtx,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_connected,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_disconnect,
        (void (*)(MultiConnCtx * ctx, void*))mgr_on_connect_fail, 
        m_data);

    multi_conn_set_read_cb(m_data->netCtx, (DataReadCb)mgr_data_read_cb);
}

TraderApiHelper::~TraderApiHelper()
{
    multi_conn_destroy(m_data->netCtx);

    m_data->spi = NULL;
    m_data->netCtx = NULL;
    m_data->base = NULL;

    delete m_data;
}

#ifdef CTP_6_7
ctp_helper_trade_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath, bool bIsProductionMode )
{
    return new TraderApiHelper;
}
#else

ctp_helper_trade_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath )
{
    return new TraderApiHelper;
}

#endif
	///获取API的版本信息
	///@retrun 获取到的版本号
ctp_helper_trade_API const char *CThostFtdcTraderApi::GetApiVersion()
{
    return "6.7.11-sandbox";
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
    if( g_base_run == 0 )
    {
        g_base_run = 1;
        net_thread = std::thread(net_worker, g_base);
        net_thread.detach();
    }
    //初始化网络
    //初始化其他信息
    multi_conn_start(m_data->netCtx);
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

#ifdef CTP_6_7	
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
    return ;
}
	
	///注册回调接口
	///@param pSpi 派生自回调接口类的实例
void TraderApiHelper::RegisterSpi(CThostFtdcTraderSpi *pSpi) 
{
    m_data->spi = pSpi;
}


//传输结构定义：
/*
{
    len : 4 bytes bit,本消息总长度  (int) //包括自己和后面数据的总长度
    func: 2 bytes bitAPI 函数编号   (unsigned short)
    pn: : 2 bytes 参数个数          (unsigned short)
    params:[
        {
            len: 2 bytes 参数长度
            data: n bytes
        }
    ]
}
*/



	///订阅私有流。
	///@param nResumeType 私有流重传方式  
	///        THOST_TERT_RESTART:从本交易日开始重传
	///        THOST_TERT_RESUME:从上次收到的续传
	///        THOST_TERT_QUICK:只传送登录后私有流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到私有流的数据。

void TraderApiHelper::SubscribePrivateTopic(THOST_TE_RESUME_TYPE nResumeType)
{
    TradeApi_Package package;
    short val = nResumeType;
    Init_TradeApi_Package(&package,Api_SubscribePrivateTopic);
    Append_TradeApi_Package_Val(package,val);

    //发送数据
    send_data(m_data->netCtx,package.data,package.header.total_len);
}
	
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


	///客户端认证请求
/*
	///经纪公司代码
	TThostFtdcBrokerIDType	BrokerID;
	///用户代码
	TThostFtdcUserIDType	UserID;
	///用户端产品信息
	TThostFtdcProductInfoType	UserProductInfo;
	///App代码
	TThostFtdcAppIDType	AppID;
	///App类型
	TThostFtdcAppTypeType	AppType;
-----------------------------------------------------
	TThostFtdcBrokerIDType	BrokerID;
	///用户代码
	TThostFtdcUserIDType	UserID;
	///用户端产品信息
	TThostFtdcProductInfoType	UserProductInfo;
	///认证码
	TThostFtdcAuthCodeType	AuthCode;
	///App代码
	TThostFtdcAppIDType	AppID;

*/

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

#ifdef CTP_6_7	
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
int TraderApiHelper::ReqQryMaxOrderVolume(CThostFtdcQryMaxOrderVolumeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryMaxOrderVolume)
}

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
int TraderApiHelper::ReqQryOrder(CThostFtdcQryOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryOrder)
}

	///请求查询成交
int TraderApiHelper::ReqQryTrade(CThostFtdcQryTradeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTrade)
}

	///请求查询投资者持仓
int TraderApiHelper::ReqQryInvestorPosition(CThostFtdcQryInvestorPositionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorPosition)
}

	///请求查询资金账户
int TraderApiHelper::ReqQryTradingAccount(CThostFtdcQryTradingAccountField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTradingAccount)
}

	///请求查询投资者
int TraderApiHelper::ReqQryInvestor(CThostFtdcQryInvestorField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestor)
}

	///请求查询交易编码
int TraderApiHelper::ReqQryTradingCode(CThostFtdcQryTradingCodeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTradingCode)
}

	///请求查询合约保证金率
int TraderApiHelper::ReqQryInstrumentMarginRate(CThostFtdcQryInstrumentMarginRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInstrumentMarginRate)
}

	///请求查询合约手续费率
int TraderApiHelper::ReqQryInstrumentCommissionRate(CThostFtdcQryInstrumentCommissionRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInstrumentCommissionRate)
}

#ifdef CTP_6_7	
int TraderApiHelper::ReqQryUserSession(CThostFtdcQryUserSessionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryUserSession)
}
#endif
	///请求查询交易所
int TraderApiHelper::ReqQryExchange(CThostFtdcQryExchangeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryExchange)
}

	///请求查询产品
int TraderApiHelper::ReqQryProduct(CThostFtdcQryProductField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryProduct)
}

	///请求查询合约
int TraderApiHelper::ReqQryInstrument(CThostFtdcQryInstrumentField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInstrument)
}

	///请求查询行情
int TraderApiHelper::ReqQryDepthMarketData(CThostFtdcQryDepthMarketDataField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryDepthMarketData)
}
#ifdef CTP_6_7
int TraderApiHelper::ReqQryTraderOffer(CThostFtdcQryTraderOfferField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTraderOffer)
}
#endif
	///请求查询投资者结算结果
int TraderApiHelper::ReqQrySettlementInfo(CThostFtdcQrySettlementInfoField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySettlementInfo)
}

	///请求查询转帐银行
int TraderApiHelper::ReqQryTransferBank(CThostFtdcQryTransferBankField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTransferBank)
}

	///请求查询投资者持仓明细
int TraderApiHelper::ReqQryInvestorPositionDetail(CThostFtdcQryInvestorPositionDetailField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorPositionDetail)
}

	///请求查询客户通知
int TraderApiHelper::ReqQryNotice(CThostFtdcQryNoticeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryNotice)
}

	///请求查询结算信息确认
int TraderApiHelper::ReqQrySettlementInfoConfirm(CThostFtdcQrySettlementInfoConfirmField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySettlementInfoConfirm)
}

	///请求查询投资者持仓明细
int TraderApiHelper::ReqQryInvestorPositionCombineDetail(CThostFtdcQryInvestorPositionCombineDetailField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorPositionCombineDetail)
}

	///请求查询保证金监管系统经纪公司资金账户密钥
int TraderApiHelper::ReqQryCFMMCTradingAccountKey(CThostFtdcQryCFMMCTradingAccountKeyField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryCFMMCTradingAccountKey)
}

	///请求查询仓单折抵信息
int TraderApiHelper::ReqQryEWarrantOffset(CThostFtdcQryEWarrantOffsetField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryEWarrantOffset)
}

	///请求查询投资者品种/跨品种保证金
int TraderApiHelper::ReqQryInvestorProductGroupMargin(CThostFtdcQryInvestorProductGroupMarginField *pReqField, int nRequestID)
{
    TradeApi_CallFuncRet(ReqQryInvestorProductGroupMargin)
}

	///请求查询交易所保证金率
int TraderApiHelper::ReqQryExchangeMarginRate(CThostFtdcQryExchangeMarginRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryExchangeMarginRate)
}

	///请求查询交易所调整保证金率
int TraderApiHelper::ReqQryExchangeMarginRateAdjust(CThostFtdcQryExchangeMarginRateAdjustField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryExchangeMarginRateAdjust)
}

	///请求查询汇率
int TraderApiHelper::ReqQryExchangeRate(CThostFtdcQryExchangeRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryExchangeRate)
}

	///请求查询二级代理操作员银期权限
int TraderApiHelper::ReqQrySecAgentACIDMap(CThostFtdcQrySecAgentACIDMapField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySecAgentACIDMap)
}

	///请求查询产品报价汇率
int TraderApiHelper::ReqQryProductExchRate(CThostFtdcQryProductExchRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryProductExchRate)
}

	///请求查询产品组
int TraderApiHelper::ReqQryProductGroup(CThostFtdcQryProductGroupField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryProductGroup)
}

	///请求查询做市商合约手续费率
int TraderApiHelper::ReqQryMMInstrumentCommissionRate(CThostFtdcQryMMInstrumentCommissionRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryMMInstrumentCommissionRate)
}

	///请求查询做市商期权合约手续费
int TraderApiHelper::ReqQryMMOptionInstrCommRate(CThostFtdcQryMMOptionInstrCommRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryMMOptionInstrCommRate)
}

	///请求查询报单手续费
int TraderApiHelper::ReqQryInstrumentOrderCommRate(CThostFtdcQryInstrumentOrderCommRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInstrumentOrderCommRate)
}

	///请求查询资金账户
int TraderApiHelper::ReqQrySecAgentTradingAccount(CThostFtdcQryTradingAccountField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySecAgentTradingAccount)
}

	///请求查询二级代理商资金校验模式
int TraderApiHelper::ReqQrySecAgentCheckMode(CThostFtdcQrySecAgentCheckModeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySecAgentCheckMode)
}

	///请求查询二级代理商信息
int TraderApiHelper::ReqQrySecAgentTradeInfo(CThostFtdcQrySecAgentTradeInfoField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySecAgentTradeInfo)   
}

	///请求查询期权交易成本
int TraderApiHelper::ReqQryOptionInstrTradeCost(CThostFtdcQryOptionInstrTradeCostField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryOptionInstrTradeCost)
}

	///请求查询期权合约手续费
int TraderApiHelper::ReqQryOptionInstrCommRate(CThostFtdcQryOptionInstrCommRateField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryOptionInstrCommRate)
}

	///请求查询执行宣告
int TraderApiHelper::ReqQryExecOrder(CThostFtdcQryExecOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryExecOrder)
}

	///请求查询询价
int TraderApiHelper::ReqQryForQuote(CThostFtdcQryForQuoteField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryForQuote)
}

	///请求查询报价
int TraderApiHelper::ReqQryQuote(CThostFtdcQryQuoteField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryQuote)
}

	///请求查询期权自对冲
int TraderApiHelper::ReqQryOptionSelfClose(CThostFtdcQryOptionSelfCloseField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryOptionSelfClose)
}

	///请求查询投资单元
int TraderApiHelper::ReqQryInvestUnit(CThostFtdcQryInvestUnitField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestUnit)
}

	///请求查询组合合约安全系数
int TraderApiHelper::ReqQryCombInstrumentGuard(CThostFtdcQryCombInstrumentGuardField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryCombInstrumentGuard)
}

	///请求查询申请组合
int TraderApiHelper::ReqQryCombAction(CThostFtdcQryCombActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryCombAction)
}

	///请求查询转帐流水
int TraderApiHelper::ReqQryTransferSerial(CThostFtdcQryTransferSerialField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTransferSerial)
}

	///请求查询银期签约关系
int TraderApiHelper::ReqQryAccountregister(CThostFtdcQryAccountregisterField *pReqField, int nRequestID) 
{  
    TradeApi_CallFuncRet(ReqQryAccountregister)
}

	///请求查询签约银行
int TraderApiHelper::ReqQryContractBank(CThostFtdcQryContractBankField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryContractBank)
}

	///请求查询预埋单
int TraderApiHelper::ReqQryParkedOrder(CThostFtdcQryParkedOrderField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryParkedOrder)
}

	///请求查询预埋撤单
int TraderApiHelper::ReqQryParkedOrderAction(CThostFtdcQryParkedOrderActionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryParkedOrderAction)
}

	///请求查询交易通知
int TraderApiHelper::ReqQryTradingNotice(CThostFtdcQryTradingNoticeField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryTradingNotice)
}

	///请求查询经纪公司交易参数
int TraderApiHelper::ReqQryBrokerTradingParams(CThostFtdcQryBrokerTradingParamsField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryBrokerTradingParams)
}

	///请求查询经纪公司交易算法
int TraderApiHelper::ReqQryBrokerTradingAlgos(CThostFtdcQryBrokerTradingAlgosField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryBrokerTradingAlgos)
}

	///请求查询监控中心用户令牌
int TraderApiHelper::ReqQueryCFMMCTradingAccountToken(CThostFtdcQueryCFMMCTradingAccountTokenField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQueryCFMMCTradingAccountToken)
}

	///期货发起银行资金转期货请求
int TraderApiHelper::ReqFromBankToFutureByFuture(CThostFtdcReqTransferField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqFromBankToFutureByFuture)
}

	///期货发起期货资金转银行请求
int TraderApiHelper::ReqFromFutureToBankByFuture(CThostFtdcReqTransferField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqFromFutureToBankByFuture)
}

	///期货发起查询银行余额请求
int TraderApiHelper::ReqQueryBankAccountMoneyByFuture(CThostFtdcReqQueryAccountField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQueryBankAccountMoneyByFuture)
}

	///请求查询分类合约
int TraderApiHelper::ReqQryClassifiedInstrument(CThostFtdcQryClassifiedInstrumentField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryClassifiedInstrument)
}

	///请求组合优惠比例
int TraderApiHelper::ReqQryCombPromotionParam(CThostFtdcQryCombPromotionParamField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryCombPromotionParam)
}

	///投资者风险结算持仓查询
int TraderApiHelper::ReqQryRiskSettleInvstPosition(CThostFtdcQryRiskSettleInvstPositionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRiskSettleInvstPosition)
}

	///风险结算产品查询
int TraderApiHelper::ReqQryRiskSettleProductStatus(CThostFtdcQryRiskSettleProductStatusField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRiskSettleProductStatus)
}

#ifdef CTP_6_7

///SPBM期货合约参数查询
int TraderApiHelper::ReqQrySPBMFutureParameter(CThostFtdcQrySPBMFutureParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMFutureParameter)
}

	///SPBM期权合约参数查询
int TraderApiHelper::ReqQrySPBMOptionParameter(CThostFtdcQrySPBMOptionParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMOptionParameter)
}

	///SPBM品种内对锁仓折扣参数查询
int TraderApiHelper::ReqQrySPBMIntraParameter(CThostFtdcQrySPBMIntraParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMIntraParameter)
}

	///SPBM跨品种抵扣参数查询
int TraderApiHelper::ReqQrySPBMInterParameter(CThostFtdcQrySPBMInterParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMInterParameter)
}

	///SPBM组合保证金套餐查询
int TraderApiHelper::ReqQrySPBMPortfDefinition(CThostFtdcQrySPBMPortfDefinitionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMPortfDefinition)
}

	///投资者SPBM套餐选择查询
int TraderApiHelper::ReqQrySPBMInvestorPortfDef(CThostFtdcQrySPBMInvestorPortfDefField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMInvestorPortfDef)
}

	///投资者新型组合保证金系数查询
int TraderApiHelper::ReqQryInvestorPortfMarginRatio(CThostFtdcQryInvestorPortfMarginRatioField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorPortfMarginRatio)
}

	///投资者产品SPBM明细查询
int TraderApiHelper::ReqQryInvestorProdSPBMDetail(CThostFtdcQryInvestorProdSPBMDetailField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorProdSPBMDetail)
}

	///投资者商品组SPMM记录查询
int TraderApiHelper::ReqQryInvestorCommoditySPMMMargin(CThostFtdcQryInvestorCommoditySPMMMarginField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorCommoditySPMMMargin)
}

	///投资者商品群SPMM记录查询
int TraderApiHelper::ReqQryInvestorCommodityGroupSPMMMargin(CThostFtdcQryInvestorCommodityGroupSPMMMarginField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorCommodityGroupSPMMMargin)
}

	///SPMM合约参数查询
int TraderApiHelper::ReqQrySPMMInstParam(CThostFtdcQrySPMMInstParamField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPMMInstParam)
}

	///SPMM产品参数查询
int TraderApiHelper::ReqQrySPMMProductParam(CThostFtdcQrySPMMProductParamField *pReqField, int nRequestID)  
{
    TradeApi_CallFuncRet(ReqQrySPMMProductParam)
}

	///SPBM附加跨品种抵扣参数查询
int TraderApiHelper::ReqQrySPBMAddOnInterParameter(CThostFtdcQrySPBMAddOnInterParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQrySPBMAddOnInterParameter)
}

	///RCAMS产品组合信息查询
int TraderApiHelper::ReqQryRCAMSCombProductInfo(CThostFtdcQryRCAMSCombProductInfoField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSCombProductInfo)
}

	///RCAMS同合约风险对冲参数查询
int TraderApiHelper::ReqQryRCAMSInstrParameter(CThostFtdcQryRCAMSInstrParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSInstrParameter)
}

	///RCAMS品种内风险对冲参数查询
int TraderApiHelper::ReqQryRCAMSIntraParameter(CThostFtdcQryRCAMSIntraParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSIntraParameter)
}

	///RCAMS跨品种风险折抵参数查询
int TraderApiHelper::ReqQryRCAMSInterParameter(CThostFtdcQryRCAMSInterParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSInterParameter)
}

	///RCAMS空头期权风险调整参数查询
int TraderApiHelper::ReqQryRCAMSShortOptAdjustParam(CThostFtdcQryRCAMSShortOptAdjustParamField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSShortOptAdjustParam)
}

	///RCAMS策略组合持仓查询
int TraderApiHelper::ReqQryRCAMSInvestorCombPosition(CThostFtdcQryRCAMSInvestorCombPositionField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRCAMSInvestorCombPosition)
}

	///投资者品种RCAMS保证金查询
int TraderApiHelper::ReqQryInvestorProdRCAMSMargin(CThostFtdcQryInvestorProdRCAMSMarginField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorProdRCAMSMargin)
}

	///RULE合约保证金参数查询
int TraderApiHelper::ReqQryRULEInstrParameter(CThostFtdcQryRULEInstrParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRULEInstrParameter)
}

	///RULE品种内对锁仓折扣参数查询
int TraderApiHelper::ReqQryRULEIntraParameter(CThostFtdcQryRULEIntraParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRULEIntraParameter)
}

	///RULE跨品种抵扣参数查询
int TraderApiHelper::ReqQryRULEInterParameter(CThostFtdcQryRULEInterParameterField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryRULEInterParameter)
}

	///投资者产品RULE保证金查询
int TraderApiHelper::ReqQryInvestorProdRULEMargin(CThostFtdcQryInvestorProdRULEMarginField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorProdRULEMargin)
}

	///投资者新型组合保证金开关查询
int TraderApiHelper::ReqQryInvestorPortfSetting(CThostFtdcQryInvestorPortfSettingField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorPortfSetting)
}

	///投资者申报费阶梯收取记录查询
int TraderApiHelper::ReqQryInvestorInfoCommRec(CThostFtdcQryInvestorInfoCommRecField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryInvestorInfoCommRec)
}

	///组合腿信息查询
int TraderApiHelper::ReqQryCombLeg(CThostFtdcQryCombLegField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryCombLeg)
}

	///对冲设置请求
int TraderApiHelper::ReqOffsetSetting(CThostFtdcInputOffsetSettingField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqOffsetSetting)
}

	///对冲设置撤销请求
int TraderApiHelper::ReqCancelOffsetSetting(CThostFtdcInputOffsetSettingField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqCancelOffsetSetting)
}

	///投资者对冲设置查询
int TraderApiHelper::ReqQryOffsetSetting(CThostFtdcQryOffsetSettingField *pReqField, int nRequestID) 
{
    TradeApi_CallFuncRet(ReqQryOffsetSetting)
}

#endif