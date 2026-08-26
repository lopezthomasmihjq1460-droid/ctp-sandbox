#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include <string.h>

#include "ThostFtdcTraderApi.h"
#include "trade_session.h"
#include "trade_session_spi.h"

#include "trade_mgr.h"
#include "trade_inf.h"

void log_msg(const char *fmt, ...);

extern sqlite3 *  g_cfg_db ;
const char * user_sql = "select broker_id,account,pwd02,flowctrl,flowperiod,product_ctrl from t_account where id = ? and pwd01 = ?";

const char * product_sql = "select product_id from t_user_product where user_id = ? and product_id = ?";


const char * l_order_dir[] = 
{
    "买",
    "卖"
};

const char * l_order_flag[] = 
{
    "开",
    "平"
};

extern bool g_bIsClose;    
extern bool g_bIsPause ;    

extern long long g_alert_order ;
extern long long g_alert_cancel ;



void gbk_printf(const char *fmt, ...);

std::unordered_map<std::string, InstrumentInfo> g_instrument_map;
unsigned int g_instrument_tradingday = 0;
unsigned int g_instrument_day = 0;

TradeSession::TradeSession(bool blsProductionMode)
{
	has_started = 0;
	action_on_connected = 0; //默认是认证
	m_api = g_ctp_mgr->CreateApi(blsProductionMode);
    m_data.api = m_api;
    SubscribePrivateTopic_flag = THOST_TERT_NONE;
    SubscribePublicTopic_flag  = THOST_TERT_NONE;

    if( m_api )
    {
        m_spi = new TradeSessionSpi(this);
        m_api->RegisterSpi(m_spi);
    }

    stmt_product_permission = 0;
	if( g_cfg_db )
	{
		sqlite3_prepare_v2(g_cfg_db, product_sql, -1, &stmt_product_permission, 0);
	}
    m_data.package_len = 0;
    day.tradingDay = 0;
    day.actionDay = 0;
}

static CThostFtdcTraderSpi spi_empty;

TradeSession::~TradeSession()
{
	if( m_api )
	{
		m_api->RegisterSpi(&spi_empty);

        if( !has_started )
        {
            m_api->Init();
        }
        printf("m_api release ...\n");
		m_api->Release();
        printf("m_api release ok\n");
		m_api = 0;
	}

    m_data.api = 0;
	delete (TradeSessionSpi*)m_spi;
	m_spi = 0;

    if( stmt_product_permission )
        sqlite3_finalize(stmt_product_permission);

}


int TradeSession::Start(const char * front_addr)
{
	if( !m_api )
		return 0;

    if( has_started )
		return 0;
	has_started = 1;

	m_api->RegisterFront((char *)front_addr);
    if( SubscribePrivateTopic_flag != THOST_TERT_NONE )
	    m_api->SubscribePrivateTopic(SubscribePrivateTopic_flag);
    if( SubscribePublicTopic_flag != THOST_TERT_NONE )
	    m_api->SubscribePublicTopic(SubscribePublicTopic_flag); 

    printf("m_api init ...\n");
    log_msg("m_api init ...\n");
	m_api->Init();
	return 1;
}


void (*funcArr[2][3])(TradeSession * session,TradeApi_FunctionInfo *info) = 
{
    {
        [](TradeSession * session,TradeApi_FunctionInfo *info) {(session->m_api->*info->func_0)(); },
        [](TradeSession * session,TradeApi_FunctionInfo *info) { (session->m_api->*info->func_1)(session->m_data.param[0].ptr); },
        [](TradeSession * session,TradeApi_FunctionInfo *info) { 
            int request_id = *((int *)session->m_data.param[1].ptr);
            (session->m_api->*info->func_2)(session->m_data.param[0].ptr, request_id); 
        },   
    },
    {
        [](TradeSession * session,TradeApi_FunctionInfo *info) {(session->*info->check_v)(); },
        [](TradeSession * session,TradeApi_FunctionInfo *info) { (session->*info->check_v1)(session->m_data.param[0].ptr); },
        [](TradeSession * session,TradeApi_FunctionInfo *info) { 
            int request_id = *((int *)session->m_data.param[1].ptr);
            (session->*info->check_2)(session->m_data.param[0].ptr, request_id); 
        }
    }
};


