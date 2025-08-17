@echo off

set winlibs=-lwinmm -lgdi32 -luser32 -lkernel32 -lshell32
set rtlibs=-lmsvcrt -lucrt -lmsvcprt -lvcruntime -llibcmt -Xlinker /NODEFAULTLIB

if [%1]==[noconsole] set rtlibs=%rtlibs% -Xlinker /SUBSYSTEM:WINDOWS -Xlinker /ENTRY:mainCRTStartup -DNDEBUG -O3

clang -o todo.exe -std=c23 main.c -Llib -lraylib %winlibs% %rtlibs% -D_CRT_SECURE_NO_WARNINGS