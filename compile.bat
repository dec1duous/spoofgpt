@echo off

if not defined CPP (
	set CPP=clang++
)

if not exist obj mkdir obj
if not exist bin mkdir bin

@echo on

%CPP% -c -g -oobj/main.o source/main.cpp @includes.rsp || goto exit_err

%CPP% -obin/prog.exe obj/main.o || goto exit_err

@echo off
goto exit_ok

:exit_err
@echo off
pause

:exit_ok
