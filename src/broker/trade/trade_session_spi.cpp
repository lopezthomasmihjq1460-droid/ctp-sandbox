#include <string.h>

#include "trade_session_spi.h"
#include "trade_session.h"
#include "package.h"

#include "trade_spi.h"

#include <string>

extern std::string g_app_id ;
extern std::string g_auth_code;

#define Safe_strcpy(target, src) \
do \
{ \
	if( !src )\
	{\
		target[0] = 0;\
		break;\
	}\
	strncpy(target, src, sizeof(target) - 1); \
	target[sizeof(target) - 1] = 0; \
} while (0)

#define SpiHelper_OnErrRtn(func,Field) void TradeSessionSpi::func(Field *pRspField, CThostFtdcRspInfoField *pRspInfo) {TradeSpi_RtnErr( func )}
#define SpiHelper_OnErrRtnAll(func,Field) void TradeSessionSpi::func(Field *pRspField, CThostFtdcRspInfoField *pRspInfo) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_RtnErr( func )\
}
#define SpiHelper_OnErrRtnBUV(func,Field) void TradeSessionSpi::func(Field *pRspField, CThostFtdcRspInfoField *pRspInfo) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_RtnErr( func )\
}

#define SpiHelper_OnErrRtnBUA(func,Field) void TradeSessionSpi::func(Field *pRspField, CThostFtdcRspInfoField *pRspInfo) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_RtnErr( func )\
}

#define SpiHelper_Rtn(func,Field) void TradeSessionSpi::func(Field *pRspField) {TradeSpi_Rtn( func )}
#define SpiHelper_Rtn_All(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}

#define SpiHelper_Rtn_BVA(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}
#define SpiHelper_Rtn_BVU(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}

#define SpiHelper_Rtn_BV(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}

#define SpiHelper_Rtn_BUA(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}

#define SpiHelper_Rtn_BA(func,Field) void TradeSessionSpi::func(Field *pRspField) {\
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Rtn( func )\
}

#define SpiHelper_Response(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {TradeSpi_Response( func )}

#define SpiHelper_Response_B(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BA(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BU(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BUA(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BV(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField ) \
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BUV(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_BVA(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

#define SpiHelper_Response_All(func,Field) void TradeSessionSpi::func(Field *pRspField,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) { \
    if( pRspField )\
    {\
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());\
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());\
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());\
    }\
    TradeSpi_Response( func ) \
}

TradeSessionSpi::TradeSessionSpi(TradeSession * session_ptr)
{
	session = session_ptr;
	m_data = &session->net;
}
    
TradeSessionSpi::~TradeSessionSpi()
{
}

	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
void TradeSessionSpi::OnFrontConnected()
{
    printf("TradeSessionSpi::OnFrontConnected action_on_connected=%d\n",session->action_on_connected);

	if( session->action_on_connected == 0 )
    {
        session->m_api->ReqUserLogin(&session->loginField, session->loginRequestId);
        return;
    }

    CThostFtdcReqAuthenticateField field;
    memset(&field,0,sizeof(field));
    strncpy(field.BrokerID, session->broker_id.c_str() ,sizeof(field.BrokerID) -1);
    strncpy(field.UserID,session->user_id.c_str() ,sizeof(field.UserID) -1);

    strncpy(field.AppID,g_app_id.c_str(),sizeof(field.AppID) -1);
    strncpy(field.AuthCode,g_auth_code.c_str(),sizeof(field.AuthCode) -1);

    session->m_api->ReqAuthenticate(&field, session->requestId);
}

///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
///@param nReason 错误原因
///        0x1001 网络读失败
///        0x1002 网络写失败
///        0x2001 接收心跳超时
///        0x2002 发送心跳失败
///        0x2003 收到错误报文
void TradeSessionSpi::OnFrontDisconnected(int nReason)
{
    printf("TradeSessionSpi::OnFrontDisconnected nReason=%d\n",nReason);
	//关闭客户端连接
    session->CloseClient();
}
    
///心跳超时警告。当长时间未收到报文时，该方法被调用。
///@param nTimeLapse 距离上次接收报文的时间
void TradeSessionSpi::OnHeartBeatWarning(int nTimeLapse)
{
}

