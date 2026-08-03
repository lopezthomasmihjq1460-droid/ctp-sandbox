#include "ThostFtdcTraderApi.h"
#include "trade_spi.h"

#define Declare2Func_nil(FuncName,type)    {{0,0,0,0},0,0,(TradeSpi_Func_0)nullptr,#FuncName}
#define Declare2Func_0(FuncName)      {{0,0,0,0},0,0,(TradeSpi_Func_0)&CThostFtdcTraderSpi::FuncName,#FuncName}
#define Declare2Func_1(FuncName,type) {{sizeof(type),0,0,0},1,0,(TradeSpi_Func_0)&CThostFtdcTraderSpi::FuncName,#FuncName}
#define Declare2Func_2(FuncName,type) {{sizeof(type),sizeof(CThostFtdcRspInfoField),0,0},2,0,(TradeSpi_Func_0)&CThostFtdcTraderSpi::FuncName,#FuncName}
#define Declare2Func_3(FuncName,type) {{sizeof(type),sizeof(CThostFtdcRspInfoField),sizeof(int),0},3,0,(TradeSpi_Func_0)&CThostFtdcTraderSpi::FuncName,#FuncName}
#define Declare2Func_4(FuncName,type) {{sizeof(type),sizeof(CThostFtdcRspInfoField),sizeof(int),2},4,0,(TradeSpi_Func_0)&CThostFtdcTraderSpi::FuncName,#FuncName}


