#pragma once

typedef enum TradeApi_Func_ID
{
	Api_Release = 0,
	Api_Init,
	Api_Join,
	Api_GetTradingDay,
	Api_GetFrontInfo,
	Api_RegisterFront,
	Api_RegisterNameServer,
	Api_RegisterFensUserInfo,
	Api_RegisterSpi,
	Api_SubscribePrivateTopic,
	Api_SubscribePublicTopic,
	Api_ReqAuthenticate,
	Api_RegisterUserSystemInfo,
	Api_SubmitUserSystemInfo,
	Api_RegisterWechatUserSystemInfo,
	Api_SubmitWechatUserSystemInfo,
	Api_ReqUserLogin,
	Api_ReqUserLogout,
	Api_ReqUserPasswordUpdate,
	Api_ReqTradingAccountPasswordUpdate,
	Api_ReqUserAuthMethod,
	Api_ReqGenUserCaptcha,
	Api_ReqGenUserText,
	Api_ReqUserLoginWithCaptcha,
	Api_ReqUserLoginWithText,
	Api_ReqUserLoginWithOTP,
	Api_ReqOrderInsert,
	Api_ReqParkedOrderInsert,
	Api_ReqParkedOrderAction,
	Api_ReqOrderAction,
	Api_ReqQryMaxOrderVolume,
	Api_ReqSettlementInfoConfirm,
	Api_ReqRemoveParkedOrder,
	Api_ReqRemoveParkedOrderAction,
	Api_ReqExecOrderInsert,
	Api_ReqExecOrderAction,
	Api_ReqForQuoteInsert,
	Api_ReqQuoteInsert,
	Api_ReqQuoteAction,
	Api_ReqBatchOrderAction,
	Api_ReqOptionSelfCloseInsert,
	Api_ReqOptionSelfCloseAction,
	Api_ReqCombActionInsert,
	Api_ReqQryOrder,
	Api_ReqQryTrade,
	Api_ReqQryInvestorPosition,
	Api_ReqQryTradingAccount,
	Api_ReqQryInvestor,
	Api_ReqQryTradingCode,
	Api_ReqQryInstrumentMarginRate,
	Api_ReqQryInstrumentCommissionRate,
	Api_ReqQryUserSession,
	Api_ReqQryExchange,
	Api_ReqQryProduct,
	Api_ReqQryInstrument,
	Api_ReqQryDepthMarketData,
	Api_ReqQryTraderOffer,
	Api_ReqQrySettlementInfo,
	Api_ReqQryTransferBank,
	Api_ReqQryInvestorPositionDetail,
	Api_ReqQryNotice,
	Api_ReqQrySettlementInfoConfirm,
	Api_ReqQryInvestorPositionCombineDetail,
	Api_ReqQryCFMMCTradingAccountKey,
	Api_ReqQryEWarrantOffset,
	Api_ReqQryInvestorProductGroupMargin,
	Api_ReqQryExchangeMarginRate,
	Api_ReqQryExchangeMarginRateAdjust,
	Api_ReqQryExchangeRate,
	Api_ReqQrySecAgentACIDMap,
	Api_ReqQryProductExchRate,
	Api_ReqQryProductGroup,
	Api_ReqQryMMInstrumentCommissionRate,
	Api_ReqQryMMOptionInstrCommRate,
	Api_ReqQryInstrumentOrderCommRate,
	Api_ReqQrySecAgentTradingAccount,
	Api_ReqQrySecAgentCheckMode,
	Api_ReqQrySecAgentTradeInfo,
	Api_ReqQryOptionInstrTradeCost,
	Api_ReqQryOptionInstrCommRate,
	Api_ReqQryExecOrder,
	Api_ReqQryForQuote,
	Api_ReqQryQuote,
	Api_ReqQryOptionSelfClose,
	Api_ReqQryInvestUnit,
	Api_ReqQryCombInstrumentGuard,
	Api_ReqQryCombAction,
	Api_ReqQryTransferSerial,
	Api_ReqQryAccountregister,
	Api_ReqQryContractBank,
	Api_ReqQryParkedOrder,
	Api_ReqQryParkedOrderAction,
	Api_ReqQryTradingNotice,
	Api_ReqQryBrokerTradingParams,
	Api_ReqQryBrokerTradingAlgos,
	Api_ReqQueryCFMMCTradingAccountToken,
	Api_ReqFromBankToFutureByFuture,
	Api_ReqFromFutureToBankByFuture,
	Api_ReqQueryBankAccountMoneyByFuture,
	Api_ReqQryClassifiedInstrument,
	Api_ReqQryCombPromotionParam,
	Api_ReqQryRiskSettleInvstPosition,
	Api_ReqQryRiskSettleProductStatus,
	Api_ReqQrySPBMFutureParameter,
	Api_ReqQrySPBMOptionParameter,
	Api_ReqQrySPBMIntraParameter,
	Api_ReqQrySPBMInterParameter,
	Api_ReqQrySPBMPortfDefinition,
	Api_ReqQrySPBMInvestorPortfDef,
	Api_ReqQryInvestorPortfMarginRatio,
	Api_ReqQryInvestorProdSPBMDetail,
	Api_ReqQryInvestorCommoditySPMMMargin,
	Api_ReqQryInvestorCommodityGroupSPMMMargin,
	Api_ReqQrySPMMInstParam,
	Api_ReqQrySPMMProductParam,
	Api_ReqQrySPBMAddOnInterParameter,
	Api_ReqQryRCAMSCombProductInfo,
	Api_ReqQryRCAMSInstrParameter,
	Api_ReqQryRCAMSIntraParameter,
	Api_ReqQryRCAMSInterParameter,
	Api_ReqQryRCAMSShortOptAdjustParam,
	Api_ReqQryRCAMSInvestorCombPosition,
	Api_ReqQryInvestorProdRCAMSMargin,
	Api_ReqQryRULEInstrParameter,
	Api_ReqQryRULEIntraParameter,
	Api_ReqQryRULEInterParameter,
	Api_ReqQryInvestorProdRULEMargin,
	Api_ReqQryInvestorPortfSetting,
	Api_ReqQryInvestorInfoCommRec,
	Api_ReqQryCombLeg,
	Api_ReqOffsetSetting,
	Api_ReqCancelOffsetSetting,
	Api_ReqQryOffsetSetting,
	Api_ReqGenSMSCode,
	Api_ReqSpdApply,
	Api_ReqSpdApplyAction,
	Api_ReqQrySpdApply,
	Api_ReqHedgeCfm,
	Api_ReqHedgeCfmAction,
	Api_ReqQryHedgeCfm,
	Api_Count
}TradeApi_Func_ID;



