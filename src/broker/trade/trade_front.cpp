#include "trade_inf.h"

#include "ThostFtdcTraderApi.h"
#include "trade_session.h"

#define Declare2Func_0(FuncName)      {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},0,{0,0},#FuncName,0,-1,-1,-1,-1}
#define Declare2Func_1(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},1,{sizeof(type),0},#FuncName,0,-1,-1,-1,-1}
#define Declare2Func_2(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,-1,-1,-1,-1}

#define FiledOffset(type,field) ((const char *)&(((type *)0)->field) - (const char *)0)

#define Declare2Func_all(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,FiledOffset(type,BrokerID),FiledOffset(type,UserID),FiledOffset(type,InvestorID),-1}

#define Declare2Func_B(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,FiledOffset(type,BrokerID),-1,-1,-1}
#define Declare2Func_B_U(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,FiledOffset(type,BrokerID),FiledOffset(type,UserID),-1,-1}
#define Declare2Func_B_V(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,FiledOffset(type,BrokerID),-1,FiledOffset(type,InvestorID),-1}
#define Declare2Func_BVA(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,FiledOffset(type,BrokerID),-1,FiledOffset(type,InvestorID),FiledOffset(type,AccountID)}

#define Declare2Func_U(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,-1,FiledOffset(type,UserID),-1,-1}
#define Declare2Func_V(FuncName,type) {{(TradeApi_Func_v)&CThostFtdcTraderApi::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,0,-1,-1,FiledOffset(type,InvestorID),-1}


#define PreCheckFunc_1(FuncName,type) {{(TradeApi_Func_v)&TradeSession::FuncName},1,{sizeof(type),0},#FuncName,1,-1,-1,-1,-1}
#define PreCheckFunc_2(FuncName,type) {{(TradeApi_Func_v)&TradeSession::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,1,-1,-1,-1,-1}
#define PreCheckFunc_all(FuncName,type) {{(TradeApi_Func_v)&TradeSession::FuncName},2,{sizeof(type),sizeof(int)},#FuncName,1,FiledOffset(type,BrokerID),FiledOffset(type,UserID),FiledOffset(type,InvestorID),-1}