///客户端认证响应
void TradeSessionSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	//TradeSpi_Response(OnRspAuthenticate);
    session->m_api->ReqUserLogin(&session->loginField, session->loginRequestId);
}

void TradeSessionSpi::doAuthenticate(CThostFtdcRspAuthenticateField *pRspField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	TradeSpi_Response(OnRspAuthenticate);
}

///登录请求响应
SpiHelper_Response_BU(OnRspUserLogin,CThostFtdcRspUserLoginField)

///登出请求响应
SpiHelper_Response_BU(OnRspUserLogout,CThostFtdcUserLogoutField)

///用户口令更新请求响应
SpiHelper_Response_BU(OnRspUserPasswordUpdate,CThostFtdcUserPasswordUpdateField)

///资金账户口令更新请求响应
SpiHelper_Response_BA(OnRspTradingAccountPasswordUpdate,CThostFtdcTradingAccountPasswordUpdateField)

///查询用户当前支持的认证模式的回复
SpiHelper_Response(OnRspUserAuthMethod,CThostFtdcRspUserAuthMethodField)

///获取图形验证码请求的回复
SpiHelper_Response_BU(OnRspGenUserCaptcha,CThostFtdcRspGenUserCaptchaField)

///获取短信验证码请求的回复
SpiHelper_Response(OnRspGenUserText,CThostFtdcRspGenUserTextField)

///报单录入请求响应
SpiHelper_Response_All(OnRspOrderInsert,CThostFtdcInputOrderField)

///预埋单录入请求响应
SpiHelper_Response_All(OnRspParkedOrderInsert,CThostFtdcParkedOrderField)

///预埋撤单录入请求响应
SpiHelper_Response_BUV(OnRspParkedOrderAction,CThostFtdcParkedOrderActionField)

///报单操作请求响应
SpiHelper_Response_BUV(OnRspOrderAction,CThostFtdcInputOrderActionField)

///查询最大报单数量响应
SpiHelper_Response_BV(OnRspQryMaxOrderVolume,CThostFtdcQryMaxOrderVolumeField)

///投资者结算结果确认响应
SpiHelper_Response_BVA(OnRspSettlementInfoConfirm,CThostFtdcSettlementInfoConfirmField)

///删除预埋单响应
SpiHelper_Response_BV(OnRspRemoveParkedOrder,CThostFtdcRemoveParkedOrderField)

///删除预埋撤单响应
SpiHelper_Response_BV(OnRspRemoveParkedOrderAction,CThostFtdcRemoveParkedOrderActionField)

///执行宣告录入请求响应
SpiHelper_Response_All(OnRspExecOrderInsert,CThostFtdcInputExecOrderField)

///执行宣告操作请求响应
SpiHelper_Response_BUV(OnRspExecOrderAction,CThostFtdcInputExecOrderActionField)

///询价录入请求响应
SpiHelper_Response_BUV(OnRspForQuoteInsert,CThostFtdcInputForQuoteField)

///报价录入请求响应
SpiHelper_Response_BUV(OnRspQuoteInsert,CThostFtdcInputQuoteField)

///报价操作请求响应
SpiHelper_Response_BUV(OnRspQuoteAction,CThostFtdcInputQuoteActionField)

///批量报单操作请求响应
SpiHelper_Response_BUV(OnRspBatchOrderAction,CThostFtdcInputBatchOrderActionField)

///期权自对冲录入请求响应
SpiHelper_Response_All(OnRspOptionSelfCloseInsert,CThostFtdcInputOptionSelfCloseField)

///期权自对冲操作请求响应
SpiHelper_Response_BUV(OnRspOptionSelfCloseAction,CThostFtdcInputOptionSelfCloseActionField)

///申请组合录入请求响应
SpiHelper_Response_BUV(OnRspCombActionInsert,CThostFtdcInputCombActionField)

///请求查询报单响应
SpiHelper_Response_All(OnRspQryOrder,CThostFtdcOrderField)

//请求查询成交响应
SpiHelper_Response_BUV(OnRspQryTrade,CThostFtdcTradeField)

///请求查询投资者持仓响应
SpiHelper_Response_BV(OnRspQryInvestorPosition,CThostFtdcInvestorPositionField)

