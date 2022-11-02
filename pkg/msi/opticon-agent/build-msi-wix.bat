@echo off
@setlocal

set buildMode=dev
if NOT [%1]==[] (set buildMode=%1)


@rem set /p versionNumber=<version.txt

@rem ---------------------------------------------------
@rem - Get version number from git tag
@rem ---------------------------------------------------
@rem @todo git describe cannot handle multiple tags on the same commit
where git describe --tags >nul 2>nul
if %ERRORLEVEL% EQU 0 ( goto continue_1 )
echo Git command not found
goto exit

:continue_1
FOR /F %%a IN ('git describe --tags') DO set gitTag=%%a
set remove=%gitTag:*-=%
call set versionNumber=%%gitTag:-%remove%=%%
if NOT [%versionNumber%]==[] ( goto continue_2 )
echo Version tag from git is empty
goto exit
@rem ---------------------------------------------------

:continue_2

@rem set versionNumber=0.9.5


call :echoCommand candle opticon-agent.wxs -arch x64 -dbuildMode="%buildMode%" -dversionNumber="%versionNumber%" -ext WixUIExtension -ext WixUtilExtension -o ../../../build/opticon-agent/msi/opticon-agent.wixobj
if %ERRORLEVEL% EQU 0 (
	@rem -spdb suppresses the output of opticon-agent.wixpdb (it contains extra symbol information about the .msi, it may be useful to archive this file to build patch files)
	call :echoCommand light ../../../build/opticon-agent/msi/opticon-agent.wixobj -spdb -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -o ../../../bin/opticon-agent-%versionNumber%.msi
)




:exit



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