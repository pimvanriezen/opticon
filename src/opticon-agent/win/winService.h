#pragma once

#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

typedef int (*FAppMain)(int, const char *[]);

uint32_t installWindowsService(bool startAfterInstall);
uint32_t uninstallWindowsService();
uint32_t mainWithServiceSupport(int argc, const char *argv[], FAppMain);