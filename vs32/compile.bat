@echo off
cp 65001
set PYTHONIOENCODING=
set OUTPUT_ENCODING=
setlocal enabledelayedexpansion

set CTP_VERSION=6.7.13

:: 按 . 分割为 major minor patch
for /f "delims=., tokens=1,2,3" %%a in ("%CTP_VERSION%") do (
    set VER_MAJOR=%%a
    set VER_MINOR=%%b
    set VER_PATCH=%%c
)

:: 算术计算
set /a CTP_VER=!VER_MAJOR!*1000000 + !VER_MINOR!*1000 + !VER_PATCH!

echo !CTP_VER!

set PRO_NAME=thosttraderapi_se

:: 宏定义
set MACROS=/DWIN32 /DLIB_TRADER_API_EXPORT /DISLIB /DWIN32_LEAN_AND_MEAN   /DCTP_VER=!CTP_VER!



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

set INCLUDE_DIR=/I ../src/api/net /I../src/api/trade /I ../src/public /I../depend/event/include /I ..\depend\ctp\v%CTP_VERSION%\include

set LIBS=../depend/event/lib/vs32/event.lib  ws2_32.lib  advapi32.lib iphlpapi.lib

::../depend/event/lib/vs32/event_core.lib ../depend/event/lib/vs32/event_extra.lib

echo. > %PRO_NAME%.obj.rsp
dir /b /s %OBJ_DIR%\*.obj > %PRO_NAME%.obj.rsp

cl ^
/c ^
/O2 ^
/MT ^
/std:c++20 ^
/EHsc ^
%INCLUDE_DIR% ^
%MACROS% ^
/nologo ^
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

