# ==功能描述==
    为了满足算法平台对接，提供一个安全的隔离环境来使用CTP账户，提供如下安全机制
    1. 虚拟账号映射，把CTP柜台账户映射为一个自定义的账户和密码，算法平台只能使用该虚拟账号，保障CTP原始账号安全，包括账户密码修改的独立性
    2. 交易品种控制，包括白名单/黑名单模式
    3. 自成交防范
    4. 报撤单流控

# ==使用方法==
本软件分成客户端和服务器端

## 客户端
   任意基于CTP api开发的客户端或者程序化软件，只要替换对应程序的thosttraderapi_se.so，为本软件相应代码版本即可
   本程序默认提供 基于6.6.1_P的编译二进制版本（dist/api/debug|release|win32|win64），其他版本请自行编译

## 服务器配置
**==ctp_sandbox.db为sqlite数据库==**
  请使用sqlite工具编辑相关配置
  配置完成后，执行ctp_sandbox.exe启动服务器

### 网络配置：
```
   replace into t_cfg values('listen_ip','127.0.0.1');  
   replace into t_cfg values('listen_port','30001');
```
### APP id
```
-- 使用的appid 
replace into t_cfg values('app_id','simnow_client_test');
--**使用的auth code**
replace into t_cfg values('auth_code','0000000000000000');
```
### **==账户映射==**
```
请根据实际情况，配置如下账户映射关系和对应的账号密码

---无需品种控制，无流控
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',0,0,0);
---品种白名单控制，无流控
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',0,0,1);
---品种黑名单控制，无流控
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',0,0,2);
---无需品种控制，流控 每秒 10笔
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',10,1000,0);
---品种白名单控制，流控 每秒 10笔
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',10,1000,1);
---品种黑名单控制，流控 每秒 10笔
replace into t_account values('虚拟账户','虚拟账户密码','CTP柜台broker_id','CTP柜台账户','CTP柜台账户密码',10,1000,2);
```

### **==品种黑白名单==**
```
replace into t_user_product values('虚拟账户','品种id');
```

### CTP柜台地址
```
replace into t_broker values('broker id','前置地址',null,null,null,null);

例如 simnow仿真柜台：
replace into t_broker values('9999','tcp://182.254.243.31:30001',null,null,null,null);
```

### 详细内容请参考 config/db.sql


# 源码结构
```
 src是本软件的源码目录，包含所有必要的文件和目录
 vs32 是用于编译 win32 版本 thosttraderapi_se.so的脚本
 vs64 是用于编译 win64 版本 thosttraderapi_se.so的脚本
 Makefile.api 用于编译linux版本的thosttraderapi_se.so
 Makefile.server 用于编译 linux版本的后台程序
 Makefile.win64 用于编译 windows 64bit 版本的后台程序
 Makefile 用于编译所有linux版本的thosttraderapi_se.so和后台程序

 dist/api/debug         //thosttraderapi_se.so linux  debug
 dist/api/release       //thosttraderapi_se.so linux  release
 dist/api/vs32          //thosttraderapi_se.so win32
 dist/api/vs64          //thosttraderapi_se.so win64 

 dist/server/debug         //后台程序 linux  debug
 dist/server/release       //后台程序 linux  release
 dist/server/win64         //后台程序 win32
```