///请求查询资金账户响应
SpiHelper_Response_BA(OnRspQryTradingAccount,CThostFtdcTradingAccountField)

///请求查询投资者响应
SpiHelper_Response_BV(OnRspQryInvestor,CThostFtdcInvestorField)

///请求查询交易编码响应
SpiHelper_Response_BV(OnRspQryTradingCode,CThostFtdcTradingCodeField)

///请求查询合约保证金率响应
SpiHelper_Response_BV(OnRspQryInstrumentMarginRate,CThostFtdcInstrumentMarginRateField)

///请求查询合约手续费率响应
SpiHelper_Response_BV(OnRspQryInstrumentCommissionRate,CThostFtdcInstrumentCommissionRateField)

///请求查询用户会话响应
SpiHelper_Response_BU(OnRspQryUserSession,CThostFtdcUserSessionField)

///请求查询交易所响应
SpiHelper_Response(OnRspQryExchange,CThostFtdcExchangeField)

///请求查询产品响应
SpiHelper_Response(OnRspQryProduct,CThostFtdcProductField)

///请求查询合约响应
SpiHelper_Response(OnRspQryInstrument,CThostFtdcInstrumentField)

///请求查询行情响应
SpiHelper_Response(OnRspQryDepthMarketData,CThostFtdcDepthMarketDataField)

///请求查询交易员报盘机响应
SpiHelper_Response(OnRspQryTraderOffer,CThostFtdcTraderOfferField)

///请求查询投资者结算结果响应
SpiHelper_Response_BVA(OnRspQrySettlementInfo,CThostFtdcSettlementInfoField)

///请求查询转帐银行响应
SpiHelper_Response(OnRspQryTransferBank,CThostFtdcTransferBankField)

///请求查询投资者持仓明细响应
SpiHelper_Response_BV(OnRspQryInvestorPositionDetail,CThostFtdcInvestorPositionDetailField)

///请求查询客户通知响应
SpiHelper_Response_B(OnRspQryNotice,CThostFtdcNoticeField)

///请求查询结算信息确认响应
SpiHelper_Response_BVA(OnRspQrySettlementInfoConfirm,CThostFtdcSettlementInfoConfirmField)

///请求查询投资者持仓明细响应
SpiHelper_Response_BV(OnRspQryInvestorPositionCombineDetail,CThostFtdcInvestorPositionCombineDetailField)

///查询保证金监管系统经纪公司资金账户密钥响应
SpiHelper_Response_BA(OnRspQryCFMMCTradingAccountKey,CThostFtdcCFMMCTradingAccountKeyField)

///请求查询仓单折抵信息响应
SpiHelper_Response_BV(OnRspQryEWarrantOffset,CThostFtdcEWarrantOffsetField)

///请求查询投资者品种/跨品种保证金响应
SpiHelper_Response_BV(OnRspQryInvestorProductGroupMargin,CThostFtdcInvestorProductGroupMarginField)

///请求查询交易所保证金率响应
SpiHelper_Response_B(OnRspQryExchangeMarginRate,CThostFtdcExchangeMarginRateField)

///请求查询交易所调整保证金率响应
SpiHelper_Response_B(OnRspQryExchangeMarginRateAdjust,CThostFtdcExchangeMarginRateAdjustField)

///请求查询汇率响应
SpiHelper_Response_B(OnRspQryExchangeRate,CThostFtdcExchangeRateField)

///请求查询二级代理操作员银期权限响应
SpiHelper_Response_BUA(OnRspQrySecAgentACIDMap,CThostFtdcSecAgentACIDMapField)

///请求查询产品报价汇率
SpiHelper_Response(OnRspQryProductExchRate,CThostFtdcProductExchRateField)

///请求查询产品组
SpiHelper_Response(OnRspQryProductGroup,CThostFtdcProductGroupField)

///请求查询做市商合约手续费率响应
SpiHelper_Response_BV(OnRspQryMMInstrumentCommissionRate,CThostFtdcMMInstrumentCommissionRateField)

///请求查询做市商期权合约手续费响应
SpiHelper_Response_BV(OnRspQryMMOptionInstrCommRate,CThostFtdcMMOptionInstrCommRateField)