TradeApi_FunctionInfo api_function_list[Api_Count] = 
{
	Declare2Func_0(Release),
	Declare2Func_0(Init),
	Declare2Func_0(Join),
	Declare2Func_0(GetTradingDay),
	Declare2Func_1(GetFrontInfo,CThostFtdcFrontInfoField),
	Declare2Func_1(RegisterFront, char *), //需要单独配置调用
	Declare2Func_1(RegisterNameServer,char *),
	Declare2Func_1(RegisterFensUserInfo,CThostFtdcFensUserInfoField ),
	Declare2Func_1(RegisterSpi,CThostFtdcTraderSpi ), //单独调用,信息发给client
//--------------------------------------------------------------------------------------
	PreCheckFunc_1(SubscribePrivateTopic,THOST_TE_RESUME_TYPE ),
	PreCheckFunc_1(SubscribePublicTopic,THOST_TE_RESUME_TYPE ),

	PreCheckFunc_2(ReqAuthenticate,CThostFtdcReqAuthenticateField ),
	Declare2Func_B_U(RegisterUserSystemInfo,CThostFtdcUserSystemInfoField ),
	Declare2Func_B_U(SubmitUserSystemInfo,CThostFtdcUserSystemInfoField ),
	Declare2Func_B_U(RegisterWechatUserSystemInfo,CThostFtdcWechatUserSystemInfoField ),
	Declare2Func_B_U(SubmitWechatUserSystemInfo,CThostFtdcWechatUserSystemInfoField ),
	PreCheckFunc_2(ReqUserLogin,CThostFtdcReqUserLoginField ),
	Declare2Func_B_U(ReqUserLogout,CThostFtdcUserLogoutField ),
	Declare2Func_B_U(ReqUserPasswordUpdate,CThostFtdcUserPasswordUpdateField ),
	Declare2Func_B(ReqTradingAccountPasswordUpdate,CThostFtdcTradingAccountPasswordUpdateField ),
	Declare2Func_B_U(ReqUserAuthMethod,CThostFtdcReqUserAuthMethodField ),
	Declare2Func_B_U(ReqGenUserCaptcha,CThostFtdcReqGenUserCaptchaField ),
	Declare2Func_B_U(ReqGenUserText,CThostFtdcReqGenUserTextField ),
	Declare2Func_B_U(ReqUserLoginWithCaptcha,CThostFtdcReqUserLoginWithCaptchaField ),
	Declare2Func_B_U(ReqUserLoginWithText,CThostFtdcReqUserLoginWithTextField ),
	Declare2Func_B_U(ReqUserLoginWithOTP,CThostFtdcReqUserLoginWithOTPField ),
	PreCheckFunc_all(ReqOrderInsert,CThostFtdcInputOrderField ),
	
	Declare2Func_all(ReqParkedOrderInsert,CThostFtdcParkedOrderField ),
	Declare2Func_all(ReqParkedOrderAction,CThostFtdcParkedOrderActionField ),
	Declare2Func_all(ReqOrderAction,CThostFtdcInputOrderActionField ),
	Declare2Func_B(ReqQryMaxOrderVolume,CThostFtdcQryMaxOrderVolumeField ),
	Declare2Func_B_V(ReqSettlementInfoConfirm,CThostFtdcSettlementInfoConfirmField ),
	Declare2Func_B_V(ReqRemoveParkedOrder,CThostFtdcRemoveParkedOrderField ),
	Declare2Func_B_V(ReqRemoveParkedOrderAction,CThostFtdcRemoveParkedOrderActionField ),
	Declare2Func_all(ReqExecOrderInsert,CThostFtdcInputExecOrderField ),
	Declare2Func_all(ReqExecOrderAction,CThostFtdcInputExecOrderActionField ),
	Declare2Func_all(ReqForQuoteInsert,CThostFtdcInputForQuoteField ),
	Declare2Func_all(ReqQuoteInsert,CThostFtdcInputQuoteField ),
	Declare2Func_all(ReqQuoteAction,CThostFtdcInputQuoteActionField ),
	Declare2Func_all(ReqBatchOrderAction,CThostFtdcInputBatchOrderActionField ),
	Declare2Func_all(ReqOptionSelfCloseInsert,CThostFtdcInputOptionSelfCloseField ),
	Declare2Func_all(ReqOptionSelfCloseAction,CThostFtdcInputOptionSelfCloseActionField ),
	Declare2Func_all(ReqCombActionInsert,CThostFtdcInputCombActionField ),
	Declare2Func_B_V(ReqQryOrder,CThostFtdcQryOrderField ),

	Declare2Func_B_V(ReqQryTrade,CThostFtdcQryTradeField ),
	Declare2Func_B_V(ReqQryInvestorPosition,CThostFtdcQryInvestorPositionField ),
	Declare2Func_BVA(ReqQryTradingAccount,CThostFtdcQryTradingAccountField ),
	Declare2Func_B_V(ReqQryInvestor,CThostFtdcQryInvestorField ),
	Declare2Func_B_V(ReqQryTradingCode,CThostFtdcQryTradingCodeField ),
	Declare2Func_B_V(ReqQryInstrumentMarginRate,CThostFtdcQryInstrumentMarginRateField ),
	Declare2Func_B_V(ReqQryInstrumentCommissionRate,CThostFtdcQryInstrumentCommissionRateField ),

	Declare2Func_B_U(ReqQryUserSession,CThostFtdcQryUserSessionField ),
	Declare2Func_2(ReqQryExchange,CThostFtdcQryExchangeField ),
	Declare2Func_2(ReqQryProduct,CThostFtdcQryProductField ),
	Declare2Func_2(ReqQryInstrument,CThostFtdcQryInstrumentField ),
	Declare2Func_2(ReqQryDepthMarketData,CThostFtdcQryDepthMarketDataField ),
	Declare2Func_2(ReqQryTraderOffer,CThostFtdcQryTraderOfferField ),
	Declare2Func_B_V(ReqQrySettlementInfo,CThostFtdcQrySettlementInfoField ),
	Declare2Func_2(ReqQryTransferBank,CThostFtdcQryTransferBankField ),
	Declare2Func_B_V(ReqQryInvestorPositionDetail,CThostFtdcQryInvestorPositionDetailField ),
	Declare2Func_B(ReqQryNotice,CThostFtdcQryNoticeField ),
	Declare2Func_B_V(ReqQrySettlementInfoConfirm,CThostFtdcQrySettlementInfoConfirmField ),
	Declare2Func_B_V(ReqQryInvestorPositionCombineDetail,CThostFtdcQryInvestorPositionCombineDetailField ),
	Declare2Func_B_V(ReqQryCFMMCTradingAccountKey,CThostFtdcQryCFMMCTradingAccountKeyField ),
	Declare2Func_B_V(ReqQryEWarrantOffset,CThostFtdcQryEWarrantOffsetField ),
	Declare2Func_B_V(ReqQryInvestorProductGroupMargin,CThostFtdcQryInvestorProductGroupMarginField ),
	Declare2Func_B(ReqQryExchangeMarginRate,CThostFtdcQryExchangeMarginRateField ),
	Declare2Func_B(ReqQryExchangeMarginRateAdjust,CThostFtdcQryExchangeMarginRateAdjustField ),
	Declare2Func_B(ReqQryExchangeRate,CThostFtdcQryExchangeRateField ),
	Declare2Func_B_U(ReqQrySecAgentACIDMap,CThostFtdcQrySecAgentACIDMapField ),
	Declare2Func_2(ReqQryProductExchRate,CThostFtdcQryProductExchRateField ),
	Declare2Func_2(ReqQryProductGroup,CThostFtdcQryProductGroupField ),
	Declare2Func_B_V(ReqQryMMInstrumentCommissionRate,CThostFtdcQryMMInstrumentCommissionRateField ),
	Declare2Func_B_V(ReqQryMMOptionInstrCommRate,CThostFtdcQryMMOptionInstrCommRateField ),
	Declare2Func_B_V(ReqQryInstrumentOrderCommRate,CThostFtdcQryInstrumentOrderCommRateField ),
	Declare2Func_B_V(ReqQrySecAgentTradingAccount,CThostFtdcQryTradingAccountField ),
	Declare2Func_B_V(ReqQrySecAgentCheckMode,CThostFtdcQrySecAgentCheckModeField ),
	Declare2Func_B(ReqQrySecAgentTradeInfo,CThostFtdcQrySecAgentTradeInfoField ),
	Declare2Func_B_V(ReqQryOptionInstrTradeCost,CThostFtdcQryOptionInstrTradeCostField ),
	Declare2Func_B_V(ReqQryOptionInstrCommRate,CThostFtdcQryOptionInstrCommRateField ),
	Declare2Func_B_V(ReqQryExecOrder,CThostFtdcQryExecOrderField ),
	Declare2Func_B_V(ReqQryForQuote,CThostFtdcQryForQuoteField ),
	Declare2Func_B_V(ReqQryQuote,CThostFtdcQryQuoteField ),
	Declare2Func_B_V(ReqQryOptionSelfClose,CThostFtdcQryOptionSelfCloseField ),
	Declare2Func_B_V(ReqQryInvestUnit,CThostFtdcQryInvestUnitField ),
	Declare2Func_B(ReqQryCombInstrumentGuard,CThostFtdcQryCombInstrumentGuardField ),
	Declare2Func_B_V(ReqQryCombAction,CThostFtdcQryCombActionField ),
	Declare2Func_B(ReqQryTransferSerial,CThostFtdcQryTransferSerialField ),
	Declare2Func_B(ReqQryAccountregister,CThostFtdcQryAccountregisterField ),
	Declare2Func_B(ReqQryContractBank,CThostFtdcQryContractBankField ),
	Declare2Func_B_V(ReqQryParkedOrder,CThostFtdcQryParkedOrderField ),
	Declare2Func_B_V(ReqQryParkedOrderAction,CThostFtdcQryParkedOrderActionField ),
	Declare2Func_B_V(ReqQryTradingNotice,CThostFtdcQryTradingNoticeField ),
	Declare2Func_B_V(ReqQryBrokerTradingParams,CThostFtdcQryBrokerTradingParamsField ),
	Declare2Func_B(ReqQryBrokerTradingAlgos,CThostFtdcQryBrokerTradingAlgosField ),
	Declare2Func_B_V(ReqQueryCFMMCTradingAccountToken,CThostFtdcQueryCFMMCTradingAccountTokenField ),
	Declare2Func_B_U(ReqFromBankToFutureByFuture,CThostFtdcReqTransferField ),
	Declare2Func_B_U(ReqFromFutureToBankByFuture,CThostFtdcReqTransferField ),
	Declare2Func_B_U(ReqQueryBankAccountMoneyByFuture,CThostFtdcReqQueryAccountField ),
	Declare2Func_2(ReqQryClassifiedInstrument,CThostFtdcQryClassifiedInstrumentField ),
	Declare2Func_2(ReqQryCombPromotionParam,CThostFtdcQryCombPromotionParamField ),
	Declare2Func_B_V(ReqQryRiskSettleInvstPosition,CThostFtdcQryRiskSettleInvstPositionField ),
	Declare2Func_2(ReqQryRiskSettleProductStatus,CThostFtdcQryRiskSettleProductStatusField ),
	Declare2Func_2(ReqQrySPBMFutureParameter,CThostFtdcQrySPBMFutureParameterField ),
	Declare2Func_2(ReqQrySPBMOptionParameter,CThostFtdcQrySPBMOptionParameterField ),
	Declare2Func_2(ReqQrySPBMIntraParameter,CThostFtdcQrySPBMIntraParameterField ),
	Declare2Func_2(ReqQrySPBMInterParameter,CThostFtdcQrySPBMInterParameterField ),

	Declare2Func_2(ReqQrySPBMPortfDefinition,CThostFtdcQrySPBMPortfDefinitionField ),
	Declare2Func_B_V(ReqQrySPBMInvestorPortfDef,CThostFtdcQrySPBMInvestorPortfDefField ),
	Declare2Func_B_V(ReqQryInvestorPortfMarginRatio,CThostFtdcQryInvestorPortfMarginRatioField ),
	Declare2Func_B_V(ReqQryInvestorProdSPBMDetail,CThostFtdcQryInvestorProdSPBMDetailField ),
	Declare2Func_B_V(ReqQryInvestorCommoditySPMMMargin,CThostFtdcQryInvestorCommoditySPMMMarginField ),
	Declare2Func_B_V(ReqQryInvestorCommodityGroupSPMMMargin,CThostFtdcQryInvestorCommodityGroupSPMMMarginField ),
	Declare2Func_2(ReqQrySPMMInstParam,CThostFtdcQrySPMMInstParamField ),
	Declare2Func_2(ReqQrySPMMProductParam,CThostFtdcQrySPMMProductParamField ),
	Declare2Func_2(ReqQrySPBMAddOnInterParameter,CThostFtdcQrySPBMAddOnInterParameterField ),
	Declare2Func_2(ReqQryRCAMSCombProductInfo,CThostFtdcQryRCAMSCombProductInfoField ),
	Declare2Func_2(ReqQryRCAMSInstrParameter,CThostFtdcQryRCAMSInstrParameterField ),
	Declare2Func_2(ReqQryRCAMSIntraParameter,CThostFtdcQryRCAMSIntraParameterField ),
	Declare2Func_2(ReqQryRCAMSInterParameter,CThostFtdcQryRCAMSInterParameterField ),
	Declare2Func_2(ReqQryRCAMSShortOptAdjustParam,CThostFtdcQryRCAMSShortOptAdjustParamField ),
	Declare2Func_B_V(ReqQryRCAMSInvestorCombPosition,CThostFtdcQryRCAMSInvestorCombPositionField ),
	Declare2Func_B_V(ReqQryInvestorProdRCAMSMargin,CThostFtdcQryInvestorProdRCAMSMarginField ),
	Declare2Func_2(ReqQryRULEInstrParameter,CThostFtdcQryRULEInstrParameterField ),
	Declare2Func_2(ReqQryRULEIntraParameter,CThostFtdcQryRULEIntraParameterField ),
	Declare2Func_2(ReqQryRULEInterParameter,CThostFtdcQryRULEInterParameterField ),
	Declare2Func_B_V(ReqQryInvestorProdRULEMargin,CThostFtdcQryInvestorProdRULEMarginField ),
	Declare2Func_B_V(ReqQryInvestorPortfSetting,CThostFtdcQryInvestorPortfSettingField ),
	Declare2Func_B_V(ReqQryInvestorInfoCommRec,CThostFtdcQryInvestorInfoCommRecField ),
	Declare2Func_2(ReqQryCombLeg,CThostFtdcQryCombLegField ),
	Declare2Func_all(ReqOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_all(ReqCancelOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_B_V(ReqQryOffsetSetting,CThostFtdcQryOffsetSettingField )
};