int TradeSession::ReadPackage(struct evbuffer *in_buf,PackageData * data)
{
    int read_len;
    size_t readable = evbuffer_get_length(in_buf);
    do
    {
        if( readable <= 0 )
            break;
        if( (readable + data->package_len) < TradeApi_Header_Size )
        {
            read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, readable);
            data->package_len += read_len;
            readable -= read_len;
            break;
        }

        if( data->package_len < TradeApi_Header_Size )
        {
            //先读取头信息
            read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, TradeApi_Header_Size - data->package_len);
            data->package_len += read_len;
            readable -= read_len;
            if( data->package_len < TradeApi_Header_Size )
                break; //这里一定出错了
            
        }
        if( data->package.header.total_len > TradeApi_Buffer )
        {
            printf("package_len: %d, total_len: %d\n", data->package_len, data->package.header.total_len);
            if( (readable + data->package_len) < data->package.header.total_len )
            {
                evbuffer_drain(in_buf, readable);
                data->package_len += readable;
                readable = 0;
                break;
            }
            read_len = evbuffer_drain(in_buf, data->package.header.total_len - data->package_len);
            data->package_len = 0;
            readable -= read_len;
            break ;
        }

        //正常数据，直接读取
        if( (readable + data->package_len) < data->package.header.total_len )
        {
            read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, readable);
            data->package_len += read_len;
            readable = 0;
            break;
        }

        //可以获取到完整数据，处理数据
        read_len = evbuffer_remove(in_buf, data->package.data + data->package_len, data->package.header.total_len - data->package_len);

        //已经读取完整数据，处理数据 ,data->package.header.total_len 为数据长度
        data->package_len = 0;
        readable -= read_len;

        if( data->package.header.func_id >= Api_Count )
        {
            printf("===> data->package.header.func_id =%d !!!!!\n",data->package.header.func_id);
            break; //这里一定出错了
        }

        TradeApi_FunctionInfo * callback = &api_function_list[data->package.header.func_id];
        if( data->package.header.p_cnt > 8 )
        {
            printf("===> %s , p_cnt = %d !!!!!\n",callback->name,data->package.header.p_cnt);
            break; //这里一定出错了
        }


        if( callback->p_cnt != data->package.header.p_cnt )
        {
            printf("===> %s , p_cnt = %d !!!!!\n",callback->name,data->package.header.p_cnt);
            break; //参数数量不匹配，这里一定出错了
        }

        //解析返回数据，调用回调函数
        //目前最多只有4个参数

        int offset = TradeApi_Header_Size;
        char * ptr = data->package.data + offset;

        unsigned short p0_size = 0;

        if( data->package.header.p_cnt > 0 )
        {
            p0_size = *((unsigned short *)ptr);
             offset += sizeof(unsigned short);
            if( (p0_size + offset) > data->package.header.total_len )
            {
                return readable; //这里一定出错了
            }
            ptr += sizeof(unsigned short);
            if( p0_size == 0 )
            {
                data->param[0].ptr = nullptr;
                data->param[0].len = 0;
            }
            else if( p0_size < callback->psize[0] )
            {
                memset(buffer_ext ,0,callback->psize[0]);
                memcpy(buffer_ext,ptr,p0_size);
                memset(buffer_ext + p0_size,0,callback->psize[0] - p0_size);
                data->param[0].len = callback->psize[0];
                data->param[0].ptr = buffer_ext;

                //printf("%s need size=%d ,recv size = %d\n",callback->name,callback->psize[0],p0_size);
                // return readable;
            }
            else
            {
                data->param[0].len = p0_size;
                data->param[0].ptr = ptr;       
            }
            offset += p0_size;
            ptr += p0_size;            
        }
    
        for(int i=1; i< data->package.header.p_cnt; i++)
        {
            unsigned short param_len = *((unsigned short *)ptr);
            offset += sizeof(unsigned short);
            if( (param_len + offset) > data->package.header.total_len )
            {
                printf("2 %s need size=%d ,recv size = %d\n",callback->name,callback->psize[0],param_len);
                return readable; //这里一定出错了
            }
            ptr += sizeof(unsigned short);

            if( param_len > 0 && callback->psize[i] != param_len )
            {
                printf("3 %s need size=%d ,recv size = %d\n",callback->name,callback->psize[0],param_len);
                return readable; //参数长度不匹配，这里一定出错了
            }

            if( param_len == 0 )
                data->param[i].ptr = nullptr;
            else
            {
                data->param[i].len = param_len;
                data->param[i].ptr = ptr;
            }
            
            offset += param_len;
            ptr += param_len;
        }

        //处理BrokerID,UserID,InvestorID参数映射

        if( callback->BrokerID >= 0 )
        {
            Req.broker = data->param[0].ptr + callback->BrokerID;
            if( data->param[0].ptr[callback->BrokerID] != 0 )
                strncpy(data->param[0].ptr + callback->BrokerID,broker.broker_id.c_str(),sizeof(TThostFtdcBrokerIDType) - 1);
        }
        else
        {
            Req.broker = "";
        }
        if( callback->UserID >= 0 )
        {
            Req.user = data->param[0].ptr + callback->UserID;
            if( data->param[0].ptr[callback->UserID] != 0 )
                strncpy(data->param[0].ptr + callback->UserID,broker.account.c_str(),sizeof(TThostFtdcUserIDType) - 1);
        }
        else
        {
            Req.user = "";
        }
        if( callback->InvestorID >= 0 )
        {
            Req.investor = data->param[0].ptr + callback->InvestorID;
            if( data->param[0].ptr[callback->InvestorID] != 0 )
                strncpy(data->param[0].ptr + callback->InvestorID,broker.account.c_str(),sizeof(TThostFtdcInvestorIDType) - 1);
        }
        else
        {
            Req.investor = "";
        }
        if( callback->AccountID >= 0 )
        {
            Req.account = data->param[0].ptr + callback->AccountID;
            if( data->param[0].ptr[callback->AccountID] != 0 )
                strncpy(data->param[0].ptr + callback->AccountID,broker.account.c_str(),sizeof(TThostFtdcAccountIDType) -1);
        }
        else
        {
            Req.account = "";
        }
        if( data->package.header.func_id == Api_ReqQryInstrument )
        {
            Req.is_user_qry_instrument = 1;
            printf("is_user_qry_instrument = 1\n");
        }
        if( Api_ReqQryTradingAccount != data->package.header.func_id )
        {
            // printf("===>Field Len=%3d,need =%3d , %s,investor=%s,account=%s,user=%s,broker=%s\n", p0_size,callback->psize[0],callback->name,Req.investor.c_str(),Req.account.c_str(),Req.user.c_str(),Req.broker.c_str());
            // log_msg("===>Field Len=%3d,need =%3d , %s,investor=%s,account=%s,user=%s,broker=%s\n", p0_size,callback->psize[0],callback->name,Req.investor.c_str(),Req.account.c_str(),Req.user.c_str(),Req.broker.c_str());
            printf("===>Field Len=%3d,need =%3d , %s\n", p0_size,callback->psize[0],callback->name);
            log_msg("===>Field Len=%3d,need =%3d , %s\n", p0_size,callback->psize[0],callback->name);
        }

        if( g_bIsClose )
        {
            printf("===>Field Len=%3d,need =%3d , %s,账户当前被禁用\n", p0_size,callback->psize[0],callback->name); 
            log_msg("===>Field Len=%3d,need =%3d , %s,账户当前被禁用\n", p0_size,callback->psize[0],callback->name); 
        }
        else
            funcArr[callback->is_precheck][callback->p_cnt](this,callback);
    }while(0);
    return readable;
}

