#ifdef WIN32
	#define patch_API __declspec(dllexport)
#else
	#define patch_API __attribute__((visibility("default"))) 
#endif

class patch_API CThostFtdcTraderApi
{
public:
	static CThostFtdcTraderApi *CreateFtdcTraderApi(const char *pszFlowPath, bool);
	static CThostFtdcTraderApi *CreateFtdcTraderApi(const char *pszFlowPath );
};

CThostFtdcTraderApi * do_create_api();

patch_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath, bool bIsProductionMode )
{
    return do_create_api();
}

patch_API CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *pszFlowPath )
{
    return do_create_api();
}
