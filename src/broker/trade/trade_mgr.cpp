#include <string.h>
#include <stdio.h>

#include "trade_mgr.h"

#include <sqlite3.h>

#ifdef WIN32
		#include <windows.h>
		#include <io.h>
		#include <direct.h>
		#include <stdlib.h>
		#include <sys/stat.h>
		#include <sys/types.h>
		#include <fcntl.h>
		#include <stdio.h>

	#define RTLD_LAZY	0x00001	/* Lazy function call binding.  */
	#define RTLD_NOW	0x00002	/* Immediate function call binding.  */
	#define RTLD_GLOBAL	0x00100
	#define RTLD_GLOBAL	0x00100
	#define api_dlopen(name,flag) LoadLibrary(name)
	#define api_dlsym(handle,fname) GetProcAddress((HMODULE)handle,fname)
	#define api_dlclose(handle) FreeLibrary((HMODULE)handle)
    #define api_mkdir(path,mode) mkdir(path)

    const char * ctp_create_func[2] =
    {
        "?CreateFtdcTraderApi@CThostFtdcTraderApi@@SAPEAV1@PEBD@Z",
        "?CreateFtdcTraderApi@CThostFtdcTraderApi@@SAPEAV1@PEBD_N@Z"
    } ;
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <fcntl.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <dlfcn.h>

	#define api_dlopen(name,flag) dlopen(name,flag)
	#define api_dlsym(handle,fname) dlsym(handle,fname)
	#define api_dlclose(handle) (dlclose(handle) == 0)
    #define api_mkdir(path,mode)			mkdir(path,mode)

    const char * ctp_create_func[2] =
    {
        "_ZN19CThostFtdcTraderApi19CreateFtdcTraderApiEPKc",
        "_ZN19CThostFtdcTraderApi19CreateFtdcTraderApiEPKcb"
    } ;
#endif

extern sqlite3 *  g_cfg_db ;
TradeMgr * g_ctp_mgr = 0;

TradeMgr::TradeMgr()
{
    dll_handler = 0;
    api_ver = 0;

	stmt_broker = 0;
	if( g_cfg_db )
	{
		sqlite3_prepare_v2(g_cfg_db, "SELECT broker_id, trade_front1 FROM t_broker WHERE broker_id = ?", -1, &stmt_broker, 0);
	}

}

TradeMgr::~TradeMgr()
{
    UnInit();
}

int TradeMgr::Init()
{
#ifdef WIN32	
    dll_handler = api_dlopen("./thosttraderapi_se.dll",RTLD_LAZY);
#else
	dll_handler = api_dlopen("./thosttraderapi_se.so",RTLD_LAZY);
#endif
    if( !dll_handler )
	{
		printf("load thosttraderapi_se.so error\n");
        return 0;
	}

	for( api_ver =1; api_ver >= 0; api_ver --)
	{
		create_func.func = (void *)api_dlsym(dll_handler, ctp_create_func[api_ver]);
		if( create_func.func )
		{
			break;
		}
	}

    if( !create_func.func )
	{
		printf("thosttraderapi_se.so create function not find\n");
        return 0;
	}

	if( !g_cfg_db )
		return 1;



    return 1;
}

void TradeMgr::UnInit()
{
    if( dll_handler )
        api_dlclose(dll_handler);
    dll_handler = 0;
	if( stmt_broker )
		sqlite3_finalize(stmt_broker);
}

#define PATH_MAX_SIZE 512

int mkdir_dir_each(const char * filename,int mode)
{
	const char * pdir = filename;
	char tempdir[PATH_MAX_SIZE] ;

	if( (pdir[0] == '\\') && (pdir[1] == '\\') ) //¡¤,for windows
	{
		pdir = strchr(filename,'$');
		if(!pdir)
			return -1;
	}
	while(*pdir)
	{
		if( (*pdir == '\\') || (*pdir == '/') )
		{
			if( (pdir - filename) >= (PATH_MAX_SIZE -1 ))
				break;
			strncpy(tempdir,filename,pdir - filename);
			tempdir[pdir-filename] = '/';
			tempdir[pdir-filename+1] = 0;
			if(access(tempdir,00) != 0)
				api_mkdir(tempdir,mode);
		}
		pdir++;
	}
	return 0;
}

CThostFtdcTraderApi * TradeMgr::CreateApi(const char * tmp_dir)
{
    if( !create_func.func )
        return 0;

    mkdir_dir_each(tmp_dir,00750);
	CThostFtdcTraderApi * api = 0;
	if( api_ver == 0 )
		api = create_func.v1( tmp_dir );
	else
		api = create_func.v2( tmp_dir,true);
    return api;
}

const char *  TradeMgr::GetFrontAddr(const char * broker_id, char front_addr_buffer[128])
{
	front_addr_buffer[0] = 0;
	if( !stmt_broker )
		return 0;
		
	sqlite3_bind_text(stmt_broker, 1, broker_id, -1, SQLITE_STATIC);
	sqlite3_step(stmt_broker);
	const char * addr = (const char *)sqlite3_column_text(stmt_broker, 1);
	if( !addr )
	{
		sqlite3_reset( stmt_broker );
		return 0;
	}
	
	strncpy(front_addr_buffer,addr,127);
	sqlite3_reset( stmt_broker );

	return front_addr_buffer;
}