void TradeSession::CloseClient()
{
    // int fd = bufferevent_getfd(net.bev);    
    // bufferevent_setfd(net.bev, -1);
    // evutil_closesocket(fd);

    bufferevent_setcb(net.bev, NULL, NULL, NULL, NULL); 
    // 可选：清空读写缓冲区，释放缓冲区内存
    bufferevent_disable(net.bev, EV_READ|EV_WRITE);
    // 释放bufferevent，BEV_OPT_CLOSE_ON_FREE 会自动 close socket fd
    bufferevent_free(net.bev);
    net.bev = 0;

    delete this;

/**
   Returns the file descriptor associated with a bufferevent, or -1 if
   no file descriptor is associated with the bufferevent.
 */
}

void TradeSession::OnClientData()
{
	PackageData * data = &m_data;
    struct evbuffer *in_buf = bufferevent_get_input(net.netCtx);

    while( 1 )
    {
        if( ReadPackage(in_buf,data) <= 0 )
            break;
    }
}

void TradeSession::SubscribePrivateTopic(THOST_TE_RESUME_TYPE nResumeType)
{
    SubscribePrivateTopic_flag = (THOST_TE_RESUME_TYPE)*((short *)m_data.param[0].ptr);
}

void TradeSession::SubscribePublicTopic(THOST_TE_RESUME_TYPE nResumeType)
{
    SubscribePublicTopic_flag = (THOST_TE_RESUME_TYPE)*((short *)m_data.param[0].ptr);
}

int TradeSession::ReqAuthenticate(CThostFtdcReqAuthenticateField *pReqField, int request_id) 
{
    requestId = request_id;
    CThostFtdcRspAuthenticateField RspField;
    CThostFtdcRspInfoField RspInfo;

    RspInfo.ErrorID = 0;
    strncpy(RspInfo.ErrorMsg,"Authenticated successfully",sizeof(RspInfo.ErrorMsg)-1 );

    strncpy(RspField.BrokerID,pReqField->BrokerID,sizeof(RspField.BrokerID) -1);
    strncpy(RspField.UserID,pReqField->UserID,sizeof(RspField.UserID) - 1);
    strncpy(RspField.UserProductInfo,pReqField->UserProductInfo,sizeof(RspField.UserProductInfo) -1);
    strncpy(RspField.AppID,pReqField->AppID,sizeof(RspField.AppID) - 1);
    RspField.AppType = 1;
            
    ((TradeSessionSpi*)m_spi)->doAuthenticate(&RspField, &RspInfo, request_id, true) ;

    action_on_connected = 1;
    return 1;
}