///请求查询报单手续费响应
SpiHelper_Response_BV(OnRspQryInstrumentOrderCommRate,CThostFtdcInstrumentOrderCommRateField)

///请求查询资金账户响应
SpiHelper_Response_BA(OnRspQrySecAgentTradingAccount,CThostFtdcTradingAccountField)

///请求查询二级代理商资金校验模式响应
SpiHelper_Response_BV(OnRspQrySecAgentCheckMode,CThostFtdcSecAgentCheckModeField)

///请求查询二级代理商信息响应
SpiHelper_Response_BV(OnRspQrySecAgentTradeInfo,CThostFtdcSecAgentTradeInfoField)

///请求查询期权交易成本响应
SpiHelper_Response_BV(OnRspQryOptionInstrTradeCost,CThostFtdcOptionInstrTradeCostField)

///请求查询期权合约手续费响应
SpiHelper_Response_BV(OnRspQryOptionInstrCommRate,CThostFtdcOptionInstrCommRateField)

///请求查询执行宣告响应
SpiHelper_Response_All(OnRspQryExecOrder,CThostFtdcExecOrderField)

///请求查询询价响应
SpiHelper_Response_BUV(OnRspQryForQuote,CThostFtdcForQuoteField)

///请求查询报价响应
SpiHelper_Response_BUV(OnRspQryQuote,CThostFtdcQuoteField)

///请求查询期权自对冲响应
SpiHelper_Response_BUV(OnRspQryOptionSelfClose,CThostFtdcOptionSelfCloseField)

///请求查询投资单元响应
SpiHelper_Response_BVA(OnRspQryInvestUnit,CThostFtdcInvestUnitField)

///请求查询组合合约安全系数响应
SpiHelper_Response_B(OnRspQryCombInstrumentGuard,CThostFtdcCombInstrumentGuardField)

///请求查询申请组合响应
SpiHelper_Response_BUV(OnRspQryCombAction,CThostFtdcCombActionField)

///请求查询转帐流水响应
SpiHelper_Response_BVA(OnRspQryTransferSerial,CThostFtdcTransferSerialField)

///请求查询银期签约关系响应
SpiHelper_Response_BA(OnRspQryAccountregister,CThostFtdcAccountregisterField)

///错误应答
void TradeSessionSpi::OnRspError(CThostFtdcRspInfoField *pRspField, int nRequestID, bool bIsLast)
{
    TradeSpi_RspError( OnRspError )
}

///报单通知
void TradeSessionSpi::OnRtnOrder(CThostFtdcOrderField *pRspField)
{
    session->OnRtnOrder(pRspField); //需要检查订单是否完结，用于自成交数据判断
    if( pRspField )
    {
        Safe_strcpy(pRspField->BrokerID,session->broker.raw_broker.c_str());
        Safe_strcpy(pRspField->UserID,session->broker.raw_user.c_str());
        Safe_strcpy(pRspField->InvestorID,session->broker.raw_user.c_str());
        Safe_strcpy(pRspField->AccountID,session->broker.raw_user.c_str());
    }    
    TradeSpi_Rtn( OnRtnOrder )
}

///成交通知
SpiHelper_Rtn_BVU(OnRtnTrade,CThostFtdcTradeField)

// ///报单录入错误回报
SpiHelper_OnErrRtnAll(OnErrRtnOrderInsert,CThostFtdcInputOrderField)

// ///报单操作错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnOrderAction,CThostFtdcOrderActionField)

///合约交易状态通知
SpiHelper_Rtn(OnRtnInstrumentStatus,CThostFtdcInstrumentStatusField)

///交易所公告通知
SpiHelper_Rtn(OnRtnBulletin,CThostFtdcBulletinField)

///交易通知
SpiHelper_Rtn_BV(OnRtnTradingNotice,CThostFtdcTradingNoticeInfoField)

///提示条件单校验错误
SpiHelper_Rtn_All(OnRtnErrorConditionalOrder,CThostFtdcErrorConditionalOrderField)

///执行宣告通知
SpiHelper_Rtn_All(OnRtnExecOrder,CThostFtdcExecOrderField)

