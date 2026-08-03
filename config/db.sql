-- 配置表

CREATE TABLE IF NOT EXISTS t_cfg(
    id VARCHAR(128) PRIMARY KEY not null, -- 配置项
    vals VARCHAR(512) null  -- 值
);

replace into t_cfg values('listen_ip','127.0.0.1');
replace into t_cfg values('listen_port','30001');
replace into t_cfg values('app_id','simnow_client_test');
replace into t_cfg values('auth_code','0000000000000000');


CREATE TABLE IF NOT EXISTS t_account(
    id VARCHAR(128) PRIMARY KEY not null, -- 用户id
    pwd01 VARCHAR(128) null,  -- 用户密码
    broker_id VARCHAR(128) null,  -- 经纪商id
    account VARCHAR(128) null,  -- 经纪商账户
    pwd02 VARCHAR(128) null,  -- 经纪商账户密码
    flowctrl int null default 100000,  -- 流控阈值
    flowperiod int null default 0,  -- 流控周期,单位 ms ,当周期为0,表示不开启流控
    product_ctrl int null default 0  -- 品种权限控制,0:不控制,1:白名单，2:黑名单
);

--replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',10,1000,1);

CREATE TABLE IF NOT EXISTS t_broker(
    broker_id VARCHAR(128) PRIMARY KEY not null, -- 用户id
    trade_front1 VARCHAR(128) null,  -- 经纪商前端地址
    trade_front2 VARCHAR(128) null,  -- 经纪商前端地址
    trade_front3 VARCHAR(128) null,  -- 经纪商前端地址
    trade_front4 VARCHAR(128) null,  -- 经纪商前端地址
    trade_front5 VARCHAR(128) null   -- 经纪商前端地址
);

replace into t_broker values('9999','tcp://182.254.243.31:30001',null,null,null,null);

-- 品种黑白名单
CREATE TABLE IF NOT EXISTS t_user_product(
    user_id VARCHAR(128) PRIMARY KEY not null, -- 用户id
    product_id VARCHAR(128) null  -- 品种id
);

replace into t_user_product values('test01','jm');