int TradeSession::ReqUserLogin(CThostFtdcReqUserLoginField *pReqField, int request_id)
{
    if( g_bIsClose)
    {
        CThostFtdcRspUserLoginField RspField = {0};
        CThostFtdcRspInfoField RspInfo;

        RspInfo.ErrorID = 48;
        strncpy(RspInfo.ErrorMsg,"账户被禁用",sizeof(RspInfo.ErrorMsg) -1);

        strncpy(RspField.BrokerID,pReqField->BrokerID,sizeof(RspField.BrokerID) - 1);
        strncpy(RspField.UserID,pReqField->UserID,sizeof(RspField.UserID) - 1);
        
        m_spi->OnRspUserLogin(&RspField, &RspInfo, request_id, true) ;
        return 1;  
    }
    sqlite3_stmt *stmt = 0;
	if( g_cfg_db )
	{
		sqlite3_prepare_v2(g_cfg_db, user_sql, -1, &stmt, 0);
	}

    const char * broker_id = nullptr;
    const char * account = nullptr;
    const char * pwd02 = nullptr;
    int flowctrl = 0;
    int flowperiod = 0;
    int product_ctrl = 0;

    char front_addr_buf[128] = {0};
    const char * front_addr = nullptr;

	do
	{
        if( !stmt )
            break;
        sqlite3_bind_text(stmt, 1, pReqField->UserID, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pReqField->Password, -1, SQLITE_STATIC);
        int nRet = sqlite3_step(stmt);

		if( nRet != SQLITE_ROW )
			break;
	
		broker_id = (const char *)sqlite3_column_text(stmt,0);
		account = (const char *)sqlite3_column_text(stmt,1);
		pwd02 = (const char *)sqlite3_column_text(stmt,2);
		flowctrl = sqlite3_column_int(stmt,3);
		flowperiod = sqlite3_column_int(stmt,4);
		product_ctrl = sqlite3_column_int(stmt,5);
	}while(0);    

    if( broker_id )
        front_addr = g_ctp_mgr->GetFrontAddr(broker_id,front_addr_buf);

    if( !front_addr || !account || !pwd02 )
    {
        CThostFtdcRspUserLoginField RspField = {0};
        CThostFtdcRspInfoField RspInfo;

        RspInfo.ErrorID = 48;
        strncpy(RspInfo.ErrorMsg,"broker not find",sizeof(RspInfo.ErrorMsg) -1);

        strncpy(RspField.BrokerID,pReqField->BrokerID,sizeof(RspField.BrokerID) - 1);
        strncpy(RspField.UserID,pReqField->UserID,sizeof(RspField.UserID) - 1);
        
        m_spi->OnRspUserLogin(&RspField, &RspInfo, request_id, true) ;
        return 1;
    }

    broker.raw_user = pReqField->UserID;
    broker.raw_broker = pReqField->BrokerID;
    broker.broker_id = broker_id;
    broker.account = account;
    broker.pwd02 = pwd02;
    broker.flowctrl = flowctrl;
    broker.flowperiod = flowperiod;
    broker.product_ctrl = product_ctrl;

    if( broker.flowperiod > 0 )
    {
        Init_FlowControl(&flow_control);
        Set_FlowControl(&flow_control,broker.flowperiod,broker.flowctrl); //每秒10笔报撤单
    }

    loginRequestId = request_id;
    loginField = *pReqField;
    
    strncpy(loginField.BrokerID,broker_id,sizeof(loginField.BrokerID) -1);
    strncpy(loginField.UserID,account,sizeof(loginField.UserID) - 1);
    strncpy(loginField.Password,pwd02,sizeof(loginField.Password) - 1);
    strncpy(loginField.UserProductInfo,"TradeSandbox",sizeof(loginField.UserProductInfo) -1);
    sqlite3_reset( stmt );

    //避免错误
    memset(loginField.MacAddress,0,sizeof(loginField.MacAddress));
    memset(loginField.ClientIPAddress,0,sizeof(loginField.ClientIPAddress));
    memset(loginField.LoginRemark,0,sizeof(loginField.LoginRemark));
    loginField.ClientIPPort = 0;
    
    //首先需要通过认证
    do
    {
        if( has_started )
            break;
        Start(front_addr);
        return 1;
    }while(0);

    m_api->ReqUserLogin(&loginField, request_id);
    return 1;
}

