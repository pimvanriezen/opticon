#pragma once
#include <stdint.h>

#include <winsock2.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN8
	uint64_t htonll(uint64_t host_64bits);
	uint64_t ntohll(uint64_t big_endian_64bits);
#endif