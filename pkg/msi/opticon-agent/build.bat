@echo off
@setlocal
cls

set buildMode=dev
if NOT [%1]==[] (set buildMode=%1)


call :echoCommand call build-msiCustomActionDll-gcc.bat
if %ERRORLEVEL% EQU 0 (
	call :echoCommand call build-msi-wix.bat %buildMode%
)



@rem --------------------------------------------------------------------------
@rem Below is used for the echoCommand utility call
@exit /B

@rem @example call :echoCommand bla.exe
@rem @note This is a hack to be able to toggle "echo on" inside parentheses (such as an if block) for a single command
:echoCommand
@echo on
%*
@echo off
@exit /B