const unsigned char id_textTab[] =
{
//            0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   a,   b,   c,   d,   e,   f,
// /*00*/    00,  00,  02,  03,  04,  05,  06,  07,  08,  \t,  \n,  0b,  0c,  \r,  0e,  0f,
   /*00*/     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
// /*10*/    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  1a,  1b,  1c,  1d,  1e,  1f,
   /*10*/     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
// /*20*/      ,   !,   ",   #,   $,   %,   &,   ',   (,   ),   *,   +,   ,,   -,   .,   /,
   /*20*/     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
// /*30*/     0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   :,   ;,   <,   =,   >,   ?,
   /*30*/     1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,
// /*40*/     @,   A,   B,   C,   D,   E,   F,   G,   H,   I,   J,   K,   L,   M,   N,   O,
   /*40*/     0,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
// /*50*/     P,   Q,   R,   S,   T,   U,   V,   W,   X,   Y,   Z,   [,   \,   ],   ^,   _,
   /*50*/     2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   0,   0,   0,   0,   4,
// /*60*/     `,   a,   b,   c,   d,   e,   f,   g,   h,   i,   j,   k,   l,   m,   n,   o,
   /*60*/     0,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
// /*70*/     p,   q,   r,   s,   t,   u,   v,   w,   x,   y,   z,   {,   |,   },   ~,  7f,
   /*70*/     3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   0,   0,   0,   0,   0,
};

const char * product_id_by_instrument(const unsigned char * instrument_id,char product_id_buffer[32])
{
    int i = 0;
    product_id_buffer[0] = 0;
    for(i=0; i< 30 ; i++)
    {
        if( instrument_id[i] > 0x7f)
            break;
        if( id_textTab[instrument_id[i]] < 2 )
            break;
        product_id_buffer[i] = instrument_id[i];
    }
    product_id_buffer[i] = '\0';
    return product_id_buffer;
}

bool TradeSession::CheckProductPermission(const char * product_id)
{
    if( broker.product_ctrl == 0 )
        return true; //不限制

    if( !stmt_product_permission )
        return false; //拒绝

    sqlite3_bind_text(stmt_product_permission, 1, broker.raw_user.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_product_permission, 2, product_id, -1, SQLITE_STATIC);

    int ret = sqlite3_step(stmt_product_permission);
    printf("CheckProductPermission db ret = %d\n",ret);
    
    log_msg("CheckProductPermission db ret = %d\n",ret);

    sqlite3_reset(stmt_product_permission);
    if( ret != SQLITE_ROW )
    {
        if( broker.product_ctrl == 1 )
        {
            //白名单
            return false; //限制
        }
        //黑名单为空
        return true;
    }

    if( broker.product_ctrl == 1 )
    {
        //白名单，找到了
        return true; //
    }
    //黑名单,找到了
    return false;
}

bool TradeSession::CheckSelfDeal(const char * instrument_id,long long price,int dir)
{
    //此处需要优化，包括价格类型，套保套利，标准套利等情况，还有order巨大情况的性能优化
    for (SessionTable::iterator iter = selfdeal_map.begin(); iter != selfdeal_map.end(); ++iter)
    {
        // iter->first  = SessionKey
        // iter->second = value字符串
        const SessionKey& key = iter->first;
        if( key.dir == dir )
            continue; //方向相同，不用管

        if( key.ins != instrument_id )
            continue; //合约不同，不用管

        const OrderInfo& data = iter->second;
        if( dir == THOST_FTDC_D_Buy )
        {
            if( price >= data.price )
                return false; //价格交叉，自成交
        }
        else 
        {
            if( price <= data.price )
                return false; //价格交叉，自成交
        }
    }    
    return true;
}


int TradeSession::ReqUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, int nRequestID)
{
    CThostFtdcRspInfoField RspInfo = {0};
    RspInfo.ErrorID = 406;
    do
    {
	    if( !g_cfg_db )
            break;
        sqlite3_stmt *stmt_update_pwd = 0;
		sqlite3_prepare_v2(g_cfg_db, "update t_account set pwd01 = ? where id=? and pwd01=?",  -1, &stmt_update_pwd, 0);
        if( !stmt_update_pwd )
            break;
        sqlite3_bind_text(stmt_update_pwd, 1, pUserPasswordUpdate->NewPassword, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update_pwd, 2, pUserPasswordUpdate->UserID, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update_pwd, 3, pUserPasswordUpdate->OldPassword, -1, SQLITE_STATIC);

        int ret = sqlite3_step(stmt_update_pwd);
        int changed = sqlite3_changes(g_cfg_db);
        sqlite3_finalize(stmt_update_pwd);

        if( changed != 1 )
            break;
        RspInfo.ErrorID = 0;
        sqlite3_exec(g_cfg_db,"commit;",0,0,0);

    }while(0);
    
    m_spi->OnRspUserPasswordUpdate(pUserPasswordUpdate, &RspInfo, nRequestID, true) ;
    return 0;
}

int TradeSession::ReqTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pTradingAccountPasswordUpdate, int nRequestID) 
{
    CThostFtdcTradingAccountPasswordUpdateField RspField = {0};
    CThostFtdcRspInfoField RspInfo = {0};
    RspInfo.ErrorID = 501;
    snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"%s","不支持的操作");
    m_spi->OnRspTradingAccountPasswordUpdate(pTradingAccountPasswordUpdate, &RspInfo, nRequestID, true) ;
    return 0;
}


