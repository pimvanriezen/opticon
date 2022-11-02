# To build:
# cmake -DBUILD_MODE=dev -S . -B ../../build/opticon-agent/cmake -G "MinGW Makefiles"
# cmake --build ../../build/opticon-agent/cmake -j 3

cmake_minimum_required(VERSION 3.10)

#set(CMAKE_VERBOSE_MAKEFILE ON)

project(opticon-agent C)

set(BUILD_MODE "dev" CACHE STRING "")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/../../bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

#set(CMAKE_C_FLAGS "")
#set(CMAKE_EXE_LINKER_FLAGS "")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

if(BUILD_MODE STREQUAL "production")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -flto")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()
unset(BUILD_MODE CACHE) # Prevent caching the cli options

add_definitions(-D OS_WINDOWS)
add_definitions(-D WIN32_LEAN_AND_MEAN)
add_definitions(-D _WIN32_WINNT=0x0601)

#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /subsystem:windows /ENTRY:mainCRTStartup")


include_directories(${PROJECT_SOURCE_DIR}/../../lib/winlibc/additional/include)

include_directories(${PROJECT_SOURCE_DIR}/../../src)
include_directories(${PROJECT_SOURCE_DIR}/../../include)
include_directories(${PROJECT_SOURCE_DIR}/../../lib/zlib/include)

link_directories(${PROJECT_SOURCE_DIR}/../../lib/zlib/lib)


#GLOB_RECURSE
#file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/../../src/c/opticon/[a-z]*.c)
#file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/../../lib/opticon/src/[a-z]*.c)
#file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/../../lib/winhelper/src/winhelper/[a-z]*.c)
#file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/../../lib/winlibc/src/[a-z]*.c)

#file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/../../src/c/opticon/main.c)


# WIN32 is like the gcc option -mwindows which will set the linker option /subsystem:windows which means it will not automatically attach or create a console
# But is also wants a WinMain entry point
# @example add_executable(opticon-agent WIN32 ${SOURCE_FILES})
#add_executable(opticon-agent ${SOURCE_FILES2})

add_executable(opticon-agent
	../../build/opticon-agent/version.c
	#
	../../src/opticon-agent/main.c
	../../src/opticon-agent/collectorlist.c
	../../src/opticon-agent/probelist.c
	../../src/opticon-agent/probes_windows.c
	../../src/opticon-agent/resender.c
	../../src/opticon-agent/win/stringHelper.c
	../../src/opticon-agent/win/systemFirmwareInfo.c
	../../src/opticon-agent/win/timeHelper.c
	../../src/opticon-agent/win/updater.c
	../../src/opticon-agent/win/winService.c
	#
	../../src/libopticon/adjust.c
	../../src/libopticon/aes.c
	../../src/libopticon/auth.c
	../../src/libopticon/base64.c
	../../src/libopticon/cliopt.c
	../../src/libopticon/codec.c
	../../src/libopticon/codec_json.c
	../../src/libopticon/codec_pkt.c
	../../src/libopticon/compress.c
	../../src/libopticon/defaults.c
	../../src/libopticon/fillrandom.c
	../../src/libopticon/glob.c
	../../src/libopticon/hash.c
	../../src/libopticon/host.c
	../../src/libopticon/host_import.c
	../../src/libopticon/ioport.c
	../../src/libopticon/ioport_buffer.c
	../../src/libopticon/ioport_file.c
	../../src/libopticon/log.c
	../../src/libopticon/meter.c
	../../src/libopticon/notify.c
	../../src/libopticon/pktwrap.c
	../../src/libopticon/popen.c
	../../src/libopticon/react.c
	../../src/libopticon/summary.c
	../../src/libopticon/tenant.c
	../../src/libopticon/thread.c
	../../src/libopticon/transport.c
	../../src/libopticon/transport_udp.c
	../../src/libopticon/util.c
	../../src/libopticon/uuid.c
	../../src/libopticon/var.c
	../../src/libopticon/var_dump.c
	../../src/libopticon/var_parse.c
	../../src/libopticon/watchlist.c
	#
	../../lib/winlibc/beforewin8/src/endian.c
	#
	../../lib/winlibc/additional/src/memory.c
	../../lib/winlibc/additional/src/string.c
	../../lib/winlibc/additional/src/time.c
)

target_link_libraries(opticon-agent libz.a)
target_link_libraries(opticon-agent libpthread.a)

target_link_libraries(opticon-agent kernel32)
target_link_libraries(opticon-agent user32)
target_link_libraries(opticon-agent ws2_32)
target_link_libraries(opticon-agent bcrypt)
target_link_libraries(opticon-agent pdh)
target_link_libraries(opticon-agent ntdll)
target_link_libraries(opticon-agent advapi32)
target_link_libraries(opticon-agent wtsapi32)
target_link_libraries(opticon-agent psapi)
target_link_libraries(opticon-agent iphlpapi)
target_link_libraries(opticon-agent winhttp)
target_link_libraries(opticon-agent wininet)

# @note Contrary to the documentation you must not use a space after -e because it includes it in the entry point name it looks for
#target_link_options(opticon-agent PRIVATE "-emainCRTStartup")
#target_link_options(opticon-agent PRIVATE "-eWinMainCRTStartup")