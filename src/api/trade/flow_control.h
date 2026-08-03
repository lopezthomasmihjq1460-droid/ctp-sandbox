#pragma once


typedef struct FlowControl_Data
{
    unsigned long long * time_flag;
    long        limit_cnt;
    long        period_ms;
    unsigned long long * ptime_head;
    long        over;
}FlowControl_Data;

void Init_FlowControl(FlowControl_Data * data);

void UnInit_FlowControl(FlowControl_Data * data);

int  Set_FlowControl(FlowControl_Data * data,long period_ms,long limit_cnt);

int Check_FlowControl(FlowControl_Data * data);