int TradeSession::ReqOrderInsert(CThostFtdcInputOrderField *pInputOrder, int nRequestID)
{
    if( g_bIsPause || g_bIsClose)
    {
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 404;
        snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"账户暂停交易权限",pInputOrder->InstrumentID);
        log_msg("%s 报单失败 账户暂停交易权限\n",pInputOrder->InstrumentID);
        gbk_printf("%s 报单失败 账户暂停交易权限\n",pInputOrder->InstrumentID);
        m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;

        return 0;
    }
    //0、检查合约是否存在，价格是否符合要求
    long long price = (pInputOrder->LimitPrice + 0.00000001)*10000; //价格转换为整数，方便比较

    do
    {
        if( g_instrument_tradingday != day.tradingDay || g_instrument_day != day.actionDay )
        {
            //需要重查合约
            CThostFtdcQryInstrumentField req = {0};
            m_api->ReqQryInstrument(&req, 0);

            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 404;
            snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"合约信息查询中,稍后重试...",pInputOrder->InstrumentID);
            log_msg("%s 报单失败 合约信息查询中,稍后重试...\n",pInputOrder->InstrumentID);
            gbk_printf("%s 报单失败 合约信息查询中,稍后重试...\n",pInputOrder->InstrumentID);
            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;

            return 0;
        }
        auto instrument = g_instrument_map.find(pInputOrder->InstrumentID);
        if( instrument == g_instrument_map.end() )
        {
            //合约不存在，拒绝该订单
            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 404;
            snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"合约 %s 不存在",pInputOrder->InstrumentID);
            log_msg("合约 %s 不存在,拒绝该订单\n",pInputOrder->InstrumentID);
            gbk_printf("合约 %s 不存在,拒绝该订单\n",pInputOrder->InstrumentID);
            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
            return 0;
        }

        if( pInputOrder->VolumeTotalOriginal > instrument->second.volume_limit )
        {
            //合约最大订单量超出，拒绝该订单
            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 405;
            snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"合约 %s 最大订单量超出限制",pInputOrder->InstrumentID);
            log_msg("合约 %s 报单量 %d  超出最大订单量 %d 限制,拒绝该订单\n",pInputOrder->InstrumentID,pInputOrder->VolumeTotalOriginal,instrument->second.volume_limit);
            gbk_printf("合约 %s 报单量 %d  超出最大订单量 %d 限制,拒绝该订单\n",pInputOrder->InstrumentID,pInputOrder->VolumeTotalOriginal,instrument->second.volume_limit);

            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
            return 0;
        }

        if( instrument->second.price_tick == 0 )
            break;
    
        if( (price % instrument->second.price_tick) != 0 )
        {
            //价格不符合要求，拒绝该订单
            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 406;
            snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"价格 %.5f 不符合要求",(pInputOrder->LimitPrice + 0.00000001));
            log_msg("%s 价格 %.5f 不符合要求,拒绝该订单\n",pInputOrder->InstrumentID,(pInputOrder->LimitPrice + 0.00000001));
            gbk_printf("%s 价格 %.5f 不符合要求,拒绝该订单\n",pInputOrder->InstrumentID,(pInputOrder->LimitPrice + 0.00000001));
            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
            return 0;
        }
    }while(0);

    //1、品种检查，是否有对应品种交易权限
    char product_id_buffer[32];
    product_id_by_instrument( (const unsigned char *)pInputOrder->InstrumentID,product_id_buffer);

    if( !CheckProductPermission(product_id_buffer) )
    {
        //没有对应品种交易权限，拒绝该订单
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 401;
        snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"无品种 %s 交易权限",product_id_buffer);
        
        log_msg("无品种 %s 交易权限,拒绝该订单\n",product_id_buffer);
        gbk_printf("无品种 %s 交易权限,拒绝该订单\n",product_id_buffer);

        m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
        return 0;
    }
    //2、自成交检查
    

    if( !CheckSelfDeal(pInputOrder->InstrumentID,price,pInputOrder->Direction) )
    {
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 402;
        snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"存在自成交风险(%s/%.4f)",pInputOrder->InstrumentID,pInputOrder->LimitPrice);
        log_msg("存在自成交风险(%s/%.4f | %s),拒绝该订单\n",pInputOrder->InstrumentID,(pInputOrder->LimitPrice + 0.00000001),l_order_dir[ (pInputOrder->Direction - '0') & 0x1]);
        gbk_printf("存在自成交风险(%s/%.4f | %s),拒绝该订单\n",pInputOrder->InstrumentID,(pInputOrder->LimitPrice + 0.00000001),l_order_dir[ (pInputOrder->Direction - '0') & 0x1]);
        m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
        return 0;
    }

    //3、流控检查
    if( broker.flowperiod > 0 )
    {
        if( Check_FlowControl(&flow_control) <= 0 )
        {
            //超出流控阈值，拒绝该订单
            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 403;
            strncpy(RspInfo.ErrorMsg,"报单频率超出限制",sizeof(RspInfo.ErrorMsg) -1);
            log_msg("报单频率超出限制,拒绝该订单 %s %d %.4f | %s\n",pInputOrder->InstrumentID,pInputOrder->VolumeTotalOriginal,(pInputOrder->LimitPrice + 0.00000001), l_order_dir[ (pInputOrder->Direction - '0') & 0x1]);
            gbk_printf("报单频率超出限制,拒绝该订单 %s %d %.4f | %s\n",pInputOrder->InstrumentID,pInputOrder->VolumeTotalOriginal,(pInputOrder->LimitPrice + 0.00000001), l_order_dir[ (pInputOrder->Direction - '0') & 0x1]);

            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
            return 0;
        }
    }
