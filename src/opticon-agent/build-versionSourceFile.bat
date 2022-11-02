@echo off
@setlocal

@rem ---------------------------------------------------
@rem - Get version number from git tag
@rem ---------------------------------------------------
@rem @todo git describe cannot handle multiple tags on the same commit
where git >nul 2>nul
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
mkdir "../../build/opticon-agent"
echo const char *VERSION = "%versionNumber%"; > ../../build/opticon-agent/version.c


:exit
@echo on