#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "flow_control.h"


void Init_FlowControl(FlowControl_Data * data)
{
    data->time_flag = 0;
    data->limit_cnt = 0;
    data->period_ms = 0;
    data->ptime_head = 0;
    data->over = 0;
}

void UnInit_FlowControl(FlowControl_Data * data)
{
    if( !data->time_flag )
        return;

    free(data->time_flag);
    data->time_flag = 0;
}

int  Set_FlowControl(FlowControl_Data * data,long period_ms,long limit_cnt)
{
    data->period_ms = period_ms;
    if( data->over < limit_cnt)
        data->over = limit_cnt;

    if( data->time_flag == 0 )
    {
        //初始化设置
        unsigned long long * time_flag = (unsigned long long *)malloc(sizeof(long long)*limit_cnt);
        memset(time_flag,0,sizeof(long long)*limit_cnt);
        data->time_flag = time_flag;
        data->limit_cnt = limit_cnt;
        data->ptime_head = data->time_flag;
        
        return limit_cnt;
    }

    //动态实时调整设置
    if( limit_cnt == data->limit_cnt )
        return limit_cnt; //仅仅变更周期

    int offset = -1;
    offset = data->ptime_head - data->time_flag;

    //流量改变，修改窗口值
    int i;
    unsigned long long * time_flag = (unsigned long long *)malloc(sizeof(long long)*limit_cnt);
    if( limit_cnt > data->limit_cnt )
    {//扩大
        if( offset <= 0 )
        {
            memcpy(time_flag,data->time_flag,sizeof(long long ) * data->limit_cnt);
        }
        else
        {
            memcpy(time_flag,data->time_flag + offset,sizeof(long long ) * (data->limit_cnt - offset));
            memcpy(time_flag + data->limit_cnt - offset,data->time_flag,offset * sizeof(long long ));
        }
        for(i= data->limit_cnt; i< limit_cnt;i++ )
        {
            time_flag[i] = 0;
        }
    }
    else 
    { //缩减
        if( offset <= 0 )
        {
            memcpy(time_flag,data->time_flag + (data->limit_cnt - limit_cnt ),sizeof(long long ) * (limit_cnt));
        }
        else
        {
            int offset2 = offset + data->limit_cnt - limit_cnt;
            if( offset2 > data->limit_cnt )
            {
                offset2 -= data->limit_cnt;
                memcpy(time_flag,data->time_flag + offset2,sizeof(long long ) * (limit_cnt));
            }
            else
            {
                memcpy(time_flag,data->time_flag + offset2,sizeof(long long ) * (data->limit_cnt - offset2));
                memcpy(time_flag + data->limit_cnt - offset2,data->time_flag ,offset*sizeof(long long ));
            }
        }
    }
    free(data->time_flag);
    data->time_flag = time_flag;
    data->limit_cnt = limit_cnt;
    data->ptime_head = data->time_flag;

    if( offset < 0 )
        return limit_cnt;

    return limit_cnt;
}

#ifdef WIN32
#include <windows.h>
#endif

unsigned long long my_GetTickCount()
{
#if defined(WIN32)
static LARGE_INTEGER TicksPerSecond = { 0 };

	LARGE_INTEGER Tick;
	if (!TicksPerSecond.QuadPart)
		QueryPerformanceFrequency(&TicksPerSecond);
	QueryPerformanceCounter(&Tick);
	__int64 Seconds = Tick.QuadPart / TicksPerSecond.QuadPart;
	__int64 LeftPart = Tick.QuadPart - (TicksPerSecond.QuadPart*Seconds);
	__int64 MillSeconds = LeftPart * 1000 / TicksPerSecond.QuadPart;
	__int64 Ret = Seconds * 1000 + MillSeconds;
	return Ret;
#else
//	struct timeval tv;
//	gettimeofday(&tv, NULL);
//	return (((unsigned long long )tv.tv_sec) * 1000 + ((unsigned long long)tv.tv_usec) / 1000);
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC,&tv); //以系统的绝对运行时间为准，不与系统时间挂钩
	return tv.tv_sec*1000 + (tv.tv_nsec/1000000LL);
#endif
	return 0;
}

int Check_FlowControl(FlowControl_Data * data)
{
    if( !data->time_flag)
        return -1; //未初始化阈值

    unsigned long long ms = my_GetTickCount();
    unsigned long long diff = ms - *data->ptime_head;
    if( diff <= data->period_ms )
    {
        data->over++;
        return 0;//超过流控阈值，拒绝
    }

    *data->ptime_head = ms;
    data->ptime_head++;
    if( data->ptime_head >= (data->time_flag + data->limit_cnt ) )
        data->ptime_head = data->time_flag;
    //流控未超标，允许通过
    data->over = data->limit_cnt;
    return 1;
}