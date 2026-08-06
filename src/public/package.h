#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#define TradeApi_Buffer 2048
typedef struct TradeApi_Header
{
    unsigned int total_len;
    unsigned short func_id;
    unsigned short p_cnt;
}TradeApi_Header;

#define TradeApi_Header_Size sizeof(TradeApi_Header)

typedef union TradeApi_Package
{
    TradeApi_Header header;
    char data[TradeApi_Buffer];    
}TradeApi_Package;

typedef struct TradeApi_PackageTask
{
    bufferevent * bev;
    TradeApi_Package package;
}TradeApi_PackageTask;

typedef struct TradeApi_Param
{
    unsigned short len;
    char * ptr;
}TradeApi_Param;

inline void Init_TradeApi_Package(TradeApi_Package * package,int func_id)
{
    package->header.total_len = TradeApi_Header_Size;
    package->header.func_id = func_id;
    package->header.p_cnt = 0;
}

#define Append_TradeApi_Package_Ptr(package,pStruct) \
do \
{\
    unsigned short p_len = 0;\
    if( pStruct )\
        p_len = sizeof(*pStruct);\
    if( (package.header.total_len +p_len + sizeof(p_len)) > TradeApi_Buffer )\
        break;\
    memcpy(package.data + package.header.total_len,&p_len,sizeof(p_len));\
    package.header.total_len += sizeof(p_len);\
    if( p_len > 0 )\
    {\
        memcpy(package.data + package.header.total_len,pStruct, p_len);\
        package.header.total_len += p_len;\
    }\
    package.header.p_cnt ++;\
} while (0)

#define Append_TradeApi_Package_Val(package,val) \
do\
{\
    unsigned short p_len = sizeof(val);\
    if( (package.header.total_len +p_len + sizeof(p_len)) > TradeApi_Buffer )\
        break;\
    memcpy(package.data + package.header.total_len,&p_len,sizeof(p_len));\
    package.header.total_len += sizeof(p_len);\
    memcpy(package.data + package.header.total_len,(const char *)&val, p_len);\
    package.header.total_len += p_len;\
    package.header.p_cnt ++;\
} while (0);

static void write_task_cb(evutil_socket_t fd, short what, void *arg)
{
    TradeApi_PackageTask *task = (TradeApi_PackageTask *)arg;
    bufferevent_write(task->bev, task->package.data, task->package.header.total_len);
    delete task;
}

extern struct event_base *g_base;


#define send_task_data write_task_cb


#define TradeApi_CallFunc(func_name)\
do\
{\
    bufferevent * bev = connect_bev(m_data->netCtx);\
    if( !bev )\
        break;\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = bev;\
    Init_TradeApi_Package(&task->package,Api_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pReqField);\
    Append_TradeApi_Package_Val(task->package,nRequestID);\
    send_task_data(0,0,task);\
} while (0)

//event_base_once(m_data->base, -1, EV_TIMEOUT, write_task_cb, task, NULL);\

#define TradeApi_CallFuncRet(func_name)\
TradeApi_CallFunc(func_name);\
return 0;


#define TradeApi_CallFuncSimple(func_name)\
do\
{\
    bufferevent * bev = connect_bev(m_data->netCtx);\
    if( !bev )\
        break;\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = bev;\
    Init_TradeApi_Package(&task->package,Api_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pReqField);\
    send_task_data(0,0,task);\
} while (0)

////event_base_once(m_data->base, -1, EV_TIMEOUT, write_task_cb, task, NULL);\

#define TradeApi_CallFuncSimpleRet(func_name)\
TradeApi_CallFuncSimple(func_name);\
return 0;



#define TradeSpi_Response(func_name) \
do \
{\
    short last_flag = bIsLast;\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = m_data->bev;\
    Init_TradeApi_Package(&task->package,Spi_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pRspField);\
    Append_TradeApi_Package_Ptr(task->package,pRspInfo);\
    Append_TradeApi_Package_Val(task->package,nRequestID);\
    Append_TradeApi_Package_Val(task->package,last_flag);\
    send_task_data(0,0,task);\
} while (0);


#define TradeSpi_RspError(func_name) \
do \
{\
    short last_flag = bIsLast;\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = m_data->bev;\
    Init_TradeApi_Package(&task->package,Spi_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pRspField);\
    Append_TradeApi_Package_Val(task->package,nRequestID);\
    Append_TradeApi_Package_Val(task->package,last_flag);\
    send_task_data(0,0,task);\
} while (0);


#define TradeSpi_Rtn(func_name) \
do \
{\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = m_data->bev;\
    Init_TradeApi_Package(&task->package,Spi_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pRspField);\
    send_task_data(0,0,task);\
} while (0);

#define TradeSpi_RtnVal(func_name,val) \
do \
{\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = m_data->bev;\
    Init_TradeApi_Package(&task->package,Spi_##func_name);\
    Append_TradeApi_Package_Val(task->package,val);\
    send_task_data(0,0,task);\
} while (0);


#define TradeSpi_RtnErr(func_name) \
do \
{\
    TradeApi_PackageTask *task = new TradeApi_PackageTask;\
    task->bev = m_data->bev;\
    Init_TradeApi_Package(&task->package,Spi_##func_name);\
    Append_TradeApi_Package_Ptr(task->package,pRspField);\
    Append_TradeApi_Package_Ptr(task->package,pRspInfo);\
    send_task_data(0,0,task);\
} while (0);



