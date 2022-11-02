@echo off

set buildMode=dev
if NOT [%1]==[] (set buildMode=%1)

set gccOptions=-g
if %buildMode% == production (
	set gccOptions=-O2 -flto
)

@echo on

call build-versionSourceFile.bat


@rem The linking process only links/resolves symbols from a linked lib that it needs *at that point in the process*.
@rem If a another lib is linked that uses symbols from the earlier lib, you have to link it again *after* the other lib.
@rem That's why source files (main.c etc) usually must come before the linked libraries (e.g. "-l kernel32") otherwise you will get "undefined reference to `__imp_SomeFunction'"

@rem "-l" flag assumes the library starts with "lib" and ends with ".a" (but prefers dynamic linking to the dll above static linking)
@rem You can use "-l:libYOURLIB.a" to link statically

@rem "-l z" is the zlib
@rem "-l:libz.a" means statically link the zlib

@rem "-l winpthread" is the winpthread lib
@rem "-l:libwinpthread.a" means statically link the winpthread lib
@rem winpthread and pthread are identical except that winpthread doesn't have a symbols table or something(?)


@rem Either "-l ntdll" or "-l ntoskrnl" seem to work (both are documented)
@rem For psapi: Kernel32.lib on Windows 7 and Windows Server 2008 R2; Psapi.lib (if PSAPI_VERSION=1) on Windows 7 and Windows Server 2008 R2; Psapi.lib on Windows Server 2008, Windows Vista, Windows Server 2003 and Windows XP


@rem "-mwindows" is the same as the linker flag: "-Wl,--subsystem,windows"
@rem "-mconsole" is the same as the linker flag: "-Wl,--subsystem,console" (this is the default)
@rem If you do not define a function: "int main() {}" you will get an error: "undefined reference to `WinMain'", even when you use -mconsole
@rem It seems with MinGW you are free the use main() or WinMain() as your start function for both -mconsole and -mwindows

@rem https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-170
@rem Need 0x0500 _WIN32_WINNT_WIN2K or later for GetConsoleWindow
@rem Need 0x0501 _WIN32_WINNT_WINXP or later for AttachConsole
@rem Need 0x0601 _WIN32_WINNT_WIN7 or later for AI_NUMERICSERV
@rem Need 0x0602 _WIN32_WINNT_WIN8 or later for htonll in winsock2.h (to support Windows Server 2008 R2 (Windows 7 codebase), we need to implement htonll ourselves in endian.c)
@rem @note _WIN32_WINNT is defined in _mingw.h, which is included by corecrt.h < crtdefs.h < string.h
@rem So we really need to define this before *any* other includes

@rem The code is vista compatible (0x0600 WIN32_WINNT_VISTA), but we do need some of the function declarations of win7 for which we do runtime version checking (QueryUnbiasedInterruptTime)

@rem https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
@rem Define WIN32_LEAN_AND_MEAN to exclude APIs such as Cryptography, DDE, RPC, Shell, and Windows Sockets.

@rem	-g ^
@rem	-O2 -flto ^

@rem	-fdebug-prefix-map=D:\\maarten\\apache\\opticon=C:\\Users\\Administrator\\Desktop\\opticon-agent ^


gcc -std=c17 -Wall -Wextra -Wpedantic ^
	%gccOptions% ^
	^
	-isystem ../../lib/winlibc/additional/include ^
	^
	-D OS_WINDOWS ^
	-D WIN32_LEAN_AND_MEAN ^
	-D _WIN32_WINNT=0x0601 ^
	^
	-I ../../src ^
	-I ../../include ^
	-I ../../lib/zlib/include ^
	^
	-L../../lib/zlib/lib ^
	^
	../../build/opticon-agent/version.c ^
	^
	../../src/opticon-agent/main.c ^
	../../src/opticon-agent/collectorlist.c ^
	../../src/opticon-agent/probelist.c ^
	../../src/opticon-agent/probes_windows.c ^
	../../src/opticon-agent/resender.c ^
	../../src/opticon-agent/win/stringHelper.c ^
	../../src/opticon-agent/win/systemFirmwareInfo.c ^
	../../src/opticon-agent/win/timeHelper.c ^
	../../src/opticon-agent/win/updater.c ^
	../../src/opticon-agent/win/winService.c ^
	^
	../../src/libopticon/defaults.c ^
	^
	../../src/libopticon/adjust.c ^
	../../src/libopticon/aes.c ^
	../../src/libopticon/auth.c ^
	../../src/libopticon/base64.c ^
	../../src/libopticon/cliopt.c ^
	../../src/libopticon/codec.c ^
	../../src/libopticon/codec_json.c ^
	../../src/libopticon/codec_pkt.c ^
	../../src/libopticon/compress.c ^
	../../src/libopticon/fillrandom.c ^
	../../src/libopticon/glob.c ^
	../../src/libopticon/hash.c ^
	../../src/libopticon/host.c ^
	../../src/libopticon/host_import.c ^
	../../src/libopticon/ioport.c ^
	../../src/libopticon/ioport_buffer.c ^
	../../src/libopticon/ioport_file.c ^
	../../src/libopticon/log.c ^
	../../src/libopticon/meter.c ^
	../../src/libopticon/notify.c ^
	../../src/libopticon/pktwrap.c ^
	../../src/libopticon/popen.c ^
	../../src/libopticon/react.c ^
	../../src/libopticon/summary.c ^
	../../src/libopticon/tenant.c ^
	../../src/libopticon/thread.c ^
	../../src/libopticon/transport.c ^
	../../src/libopticon/transport_udp.c ^
	../../src/libopticon/util.c ^
	../../src/libopticon/uuid.c ^
	../../src/libopticon/var.c ^
	../../src/libopticon/var_dump.c ^
	../../src/libopticon/var_parse.c ^
	../../src/libopticon/watchlist.c ^
	^
	../../lib/winlibc/beforewin8/src/endian.c ^
	^
	../../lib/winlibc/additional/src/memory.c ^
	../../lib/winlibc/additional/src/string.c ^
	../../lib/winlibc/additional/src/time.c ^
	^
	-l:libz.a ^
	-l:libpthread.a ^
	^
	-l kernel32 ^
	-l user32 ^
	-l ws2_32 ^
	-l bcrypt ^
	-l pdh ^
	-l ntdll ^
	-l advapi32 ^
	-l wtsapi32 ^
	-l psapi ^
	-l iphlpapi ^
	-l winhttp ^
	-l wininet ^
	^
	-o ../../bin/opticon-agent.exe


:exit