static TradeSpi_CallbackInfo spi_callback_list[Spi_CallbackCount] = 
{
	Declare2Func_0(OnFrontConnected),
	Declare2Func_1(OnFrontDisconnected,int ),
	Declare2Func_1(OnHeartBeatWarning,int ),
	Declare2Func_4(OnRspAuthenticate,CThostFtdcRspAuthenticateField ),
	Declare2Func_4(OnRspUserLogin,CThostFtdcRspUserLoginField ),
	Declare2Func_4(OnRspUserLogout,CThostFtdcUserLogoutField ),
	Declare2Func_4(OnRspUserPasswordUpdate,CThostFtdcUserPasswordUpdateField ),
	Declare2Func_4(OnRspTradingAccountPasswordUpdate,CThostFtdcTradingAccountPasswordUpdateField ),
	Declare2Func_4(OnRspUserAuthMethod,CThostFtdcRspUserAuthMethodField ),
	Declare2Func_4(OnRspGenUserCaptcha,CThostFtdcRspGenUserCaptchaField ),
	Declare2Func_4(OnRspGenUserText,CThostFtdcRspGenUserTextField ),
	Declare2Func_4(OnRspOrderInsert,CThostFtdcInputOrderField ),
	Declare2Func_4(OnRspParkedOrderInsert,CThostFtdcParkedOrderField ),
	Declare2Func_4(OnRspParkedOrderAction,CThostFtdcParkedOrderActionField ),
	Declare2Func_4(OnRspOrderAction,CThostFtdcInputOrderActionField ),
	Declare2Func_4(OnRspQryMaxOrderVolume,CThostFtdcQryMaxOrderVolumeField ),
	Declare2Func_4(OnRspSettlementInfoConfirm,CThostFtdcSettlementInfoConfirmField ),
	Declare2Func_4(OnRspRemoveParkedOrder,CThostFtdcRemoveParkedOrderField ),
	Declare2Func_4(OnRspRemoveParkedOrderAction,CThostFtdcRemoveParkedOrderActionField ),
	Declare2Func_4(OnRspExecOrderInsert,CThostFtdcInputExecOrderField ),
	Declare2Func_4(OnRspExecOrderAction,CThostFtdcInputExecOrderActionField ),
	Declare2Func_4(OnRspForQuoteInsert,CThostFtdcInputForQuoteField ),
	Declare2Func_4(OnRspQuoteInsert,CThostFtdcInputQuoteField ),
	Declare2Func_4(OnRspQuoteAction,CThostFtdcInputQuoteActionField ),
	Declare2Func_4(OnRspBatchOrderAction,CThostFtdcInputBatchOrderActionField ),
	Declare2Func_4(OnRspOptionSelfCloseInsert,CThostFtdcInputOptionSelfCloseField ),
	Declare2Func_4(OnRspOptionSelfCloseAction,CThostFtdcInputOptionSelfCloseActionField ),
	Declare2Func_4(OnRspCombActionInsert,CThostFtdcInputCombActionField ),
	Declare2Func_4(OnRspQryOrder,CThostFtdcOrderField ),
	Declare2Func_4(OnRspQryTrade,CThostFtdcTradeField ),
	Declare2Func_4(OnRspQryInvestorPosition,CThostFtdcInvestorPositionField ),
	Declare2Func_4(OnRspQryTradingAccount,CThostFtdcTradingAccountField ),
	Declare2Func_4(OnRspQryInvestor,CThostFtdcInvestorField ),
	Declare2Func_4(OnRspQryTradingCode,CThostFtdcTradingCodeField ),
	Declare2Func_4(OnRspQryInstrumentMarginRate,CThostFtdcInstrumentMarginRateField ),
	Declare2Func_4(OnRspQryInstrumentCommissionRate,CThostFtdcInstrumentCommissionRateField ),
#ifdef CTP_6_7	
	Declare2Func_4(OnRspQryUserSession,CThostFtdcUserSessionField ),
#else
	Declare2Func_nil(OnRspQryUserSession,CThostFtdcUserSessionField ),
#endif
	Declare2Func_4(OnRspQryExchange,CThostFtdcExchangeField ),
	Declare2Func_4(OnRspQryProduct,CThostFtdcProductField ),
	Declare2Func_4(OnRspQryInstrument,CThostFtdcInstrumentField ),
	Declare2Func_4(OnRspQryDepthMarketData,CThostFtdcDepthMarketDataField ),
#ifdef CTP_6_7		
	Declare2Func_4(OnRspQryTraderOffer,CThostFtdcTraderOfferField ),
#else
	Declare2Func_nil(OnRspQryTraderOffer,CThostFtdcTraderOfferField ),
#endif

	Declare2Func_4(OnRspQrySettlementInfo,CThostFtdcSettlementInfoField ),
	Declare2Func_4(OnRspQryTransferBank,CThostFtdcTransferBankField ),
	Declare2Func_4(OnRspQryInvestorPositionDetail,CThostFtdcInvestorPositionDetailField ),
	Declare2Func_4(OnRspQryNotice,CThostFtdcNoticeField ),
	Declare2Func_4(OnRspQrySettlementInfoConfirm,CThostFtdcSettlementInfoConfirmField ),
	Declare2Func_4(OnRspQryInvestorPositionCombineDetail,CThostFtdcInvestorPositionCombineDetailField ),
	Declare2Func_4(OnRspQryCFMMCTradingAccountKey,CThostFtdcCFMMCTradingAccountKeyField ),
	Declare2Func_4(OnRspQryEWarrantOffset,CThostFtdcEWarrantOffsetField ),
	Declare2Func_4(OnRspQryInvestorProductGroupMargin,CThostFtdcInvestorProductGroupMarginField ),
	Declare2Func_4(OnRspQryExchangeMarginRate,CThostFtdcExchangeMarginRateField ),
	Declare2Func_4(OnRspQryExchangeMarginRateAdjust,CThostFtdcExchangeMarginRateAdjustField ),
	Declare2Func_4(OnRspQryExchangeRate,CThostFtdcExchangeRateField ),
	Declare2Func_4(OnRspQrySecAgentACIDMap,CThostFtdcSecAgentACIDMapField ),
	Declare2Func_4(OnRspQryProductExchRate,CThostFtdcProductExchRateField ),
	Declare2Func_4(OnRspQryProductGroup,CThostFtdcProductGroupField ),
	Declare2Func_4(OnRspQryMMInstrumentCommissionRate,CThostFtdcMMInstrumentCommissionRateField ),
	Declare2Func_4(OnRspQryMMOptionInstrCommRate,CThostFtdcMMOptionInstrCommRateField ),
	Declare2Func_4(OnRspQryInstrumentOrderCommRate,CThostFtdcInstrumentOrderCommRateField ),
	Declare2Func_4(OnRspQrySecAgentTradingAccount,CThostFtdcTradingAccountField ),
	Declare2Func_4(OnRspQrySecAgentCheckMode,CThostFtdcSecAgentCheckModeField ),
	Declare2Func_4(OnRspQrySecAgentTradeInfo,CThostFtdcSecAgentTradeInfoField ),
	Declare2Func_4(OnRspQryOptionInstrTradeCost,CThostFtdcOptionInstrTradeCostField ),
	Declare2Func_4(OnRspQryOptionInstrCommRate,CThostFtdcOptionInstrCommRateField ),
	Declare2Func_4(OnRspQryExecOrder,CThostFtdcExecOrderField ),
	Declare2Func_4(OnRspQryForQuote,CThostFtdcForQuoteField ),
	Declare2Func_4(OnRspQryQuote,CThostFtdcQuoteField ),
	Declare2Func_4(OnRspQryOptionSelfClose,CThostFtdcOptionSelfCloseField ),
	Declare2Func_4(OnRspQryInvestUnit,CThostFtdcInvestUnitField ),
	Declare2Func_4(OnRspQryCombInstrumentGuard,CThostFtdcCombInstrumentGuardField ),
	Declare2Func_4(OnRspQryCombAction,CThostFtdcCombActionField ),
	Declare2Func_4(OnRspQryTransferSerial,CThostFtdcTransferSerialField ),
	Declare2Func_4(OnRspQryAccountregister,CThostFtdcAccountregisterField ),
	Declare2Func_4(OnRspError,CThostFtdcRspInfoField ),
	Declare2Func_1(OnRtnOrder,CThostFtdcOrderField ),
	Declare2Func_1(OnRtnTrade,CThostFtdcTradeField ),

	Declare2Func_2(OnErrRtnOrderInsert,CThostFtdcInputOrderField ),
	Declare2Func_2(OnErrRtnOrderAction,CThostFtdcOrderActionField ),

	Declare2Func_1(OnRtnInstrumentStatus,CThostFtdcInstrumentStatusField ),
	Declare2Func_1(OnRtnBulletin,CThostFtdcBulletinField ),
	Declare2Func_1(OnRtnTradingNotice,CThostFtdcTradingNoticeInfoField ),
	Declare2Func_1(OnRtnErrorConditionalOrder,CThostFtdcErrorConditionalOrderField ),
	Declare2Func_1(OnRtnExecOrder,CThostFtdcExecOrderField ),

	Declare2Func_2(OnErrRtnExecOrderInsert,CThostFtdcInputExecOrderField ),
	Declare2Func_2(OnErrRtnExecOrderAction,CThostFtdcExecOrderActionField ),
	Declare2Func_2(OnErrRtnForQuoteInsert,CThostFtdcInputForQuoteField ),

	Declare2Func_1(OnRtnQuote,CThostFtdcQuoteField ),

	Declare2Func_2(OnErrRtnQuoteInsert,CThostFtdcInputQuoteField ),
	Declare2Func_2(OnErrRtnQuoteAction,CThostFtdcQuoteActionField ),

	Declare2Func_1(OnRtnForQuoteRsp,CThostFtdcForQuoteRspField ),
	Declare2Func_1(OnRtnCFMMCTradingAccountToken,CThostFtdcCFMMCTradingAccountTokenField ),

	Declare2Func_2(OnErrRtnBatchOrderAction,CThostFtdcBatchOrderActionField ),

	Declare2Func_1(OnRtnOptionSelfClose,CThostFtdcOptionSelfCloseField ),

	Declare2Func_2(OnErrRtnOptionSelfCloseInsert,CThostFtdcInputOptionSelfCloseField ),
	Declare2Func_2(OnErrRtnOptionSelfCloseAction,CThostFtdcOptionSelfCloseActionField ),
	
    Declare2Func_1(OnRtnCombAction,CThostFtdcCombActionField ),

	Declare2Func_2(OnErrRtnCombActionInsert,CThostFtdcInputCombActionField ),

	Declare2Func_4(OnRspQryContractBank,CThostFtdcContractBankField ),
	Declare2Func_4(OnRspQryParkedOrder,CThostFtdcParkedOrderField ),
	Declare2Func_4(OnRspQryParkedOrderAction,CThostFtdcParkedOrderActionField ),
	Declare2Func_4(OnRspQryTradingNotice,CThostFtdcTradingNoticeField ),
	Declare2Func_4(OnRspQryBrokerTradingParams,CThostFtdcBrokerTradingParamsField ),
	Declare2Func_4(OnRspQryBrokerTradingAlgos,CThostFtdcBrokerTradingAlgosField ),
	Declare2Func_4(OnRspQueryCFMMCTradingAccountToken,CThostFtdcQueryCFMMCTradingAccountTokenField ),

	Declare2Func_1(OnRtnFromBankToFutureByBank,CThostFtdcRspTransferField ),
	Declare2Func_1(OnRtnFromFutureToBankByBank,CThostFtdcRspTransferField ),
	Declare2Func_1(OnRtnRepealFromBankToFutureByBank,CThostFtdcRspRepealField ),
	Declare2Func_1(OnRtnRepealFromFutureToBankByBank,CThostFtdcRspRepealField ),
	Declare2Func_1(OnRtnFromBankToFutureByFuture,CThostFtdcRspTransferField ),
	Declare2Func_1(OnRtnFromFutureToBankByFuture,CThostFtdcRspTransferField ),
	Declare2Func_1(OnRtnRepealFromBankToFutureByFutureManual,CThostFtdcRspRepealField ),
	Declare2Func_1(OnRtnRepealFromFutureToBankByFutureManual,CThostFtdcRspRepealField ),
	Declare2Func_1(OnRtnQueryBankBalanceByFuture,CThostFtdcNotifyQueryAccountField ),

	Declare2Func_2(OnErrRtnBankToFutureByFuture,CThostFtdcReqTransferField ),
	Declare2Func_2(OnErrRtnFutureToBankByFuture,CThostFtdcReqTransferField ),
	Declare2Func_2(OnErrRtnRepealBankToFutureByFutureManual,CThostFtdcReqRepealField ),
	Declare2Func_2(OnErrRtnRepealFutureToBankByFutureManual,CThostFtdcReqRepealField ),
	Declare2Func_2(OnErrRtnQueryBankBalanceByFuture,CThostFtdcReqQueryAccountField ),

	Declare2Func_1(OnRtnRepealFromBankToFutureByFuture,CThostFtdcRspRepealField ),
	Declare2Func_1(OnRtnRepealFromFutureToBankByFuture,CThostFtdcRspRepealField ),

	Declare2Func_4(OnRspFromBankToFutureByFuture,CThostFtdcReqTransferField ),
	Declare2Func_4(OnRspFromFutureToBankByFuture,CThostFtdcReqTransferField ),
	Declare2Func_4(OnRspQueryBankAccountMoneyByFuture,CThostFtdcReqQueryAccountField ),

	Declare2Func_1(OnRtnOpenAccountByBank,CThostFtdcOpenAccountField ),
	Declare2Func_1(OnRtnCancelAccountByBank,CThostFtdcCancelAccountField ),
	Declare2Func_1(OnRtnChangeAccountByBank,CThostFtdcChangeAccountField ),

	Declare2Func_4(OnRspQryClassifiedInstrument,CThostFtdcInstrumentField ),
	Declare2Func_4(OnRspQryCombPromotionParam,CThostFtdcCombPromotionParamField ),
	Declare2Func_4(OnRspQryRiskSettleInvstPosition,CThostFtdcRiskSettleInvstPositionField ),
	Declare2Func_4(OnRspQryRiskSettleProductStatus,CThostFtdcRiskSettleProductStatusField ),

#ifdef CTP_6_7	
	Declare2Func_4(OnRspQrySPBMFutureParameter,CThostFtdcSPBMFutureParameterField ),
	Declare2Func_4(OnRspQrySPBMOptionParameter,CThostFtdcSPBMOptionParameterField ),
	Declare2Func_4(OnRspQrySPBMIntraParameter,CThostFtdcSPBMIntraParameterField ),
	Declare2Func_4(OnRspQrySPBMInterParameter,CThostFtdcSPBMInterParameterField ),
	Declare2Func_4(OnRspQrySPBMPortfDefinition,CThostFtdcSPBMPortfDefinitionField ),
	Declare2Func_4(OnRspQrySPBMInvestorPortfDef,CThostFtdcSPBMInvestorPortfDefField ),
	Declare2Func_4(OnRspQryInvestorPortfMarginRatio,CThostFtdcInvestorPortfMarginRatioField ),
	Declare2Func_4(OnRspQryInvestorProdSPBMDetail,CThostFtdcInvestorProdSPBMDetailField ),
	Declare2Func_4(OnRspQryInvestorCommoditySPMMMargin,CThostFtdcInvestorCommoditySPMMMarginField ),
	Declare2Func_4(OnRspQryInvestorCommodityGroupSPMMMargin,CThostFtdcInvestorCommodityGroupSPMMMarginField ),
	Declare2Func_4(OnRspQrySPMMInstParam,CThostFtdcSPMMInstParamField ),
	Declare2Func_4(OnRspQrySPMMProductParam,CThostFtdcSPMMProductParamField ),
	Declare2Func_4(OnRspQrySPBMAddOnInterParameter,CThostFtdcSPBMAddOnInterParameterField ),
	Declare2Func_4(OnRspQryRCAMSCombProductInfo,CThostFtdcRCAMSCombProductInfoField ),
	Declare2Func_4(OnRspQryRCAMSInstrParameter,CThostFtdcRCAMSInstrParameterField ),
	Declare2Func_4(OnRspQryRCAMSIntraParameter,CThostFtdcRCAMSIntraParameterField ),
	Declare2Func_4(OnRspQryRCAMSInterParameter,CThostFtdcRCAMSInterParameterField ),
	Declare2Func_4(OnRspQryRCAMSShortOptAdjustParam,CThostFtdcRCAMSShortOptAdjustParamField ),
	Declare2Func_4(OnRspQryRCAMSInvestorCombPosition,CThostFtdcRCAMSInvestorCombPositionField ),
	Declare2Func_4(OnRspQryInvestorProdRCAMSMargin,CThostFtdcInvestorProdRCAMSMarginField ),
	Declare2Func_4(OnRspQryRULEInstrParameter,CThostFtdcRULEInstrParameterField ),
	Declare2Func_4(OnRspQryRULEIntraParameter,CThostFtdcRULEIntraParameterField ),
	Declare2Func_4(OnRspQryRULEInterParameter,CThostFtdcRULEInterParameterField ),
	Declare2Func_4(OnRspQryInvestorProdRULEMargin,CThostFtdcInvestorProdRULEMarginField ),
	Declare2Func_4(OnRspQryInvestorPortfSetting,CThostFtdcInvestorPortfSettingField ),
	Declare2Func_4(OnRspQryInvestorInfoCommRec,CThostFtdcInvestorInfoCommRecField ),
	Declare2Func_4(OnRspQryCombLeg,CThostFtdcCombLegField ),
	Declare2Func_4(OnRspOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_4(OnRspCancelOffsetSetting,CThostFtdcInputOffsetSettingField ),

	Declare2Func_1(OnRtnOffsetSetting,CThostFtdcOffsetSettingField ),
	Declare2Func_2(OnErrRtnOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_2(OnErrRtnCancelOffsetSetting,CThostFtdcCancelOffsetSettingField ),
    
	Declare2Func_4(OnRspQryOffsetSetting,CThostFtdcOffsetSettingField ),
#else
	Declare2Func_nil(OnRspQrySPBMFutureParameter,CThostFtdcSPBMFutureParameterField ),
	Declare2Func_nil(OnRspQrySPBMOptionParameter,CThostFtdcSPBMOptionParameterField ),
	Declare2Func_nil(OnRspQrySPBMIntraParameter,CThostFtdcSPBMIntraParameterField ),
	Declare2Func_nil(OnRspQrySPBMInterParameter,CThostFtdcSPBMInterParameterField ),
	Declare2Func_nil(OnRspQrySPBMPortfDefinition,CThostFtdcSPBMPortfDefinitionField ),
	Declare2Func_nil(OnRspQrySPBMInvestorPortfDef,CThostFtdcSPBMInvestorPortfDefField ),
	Declare2Func_nil(OnRspQryInvestorPortfMarginRatio,CThostFtdcInvestorPortfMarginRatioField ),
	Declare2Func_nil(OnRspQryInvestorProdSPBMDetail,CThostFtdcInvestorProdSPBMDetailField ),
	Declare2Func_nil(OnRspQryInvestorCommoditySPMMMargin,CThostFtdcInvestorCommoditySPMMMarginField ),
	Declare2Func_nil(OnRspQryInvestorCommodityGroupSPMMMargin,CThostFtdcInvestorCommodityGroupSPMMMarginField ),
	Declare2Func_nil(OnRspQrySPMMInstParam,CThostFtdcSPMMInstParamField ),
	Declare2Func_nil(OnRspQrySPMMProductParam,CThostFtdcSPMMProductParamField ),
	Declare2Func_nil(OnRspQrySPBMAddOnInterParameter,CThostFtdcSPBMAddOnInterParameterField ),
	Declare2Func_nil(OnRspQryRCAMSCombProductInfo,CThostFtdcRCAMSCombProductInfoField ),
	Declare2Func_nil(OnRspQryRCAMSInstrParameter,CThostFtdcRCAMSInstrParameterField ),
	Declare2Func_nil(OnRspQryRCAMSIntraParameter,CThostFtdcRCAMSIntraParameterField ),
	Declare2Func_nil(OnRspQryRCAMSInterParameter,CThostFtdcRCAMSInterParameterField ),
	Declare2Func_nil(OnRspQryRCAMSShortOptAdjustParam,CThostFtdcRCAMSShortOptAdjustParamField ),
	Declare2Func_nil(OnRspQryRCAMSInvestorCombPosition,CThostFtdcRCAMSInvestorCombPositionField ),
	Declare2Func_nil(OnRspQryInvestorProdRCAMSMargin,CThostFtdcInvestorProdRCAMSMarginField ),
	Declare2Func_nil(OnRspQryRULEInstrParameter,CThostFtdcRULEInstrParameterField ),
	Declare2Func_nil(OnRspQryRULEIntraParameter,CThostFtdcRULEIntraParameterField ),
	Declare2Func_nil(OnRspQryRULEInterParameter,CThostFtdcRULEInterParameterField ),
	Declare2Func_nil(OnRspQryInvestorProdRULEMargin,CThostFtdcInvestorProdRULEMarginField ),
	Declare2Func_nil(OnRspQryInvestorPortfSetting,CThostFtdcInvestorPortfSettingField ),
	Declare2Func_nil(OnRspQryInvestorInfoCommRec,CThostFtdcInvestorInfoCommRecField ),
	Declare2Func_nil(OnRspQryCombLeg,CThostFtdcCombLegField ),
	Declare2Func_nil(OnRspOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_nil(OnRspCancelOffsetSetting,CThostFtdcInputOffsetSettingField ),

	Declare2Func_nil(OnRtnOffsetSetting,CThostFtdcOffsetSettingField ),
	Declare2Func_nil(OnErrRtnOffsetSetting,CThostFtdcInputOffsetSettingField ),
	Declare2Func_nil(OnErrRtnCancelOffsetSetting,CThostFtdcCancelOffsetSettingField ),
    
	Declare2Func_nil(OnRspQryOffsetSetting,CThostFtdcOffsetSettingField ),
#endif	
};


TradeSpi_CallbackInfo * g_spi_callback_list = spi_callback_list;