@echo off

set buildMode=dev
if NOT [%1]==[] (set buildMode=%1)

set gccOptions=-O2 -flto
if %buildMode% == dev (
	set gccOptions=-g
)

@echo on

mkdir "../../../build/opticon-agent/msi"

gcc -std=c17 -Wall -Wextra -Wpedantic ^
	%gccOptions% ^
	-s -shared ^
	^
	-isystem ../../../lib/winlibc/additional/include ^
	^
	-D OS_WINDOWS ^
	-D WIN32_LEAN_AND_MEAN ^
	-D _WIN32_WINNT=0x0601 ^
	^
	-I ../../../src ^
	-I ../../../include ^
	^
	msiCustomActionDll.c ^
	^
	../../../src/libopticon/defaults.c ^
	^
	../../../src/libopticon/fillrandom.c ^
	../../../src/libopticon/hash.c ^
	../../../src/libopticon/ioport.c ^
	../../../src/libopticon/ioport_file.c ^
	../../../src/libopticon/util.c ^
	../../../src/libopticon/uuid.c ^
	../../../src/libopticon/var.c ^
	../../../src/libopticon/var_dump.c ^
	../../../src/libopticon/var_parse.c ^
	^
	../../../lib/winlibc/beforewin8/src/endian.c ^
	^
	../../../lib/winlibc/additional/src/string.c ^
	../../../lib/winlibc/additional/src/time.c ^
	^
	-l kernel32 ^
	-l msi ^
	-l bcrypt ^
	-l ws2_32 ^
	^
	-o ../../../build/opticon-agent/msi/msiCustomAction.dll