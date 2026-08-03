@echo off

set PRO_NAME=thosttraderapi_se

:: 宏定义
set MACROS=/DWIN32 /DLIB_TRADER_API_EXPORT /DISLIB /DWIN32_LEAN_AND_MEAN 

::/DWINVER=0502 /D_WIN32_WINNT=0x0502

chcp 65001
set PYTHONIOENCODING=
set OUTPUT_ENCODING=
setlocal enabledelayedexpansion

:: 初始化VS2022 32编译环境
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"

:: 目录配置

set OBJ_DIR=..\dist\obj32\
set OUT_DIR=..\dist\api\vs32\
set TARGET_DLL=%OUT_DIR%\%PRO_NAME%.dll
set TARGET_PDB=%OUT_DIR%\%PRO_NAME%.pdb
set RSP_FILE=%PRO_NAME%.rsp

:: 建立文件夹
::if not exist obj md obj
if not exist %OBJ_DIR% md %OBJ_DIR%
if not exist %OUT_DIR% md %OUT_DIR%

set INCLUDE_DIR=/I ../src/api/net /I../src/api/trade /I ../src/public /I../depend/event/include /I ..\depend\ctp\ThostApi6.6.1P\include

set LIBS=../depend/event/lib/vs32/event.lib  ws2_32.lib  advapi32.lib iphlpapi.lib

::../depend/event/lib/vs32/event_core.lib ../depend/event/lib/vs32/event_extra.lib

echo. > %PRO_NAME%.obj.rsp
dir /b /s %OBJ_DIR%\*.obj > %PRO_NAME%.obj.rsp

cl ^
/c ^
/std:c++20 ^
%INCLUDE_DIR% ^
%MACROS% ^
/nologo ^
/MT ^
/O2 ^
/Fo%OBJ_DIR%\ ^
@%RSP_FILE% 

link ^
/DLL ^
/NOEXP ^
/NOIMPLIB ^
/PDB:NUL ^
/out:%TARGET_DLL% ^
@%PRO_NAME%.obj.rsp ^
%LIBS%

