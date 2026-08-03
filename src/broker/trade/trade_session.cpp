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

extern sqlite3 *  g_cfg_db ;
const char * user_sql = "select broker_id,account,pwd02,flowctrl,flowperiod,product_ctrl from t_account where id = ? and pwd01 = ?";

const char * product_sql = "select product_id from t_user_product where user_id = ? and product_id = ?";

TradeSession::TradeSession()
{
	has_started = 0;
	action_on_connected = 0; //默认是认证
	m_api = g_ctp_mgr->CreateApi();
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
		m_api->Release();
		m_api = 0;
	}

    m_data.api = 0;
	delete m_spi;
	m_spi = 0;

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


static int g_SessionReqSeq = 1;

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
            break; //这里一定出错了
        if( data->package.header.p_cnt > 8 )
        {
            break; //这里一定出错了
        }

        TradeApi_FunctionInfo * callback = &api_function_list[data->package.header.func_id];
        printf("===> %s\n",callback->name);

        if( callback->p_cnt != data->package.header.p_cnt )
            break; //参数数量不匹配，这里一定出错了

        //解析返回数据，调用回调函数
        //目前最多只有4个参数

        int offset = TradeApi_Header_Size;
        char * ptr = data->package.data + offset;

        if( data->package.header.p_cnt > 0 )
        {
            unsigned short param_len = *((unsigned short *)ptr);
             offset += sizeof(unsigned short);
            if( (param_len + offset) > data->package.header.total_len )
            {
                return readable; //这里一定出错了
            }
            ptr += sizeof(unsigned short);
            if( param_len == 0 )
            {
                data->param[0].ptr = nullptr;
                data->param[0].len = 0;
            }
            else if( param_len < callback->psize[0] )
            {
                memset(buffer_ext ,0,callback->psize[0]);

                memcpy(buffer_ext,ptr,param_len);
                memset(buffer_ext + param_len,0,callback->psize[0] - param_len);
                data->param[0].len = callback->psize[0];
                data->param[0].ptr = buffer_ext;

                printf("%s need size=%d ,recv size = %d\n",callback->name,callback->psize[0],param_len);
                // return readable;
            }
            else
            {
                data->param[0].len = param_len;
                data->param[0].ptr = ptr;       
            }
            offset += param_len;
            ptr += param_len;            
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
            strncpy(data->param[0].ptr + callback->BrokerID,broker.broker_id.c_str(),sizeof(TThostFtdcBrokerIDType) - 1);
        }
        if( callback->UserID >= 0 )
        {
            strncpy(data->param[0].ptr + callback->UserID,broker.account.c_str(),sizeof(TThostFtdcUserIDType) - 1);
        }
        if( callback->InvestorID >= 0 )
        {
            strncpy(data->param[0].ptr + callback->InvestorID,broker.account.c_str(),sizeof(TThostFtdcInvestorIDType) - 1);
        }
        if( callback->AccountID >= 0 )
        {
            strncpy(data->param[0].ptr + callback->AccountID,broker.account.c_str(),sizeof(TThostFtdcAccountIDType) -1);
        }
        printf("call %s !!!!!\n",callback->name);
        funcArr[callback->is_precheck][callback->p_cnt](this,callback);
    }while(0);
    return readable;
}

void TradeSession::CloseClient()
{
    int fd = bufferevent_getfd(net.bev);    
    bufferevent_setfd(net.bev, -1);
    evutil_closesocket(fd);

/**
   Returns the file descriptor associated with a bufferevent, or -1 if
   no file descriptor is associated with the bufferevent.
 */
evutil_socket_t bufferevent_getfd(struct bufferevent *bufev);    
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
    strncpy(RspInfo.ErrorMsg,"Authenticated successfully",sizeof(RspInfo.ErrorMsg) );

    strncpy(RspField.BrokerID,pReqField->BrokerID,sizeof(RspField.BrokerID));
    strncpy(RspField.UserID,pReqField->UserID,sizeof(RspField.UserID));
    strncpy(RspField.UserProductInfo,pReqField->UserProductInfo,sizeof(RspField.UserProductInfo));
    strncpy(RspField.AppID,pReqField->AppID,sizeof(RspField.AppID));
    RspField.AppType = 0;
            
    ((TradeSessionSpi*)m_spi)->doAuthenticate(&RspField, &RspInfo, request_id, true) ;

    action_on_connected = 1;
    return 1;
}