#if CTP_VER >= 6007010
    pInputOrder->SessionReqSeq = nRequestID;
#endif
    
    OrderKey key = {pInputOrder->InstrumentID,price,pInputOrder->Direction,pInputOrder->VolumeTotalOriginal,pInputOrder->CombOffsetFlag[0] == THOST_FTDC_OF_Open ? 1 :0};
    auto order = order_map.find(key);
    if( order != order_map.end() )
    {
        order->second.index++;
        log_msg("订单 %s(%lld.%04u,%s%s,%d) 重复报单总次数 %d\n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,order->second.index);
        gbk_printf("订单 %s(%lld.%04u,%s%s,%d) 重复报单总次数 %d\n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,order->second.index);
    }
    else
    {
        order_map[key] = {price,1,pInputOrder->Direction,pInputOrder->VolumeTotalOriginal};
        order = order_map.find(key);
    }

    Req.order_cnt2++;
    if( g_alert_order > 0 && Req.order_cnt2 > g_alert_order )
    {
        log_msg("!!!! 报单 %s(%lld.%04u,%s%s,%d) 次数 %d 超过预警数量 %d\n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,Req.order_cnt2,g_alert_order);
        gbk_printf("!!!! 报单 %s(%lld.%04u,%s%s,%d) 次数 %d 超过预警数量 %d\n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,Req.order_cnt2,g_alert_order);
    }
    else{
        log_msg("发起报单 %s(%lld.%04u,%s%s,%d) \n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,order->second.index);
        gbk_printf("发起报单 %s(%lld.%04u,%s%s,%d) \n",pInputOrder->InstrumentID,price/10000,price%10000,l_order_dir[ (pInputOrder->Direction - '0') & 0x1],l_order_flag[ (pInputOrder->CombOffsetFlag[0] - '0') & 0x1],pInputOrder->VolumeTotalOriginal,order->second.index);
    }

    return m_api->ReqOrderInsert(pInputOrder,nRequestID);
}

int TradeSession::ReqOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, int nRequestID)
{
    if( g_bIsPause || g_bIsClose )
    {
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 403;
        strncpy(RspInfo.ErrorMsg,"撤单||账户暂停交易权限",sizeof(RspInfo.ErrorMsg) -1);
        log_msg("撤单||账户暂停交易权限 %s ref=%s SysID=%s\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID);
        gbk_printf("撤单||账户暂停交易权限 %s ref=%s SysID=%s\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID);
        m_spi->OnRspOrderAction(pInputOrderAction, &RspInfo, nRequestID, true) ;        
        return 0;
    }

    //流控检查
    if( broker.flowperiod > 0 )
    {
        if( Check_FlowControl(&flow_control) <= 0 )
        {
            //超出流控阈值，拒绝该订单
            CThostFtdcRspInfoField RspInfo = {0};
            RspInfo.ErrorID = 403;
            strncpy(RspInfo.ErrorMsg,"撤单||频率超出限制",sizeof(RspInfo.ErrorMsg) -1);
            log_msg("撤单||频率超出限制,拒绝该操作 %s ref=%s SysID=%s\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID);
            gbk_printf("撤单||频率超出限制,拒绝该操作 %s ref=%s SysID=%s\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID);
            m_spi->OnRspOrderAction(pInputOrderAction, &RspInfo, nRequestID, true) ;
            return 0;
        }
    }
    Req.cancel_cnt2++;

    if( g_alert_cancel > 0 && Req.cancel_cnt2 > g_alert_cancel )
    {
        log_msg("!!!! 撤单 %s ref=%s SysID=%s,总撤单数 %lld 超过预警数量 %d\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID,Req.cancel_cnt2,g_alert_cancel);
        gbk_printf("!!!! 请求撤单 %s ref=%s SysID=%s,总撤单数 %lld 超过预警数量 %d\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID,Req.cancel_cnt2,g_alert_cancel);
    }
    else
    {
        log_msg("请求撤单 %s ref=%s SysID=%s,总撤单数 %lld\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID,Req.cancel_cnt2);
        gbk_printf("请求撤单 %s ref=%s SysID=%s,总撤单数 %lld\n",pInputOrderAction->InstrumentID,pInputOrderAction->OrderRef,pInputOrderAction->OrderSysID,Req.cancel_cnt2);
    }

    // long long price = (pInputOrderAction->LimitPrice + 0.00000001)*10000; //价格转换为整数，方便比较
    // OrderKey key = {pInputOrderAction->InstrumentID,price,pInputOrderAction->Direction,pInputOrderAction->VolumeTotalOriginal,pInputOrderAction->CombOffsetFlag[0] == THOST_FTDC_OF_Open ? 1 ,0};
    // order_map[key] = {price,1,pInputOrderAction->Direction,pInputOrderAction->VolumeTotalOriginal};
    //Req.cancel_cnt++;
//    log_msg("订单 %s 已撤单,撤单总次数 %d\n",pRspField->OrderSysID,Req.cancel_cnt);    

    return m_api->ReqOrderAction(pInputOrderAction,nRequestID);
}



void TradeSession::OnRtnOrder(CThostFtdcOrderField *pRspField)
{
    // if( pRspField->OrderStatus ==  THOST_FTDC_OST_Unknown)
    // {
    //     printf("TradeSession::OnRtnOrder status = %c\n",pRspField->OrderStatus);
    //     return;
    // }
    int is_finish = 0;
    //处理在途的订单
	switch(pRspField->OrderStatus) //交易已经完结
	{//如果是
	case THOST_FTDC_OST_Canceled:
    {
        is_finish = 1;
        Req.cancel_cnt++;
        // log_msg("请求撤单 %s ref=%s SysID=%s,总撤单数 %lld\n",pRspField->InstrumentID,pRspField->OrderRef,pRspField->OrderSysID,Req.cancel_cnt);
        // gbk_printf("请求撤单 %s ref=%s SysID=%s,总撤单数 %lld\n",pRspField->InstrumentID,pRspField->OrderRef,pRspField->OrderSysID,Req.cancel_cnt);

        //增加撤单统计
    }
    case THOST_FTDC_OST_PartTradedNotQueueing:
	case THOST_FTDC_OST_NoTradeNotQueueing:
	case THOST_FTDC_OST_AllTraded:
		is_finish = 1;
	default:
		break;
	}
	if( THOST_FTDC_OSS_InsertRejected == pRspField->OrderSubmitStatus )
		is_finish = 1;

    if( pRspField->OrderSysID[0] == 0 )
        return;

    SessionKey key = {pRspField->InstrumentID,pRspField->OrderSysID,pRspField->Direction};
    auto iter = selfdeal_map.find(key);
    if( is_finish )
    {
        //订单完结，更新订单状态
        if( THOST_FTDC_OST_Canceled == pRspField->OrderStatus )
        {
            if( iter != selfdeal_map.end() )
            {
                OrderKey okey = {pRspField->InstrumentID,iter->second.price,iter->second.dir,iter->second.vol,2};
                auto order = order_map.find(okey);
                if( order != order_map.end() )
                {
                    order->second.index++;
                    log_msg("订单 %s(%lld.%04u,%d,%s%s) 重复撤单总次数 %d\n",pRspField->InstrumentID,iter->second.price/10000,iter->second.price%10000,iter->second.vol,   l_order_dir[(iter->second.dir - '0') & 0x1],l_order_flag[(pRspField->CombOffsetFlag[0] - '0') & 0x1],order->second.index);
                    gbk_printf("订单 %s(%lld.%04u,%d,%s%s) 重复撤单总次数 %d\n",pRspField->InstrumentID,iter->second.price/10000,iter->second.price%10000,iter->second.vol,l_order_dir[(iter->second.dir - '0') & 0x1],l_order_flag[(pRspField->CombOffsetFlag[0] - '0') & 0x1],order->second.index);
                }
                else
                    order_map[okey] = iter->second;
            }
        }
        selfdeal_map.erase(key);
        return;
    }

    if (iter != selfdeal_map.end())
        return;

    Req.order_cnt++;
    // log_msg("当日报单总次数 %lld\n",Req.order_cnt);
    // gbk_printf("当日报单总次数 %lld\n",Req.order_cnt);

    long long price = (pRspField->LimitPrice + 0.00000001)*10000; 
    selfdeal_map[key] = {price,1,pRspField->Direction,pRspField->VolumeTotalOriginal};
}

void TradeSession::Reset()
{
    selfdeal_map.clear();
    order_map.clear();
    Req.order_cnt = 0;
    Req.cancel_cnt = 0;
    Req.order_cnt2 = 0;
    Req.cancel_cnt2 = 0;    
    day.tradingDay = 0;
    day.actionDay = 0;
    Req.is_user_qry_instrument = 0;
}