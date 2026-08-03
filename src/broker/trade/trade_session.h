#pragma once
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include <unordered_map>
#include <string>

#include "ThostFtdcTraderApi.h"
#include "package.h"
#include "flow_control.h"
#include <sqlite3.h>

typedef struct PackageData
{
	CThostFtdcTraderApi * api;
	TradeApi_Package package; //接收数据缓存
	int package_len; //接收数据长度

	TradeApi_Param param[8];
}PackageData;

typedef struct TradeSessionNetData
{
    union
    {
        bufferevent * bev; //网络对象
        bufferevent * netCtx; //网络对象
    };
    
    event * evt; //事件对象 ,内部同步事件
}TradeSessionNetData;


// 多成员组合唯一Key
struct SessionKey
{
    std::string ins;
    std::string OrderSysID;
    int dir;

    // 相等判断：全部成员一致才算同一个key
    bool operator==(const SessionKey& other) const
    {
        return ins == other.ins && OrderSysID == other.OrderSysID && dir == other.dir;
    }
};

// 给自定义Key提供哈希函数
namespace std
{
    template<>
    struct hash<SessionKey>
    {
        size_t operator()(const SessionKey& key) const noexcept
        {
            // 分步算出各个成员的哈希值
            size_t h_dir = hash<int>{}(key.dir);
            size_t h_order_id = hash<std::string>{}(key.OrderSysID);
            size_t h_ins = hash<std::string>{}(key.ins);

            // 哈希混合算法（稳妥不易碰撞写法）
            size_t res = h_dir;
            res ^= h_order_id + 0x9e3779b9 + (res << 6) + (res >> 2);
            res ^= h_ins + 0x9e3779b9 + (res << 6) + (res >> 2);
            return res;
        }
    };
}

struct OrderInfo
{
    double price;
    int dir;
};

using SessionTable = std::unordered_map<SessionKey, OrderInfo>;

class TradeSession
{
public:
    TradeSession();
    ~TradeSession();

	int Start(const char * front_addr);

	void OnClientData();
    void CloseClient();

public:
	void SubscribePrivateTopic(THOST_TE_RESUME_TYPE nResumeType) ;
	void SubscribePublicTopic(THOST_TE_RESUME_TYPE nResumeType) ;
	int ReqAuthenticate(CThostFtdcReqAuthenticateField *pReqAuthenticateField, int nRequestID) ;
    int ReqUserLogin(CThostFtdcReqUserLoginField *pReqUserLoginField, int nRequestID) ;
    int ReqOrderInsert(CThostFtdcInputOrderField *pInputOrder, int nRequestID);

public:

    TradeSessionNetData net;

    std::string broker_id;
    std::string user_id;
    int requestId;
    int loginRequestId;

    CThostFtdcTraderApi * m_api;
    short has_started;


    CThostFtdcReqUserLoginField loginField;
    int action_on_connected;

    THOST_TE_RESUME_TYPE SubscribePrivateTopic_flag; //THOST_TERT_NONE
    THOST_TE_RESUME_TYPE SubscribePublicTopic_flag;
    PackageData m_data;
    FlowControl_Data flow_control;

    void OnRtnOrder(CThostFtdcOrderField *pRspField);

    struct
    {
        std::string raw_user;
        std::string raw_broker;

        std::string broker_id;
        std::string account;
        std::string pwd02;
        int flowctrl;
        int flowperiod;
        int product_ctrl;
    }broker;

    char buffer_ext[2048];
private:

    CThostFtdcTraderSpi * m_spi;

    int ReadPackage(struct evbuffer *in_buf,PackageData * data);
    bool CheckProductPermission(const char * product_id);
    bool CheckSelfDeal(const char * instrument_id,double price,int dir);
    
	SessionTable order_map;

    sqlite3_stmt *stmt_product_permission;

};

