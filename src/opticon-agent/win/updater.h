#pragma once

#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

uint32_t checkUpdate(const char *url, const char *channel);
uint32_t checkAndInstallUpdate(const char *url, const char *channel);
uint32_t installLatest(const char *url, const char *channel);