// ///执行宣告录入错误回报
SpiHelper_OnErrRtnAll(OnErrRtnExecOrderInsert,CThostFtdcInputExecOrderField)

// ///执行宣告操作错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnExecOrderAction,CThostFtdcExecOrderActionField)

///询价录入错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnForQuoteInsert,CThostFtdcInputForQuoteField)

///报价通知
SpiHelper_Rtn_All(OnRtnQuote,CThostFtdcQuoteField)

///报价录入错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnQuoteInsert,CThostFtdcInputQuoteField)

// ///报价操作错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnQuoteAction,CThostFtdcQuoteActionField)

///询价通知
SpiHelper_Rtn(OnRtnForQuoteRsp,CThostFtdcForQuoteRspField)

///保证金监控中心用户令牌
SpiHelper_Rtn_BA(OnRtnCFMMCTradingAccountToken,CThostFtdcCFMMCTradingAccountTokenField)

///批量报单操作错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnBatchOrderAction,CThostFtdcBatchOrderActionField)

///期权自对冲通知
SpiHelper_Rtn_All(OnRtnOptionSelfClose,CThostFtdcOptionSelfCloseField)

///期权自对冲录入错误回报
SpiHelper_OnErrRtnAll(OnErrRtnOptionSelfCloseInsert,CThostFtdcInputOptionSelfCloseField)

///期权自对冲操作错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnOptionSelfCloseAction,CThostFtdcOptionSelfCloseActionField)

//申请组合通知
SpiHelper_Rtn_BVU(OnRtnCombAction,CThostFtdcCombActionField)

///申请组合录入错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnCombActionInsert,CThostFtdcInputCombActionField)

///请求查询签约银行响应
SpiHelper_Response_B(OnRspQryContractBank,CThostFtdcContractBankField)

///请求查询预埋单响应
SpiHelper_Response_All(OnRspQryParkedOrder,CThostFtdcParkedOrderField)

///请求查询预埋撤单响应
SpiHelper_Response_BUV(OnRspQryParkedOrderAction,CThostFtdcParkedOrderActionField)

///请求查询交易通知响应
SpiHelper_Response_BUV(OnRspQryTradingNotice,CThostFtdcTradingNoticeField)

///请求查询经纪公司交易参数响应
SpiHelper_Response_BVA(OnRspQryBrokerTradingParams,CThostFtdcBrokerTradingParamsField)

//请求查询经纪公司交易算法响应
SpiHelper_Response_B(OnRspQryBrokerTradingAlgos,CThostFtdcBrokerTradingAlgosField)

///请求查询监控中心用户令牌
SpiHelper_Response_BV(OnRspQueryCFMMCTradingAccountToken,CThostFtdcQueryCFMMCTradingAccountTokenField)

///银行发起银行资金转期货通知
SpiHelper_Rtn_BA(OnRtnFromBankToFutureByBank,CThostFtdcRspTransferField)

///银行发起期货资金转银行通知
SpiHelper_Rtn_BUA(OnRtnFromFutureToBankByBank,CThostFtdcRspTransferField)

///银行发起冲正银行转期货通知
SpiHelper_Rtn_BUA(OnRtnRepealFromBankToFutureByBank,CThostFtdcRspRepealField)

///银行发起冲正期货转银行通知
SpiHelper_Rtn_BUA(OnRtnRepealFromFutureToBankByBank,CThostFtdcRspRepealField)

///期货发起银行资金转期货通知
SpiHelper_Rtn_BUA(OnRtnFromBankToFutureByFuture,CThostFtdcRspTransferField)

///期货发起期货资金转银行通知
SpiHelper_Rtn_BUA(OnRtnFromFutureToBankByFuture,CThostFtdcRspTransferField)

///系统运行时期货端手工发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
SpiHelper_Rtn_BUA(OnRtnRepealFromBankToFutureByFutureManual,CThostFtdcRspRepealField)

///系统运行时期货端手工发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
SpiHelper_Rtn_BUA(OnRtnRepealFromFutureToBankByFutureManual,CThostFtdcRspRepealField)

///期货发起查询银行余额通知
SpiHelper_Rtn_BUA(OnRtnQueryBankBalanceByFuture,CThostFtdcNotifyQueryAccountField)