int TradeSession::ReqUserLogin(CThostFtdcReqUserLoginField *pReqField, int request_id)
{
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
        front_addr = g_ctp_mgr->GetFrontAddr(pReqField->BrokerID,front_addr_buf);

    if( !front_addr || !account || !pwd02 )
    {
        printf("debug 001\n");
        CThostFtdcRspUserLoginField RspField = {0};
        CThostFtdcRspInfoField RspInfo;

        RspInfo.ErrorID = 48;
        strncpy(RspInfo.ErrorMsg,"broker not find",sizeof(RspInfo.ErrorMsg) );

        strncpy(RspField.BrokerID,pReqField->BrokerID,sizeof(RspField.BrokerID));
        strncpy(RspField.UserID,pReqField->UserID,sizeof(RspField.UserID));
        
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
    
    strncpy(loginField.BrokerID,broker_id,sizeof(loginField.BrokerID));
    strncpy(loginField.UserID,account,sizeof(loginField.UserID));
    strncpy(loginField.Password,pwd02,sizeof(loginField.Password));
    sqlite3_reset( stmt );

    //避免错误
    memset(loginField.LoginRemark,0,sizeof(loginField.LoginRemark));
    loginField.ClientIPPort = 0;
    memset(loginField.ClientIPAddress,0,sizeof(loginField.ClientIPAddress));

    //首先需要通过认证
    do
    {
        if( has_started )
            break;
        action_on_connected = 0; //连接成功后，默认是登陆
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

bool TradeSession::CheckSelfDeal(const char * instrument_id,double price,int dir)
{
    //此处需要优化，包括价格类型，套保套利，标准套利等情况，还有order巨大情况的性能优化
    for (SessionTable::iterator iter = order_map.begin(); iter != order_map.end(); ++iter)
    {
        // iter->first  = SessionKey
        // iter->second = value字符串
        const SessionKey& key = iter->first;
        if( key.dir == dir )
            continue; //方向相同，不用管

        if( key.ins != instrument_id )
            continue; //合约不同，不用管

        const OrderInfo& data = iter->second;
        if( dir > 0 && price >= data.price )
            return false; //价格交叉，自成交
    }    
    return true;
}

int TradeSession::ReqOrderInsert(CThostFtdcInputOrderField *pInputOrder, int nRequestID)
{
    //1、品种检查，是否有对应品种交易权限
    char product_id_buffer[32];
    product_id_by_instrument( (const unsigned char *)pInputOrder->InstrumentID,product_id_buffer);

    if( !CheckProductPermission(product_id_buffer) )
    {
        //没有对应品种交易权限，拒绝该订单
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 401;
        snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"无品种 %s 交易权限",product_id_buffer);
        m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
        return 0;
    }
    //2、自成交检查
    if( !CheckSelfDeal(pInputOrder->InstrumentID,pInputOrder->LimitPrice,pInputOrder->Direction) )
    {
        //没有对应品种交易权限，拒绝该订单
        CThostFtdcRspInfoField RspInfo = {0};
        RspInfo.ErrorID = 402;
        snprintf(RspInfo.ErrorMsg,sizeof(RspInfo.ErrorMsg) -1,"存在自成交风险(%s/%.3f)",pInputOrder->InstrumentID,pInputOrder->LimitPrice);
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

            m_spi->OnRspOrderInsert(pInputOrder, &RspInfo, nRequestID, true) ;
            return 0;
        }
    }
    pInputOrder->SessionReqSeq = nRequestID;
    
    m_api->ReqOrderInsert(pInputOrder,nRequestID);
    return 1;
}



void TradeSession::OnRtnOrder(CThostFtdcOrderField *pRspField)
{
    if( pRspField->OrderStatus ==  THOST_FTDC_OST_Unknown)
        return;
    int is_finish = 0;
    //处理在途的订单
	switch(pRspField->OrderStatus) //交易已经完结
	{//如果是
	case THOST_FTDC_OST_PartTradedNotQueueing:
	case THOST_FTDC_OST_Canceled:
	case THOST_FTDC_OST_NoTradeNotQueueing:
	case THOST_FTDC_OST_AllTraded:
		is_finish = 1;
	default:
		break;
	}
	if( THOST_FTDC_OSS_InsertRejected == pRspField->OrderSubmitStatus )
		is_finish = 1;

    SessionKey key = {pRspField->InstrumentID,pRspField->OrderSysID,pRspField->Direction};
    if( is_finish )
    {
        //订单完结，更新订单状态
        
        order_map.erase(key);
        return;
    }

    auto iter = order_map.find(key);
    if (iter != order_map.end())
        return;

    order_map[key] = {pRspField->LimitPrice,pRspField->Direction};

}