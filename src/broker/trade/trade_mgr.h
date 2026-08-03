#pragma once

#include <sqlite3.h>

class CThostFtdcTraderApi;
typedef CThostFtdcTraderApi * (*pfCreateTradeApi)(const char * );
typedef CThostFtdcTraderApi * (*pfCreateTradeApiV2)(const char * ,bool blsProductionMode);

class TradeMgr
{
public:
    TradeMgr();
    virtual ~TradeMgr();

    int Init();
    void UnInit();

    CThostFtdcTraderApi * CreateApi(const char * tmp_dir = "./tmpConn/");

	const char *  GetFrontAddr(const char * broker_id, char front_addr_buffer[128]);//通过broker_id获取前置地址

private:
    void * dll_handler;

	union
	{
		void * func;
		pfCreateTradeApi v1;
		pfCreateTradeApiV2 v2;
	}create_func;
	int api_ver;

	sqlite3_stmt *stmt_broker ;

};

extern TradeMgr * g_ctp_mgr;