///期货发起银行资金转期货错误回报
SpiHelper_OnErrRtnBUA(OnErrRtnBankToFutureByFuture,CThostFtdcReqTransferField)

///期货发起期货资金转银行错误回报
SpiHelper_OnErrRtnBUA(OnErrRtnFutureToBankByFuture,CThostFtdcReqTransferField)

///系统运行时期货端手工发起冲正银行转期货错误回报
SpiHelper_OnErrRtnBUA(OnErrRtnRepealBankToFutureByFutureManual,CThostFtdcReqRepealField)

///系统运行时期货端手工发起冲正期货转银行错误回报
SpiHelper_OnErrRtnBUA(OnErrRtnRepealFutureToBankByFutureManual,CThostFtdcReqRepealField)

///期货发起查询银行余额错误回报
SpiHelper_OnErrRtnBUA(OnErrRtnQueryBankBalanceByFuture,CThostFtdcReqQueryAccountField)

///期货发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
SpiHelper_Rtn_BUA(OnRtnRepealFromBankToFutureByFuture,CThostFtdcRspRepealField)

///期货发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
SpiHelper_Rtn_BUA(OnRtnRepealFromFutureToBankByFuture,CThostFtdcRspRepealField)

///期货发起银行资金转期货应答
SpiHelper_Response_BUA(OnRspFromBankToFutureByFuture,CThostFtdcReqTransferField)

///期货发起期货资金转银行应答
SpiHelper_Response_BUA(OnRspFromFutureToBankByFuture,CThostFtdcReqTransferField)

///期货发起查询银行余额应答
SpiHelper_Response_BUA(OnRspQueryBankAccountMoneyByFuture,CThostFtdcReqQueryAccountField)

///银行发起银期开户通知
SpiHelper_Rtn_BUA(OnRtnOpenAccountByBank,CThostFtdcOpenAccountField)

///银行发起银期销户通知
SpiHelper_Rtn_BUA(OnRtnCancelAccountByBank,CThostFtdcCancelAccountField)

///银行发起变更银行账号通知
SpiHelper_Rtn_BA(OnRtnChangeAccountByBank,CThostFtdcChangeAccountField)

///请求查询分类合约响应
SpiHelper_Response(OnRspQryClassifiedInstrument,CThostFtdcInstrumentField)

///请求组合优惠比例响应
SpiHelper_Response(OnRspQryCombPromotionParam,CThostFtdcCombPromotionParamField)

///投资者风险结算持仓查询响应
SpiHelper_Response_BV(OnRspQryRiskSettleInvstPosition,CThostFtdcRiskSettleInvstPositionField)

///风险结算产品查询响应
SpiHelper_Response(OnRspQryRiskSettleProductStatus,CThostFtdcRiskSettleProductStatusField)

///SPBM期货合约参数查询响应
SpiHelper_Response(OnRspQrySPBMFutureParameter,CThostFtdcSPBMFutureParameterField)

///SPBM期权合约参数查询响应
SpiHelper_Response(OnRspQrySPBMOptionParameter,CThostFtdcSPBMOptionParameterField)

///SPBM品种内对锁仓折扣参数查询响应
SpiHelper_Response(OnRspQrySPBMIntraParameter,CThostFtdcSPBMIntraParameterField)

///SPBM跨品种抵扣参数查询响应
SpiHelper_Response(OnRspQrySPBMInterParameter,CThostFtdcSPBMInterParameterField)

///SPBM组合保证金套餐查询响应
SpiHelper_Response(OnRspQrySPBMPortfDefinition,CThostFtdcSPBMPortfDefinitionField)

///投资者SPBM套餐选择查询响应
SpiHelper_Response_BV(OnRspQrySPBMInvestorPortfDef,CThostFtdcSPBMInvestorPortfDefField)

///投资者新型组合保证金系数查询响应
SpiHelper_Response_BV(OnRspQryInvestorPortfMarginRatio,CThostFtdcInvestorPortfMarginRatioField)

///投资者产品SPBM明细查询响应
SpiHelper_Response_BV(OnRspQryInvestorProdSPBMDetail,CThostFtdcInvestorProdSPBMDetailField)