class CThostFtdcTraderApi;
class TradeSession;

typedef void (CThostFtdcTraderApi::*TradeApi_Func_v)();
typedef int (CThostFtdcTraderApi::*TradeApi_Func_i)();
typedef const char * (CThostFtdcTraderApi::*TradeApi_Func_s)();

typedef void (CThostFtdcTraderApi::*TradeApi_Func_v1)(void * p);

typedef int (CThostFtdcTraderApi::*TradeApi_Func_0)();
typedef int (CThostFtdcTraderApi::*TradeApi_Func_1)(void * p1);
typedef int (CThostFtdcTraderApi::*TradeApi_Func_2)(void * p1,int nRequestID);
typedef int (CThostFtdcTraderApi::*TradeApi_Func_b)(const char *,bool );

//----------------------------------------------------
typedef void (TradeSession::*TradeApi_PreCheck_v)();
typedef int (TradeSession::*TradeApi_PreCheck_i)();
typedef const char * (TradeSession::*TradeApi_PreCheck_s)();

typedef void (TradeSession::*TradeApi_PreCheck_v1)(void * p);

typedef int (TradeSession::*TradeApi_PreCheck_0)();
typedef int (TradeSession::*TradeApi_PreCheck_1)(void * p1);
typedef int (TradeSession::*TradeApi_PreCheck_2)(void * p1,int nRequestID);
typedef int (TradeSession::*TradeApi_PreCheck_b)(const char *,bool );

typedef struct TradeApi_FunctionInfo
{
	union
	{
		TradeApi_Func_v func_v;
		TradeApi_Func_i func_i;
		TradeApi_Func_s func_s;
		TradeApi_Func_v1 func_v1;

		TradeApi_Func_0 func_0;
		TradeApi_Func_1 func_1;
		TradeApi_Func_2 func_2;
		TradeApi_Func_b func_b;

		TradeApi_PreCheck_v check_v;
		TradeApi_PreCheck_i check_i;
		TradeApi_PreCheck_s check_s;
		TradeApi_PreCheck_v1 check_v1;

		TradeApi_PreCheck_0 check_0;
		TradeApi_PreCheck_1 check_1;
		TradeApi_PreCheck_2 check_2;
		TradeApi_PreCheck_b check_b;
	};
	int p_cnt;
	int psize[2];
	const char * name;
	int is_precheck;
	//这三个字段是预检查时需要的参数
	int BrokerID;
	int UserID;
	int InvestorID;
    int AccountID;
}TradeApi_FunctionInfo;

extern TradeApi_FunctionInfo api_function_list[Api_Count];