///投资者商品组SPMM记录查询响应
SpiHelper_Response_BV(OnRspQryInvestorCommoditySPMMMargin,CThostFtdcInvestorCommoditySPMMMarginField)

///投资者商品群SPMM记录查询响应
SpiHelper_Response_BV(OnRspQryInvestorCommodityGroupSPMMMargin,CThostFtdcInvestorCommodityGroupSPMMMarginField)

///SPMM合约参数查询响应
SpiHelper_Response(OnRspQrySPMMInstParam,CThostFtdcSPMMInstParamField)

///SPMM产品参数查询响应
SpiHelper_Response(OnRspQrySPMMProductParam,CThostFtdcSPMMProductParamField)

///SPBM附加跨品种抵扣参数查询响应
SpiHelper_Response(OnRspQrySPBMAddOnInterParameter,CThostFtdcSPBMAddOnInterParameterField)

///RCAMS产品组合信息查询响应
SpiHelper_Response(OnRspQryRCAMSCombProductInfo,CThostFtdcRCAMSCombProductInfoField)

///RCAMS同合约风险对冲参数查询响应
SpiHelper_Response(OnRspQryRCAMSInstrParameter,CThostFtdcRCAMSInstrParameterField)

///RCAMS品种内风险对冲参数查询响应
SpiHelper_Response(OnRspQryRCAMSIntraParameter,CThostFtdcRCAMSIntraParameterField)

///RCAMS跨品种风险折抵参数查询响应
SpiHelper_Response(OnRspQryRCAMSInterParameter,CThostFtdcRCAMSInterParameterField)

///RCAMS空头期权风险调整参数查询响应
SpiHelper_Response(OnRspQryRCAMSShortOptAdjustParam,CThostFtdcRCAMSShortOptAdjustParamField)

///RCAMS策略组合持仓查询响应
SpiHelper_Response_BV(OnRspQryRCAMSInvestorCombPosition,CThostFtdcRCAMSInvestorCombPositionField)

///投资者品种RCAMS保证金查询响应
SpiHelper_Response_BV(OnRspQryInvestorProdRCAMSMargin,CThostFtdcInvestorProdRCAMSMarginField)

///RULE合约保证金参数查询响应
SpiHelper_Response(OnRspQryRULEInstrParameter,CThostFtdcRULEInstrParameterField)

///RULE品种内对锁仓折扣参数查询响应
SpiHelper_Response(OnRspQryRULEIntraParameter,CThostFtdcRULEIntraParameterField)

///RULE跨品种抵扣参数查询响应
SpiHelper_Response(OnRspQryRULEInterParameter,CThostFtdcRULEInterParameterField)

///投资者产品RULE保证金查询响应
SpiHelper_Response_BV(OnRspQryInvestorProdRULEMargin,CThostFtdcInvestorProdRULEMarginField)

///投资者新型组合保证金开关查询响应
SpiHelper_Response_BV(OnRspQryInvestorPortfSetting,CThostFtdcInvestorPortfSettingField)

///投资者申报费阶梯收取记录查询响应
SpiHelper_Response_BV(OnRspQryInvestorInfoCommRec,CThostFtdcInvestorInfoCommRecField)

///组合腿信息查询响应
SpiHelper_Response(OnRspQryCombLeg,CThostFtdcCombLegField)

///对冲设置请求响应
SpiHelper_Response_BUV(OnRspOffsetSetting,CThostFtdcInputOffsetSettingField)

///对冲设置撤销请求响应
SpiHelper_Response_BUV(OnRspCancelOffsetSetting,CThostFtdcInputOffsetSettingField)

///对冲设置通知
SpiHelper_Rtn_BVU(OnRtnOffsetSetting,CThostFtdcOffsetSettingField)

// ///对冲设置错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnOffsetSetting,CThostFtdcInputOffsetSettingField)

// ///对冲设置撤销错误回报
SpiHelper_OnErrRtnBUV(OnErrRtnCancelOffsetSetting,CThostFtdcCancelOffsetSettingField)

///投资者对冲设置查询响应
SpiHelper_Response_BUV(OnRspQryOffsetSetting,CThostFtdcOffsetSettingField)
