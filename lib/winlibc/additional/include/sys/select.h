#pragma once

#define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#define WIFEXITED(w) (((w) & 0xff) == 0)

//#define WEXITSTATUS(stat_val) ((stat_val) & 255)
//#define WIFEXITED(stat_val) (((stat_val) & 0xC0000000) == 0)
//#define WIFSIGNALED(stat_val) (((stat_val) & 0xC0000000) == 0xC0000000)
//#define WTERMSIG(stat_val) win32_status_to_termsig (stat_val)
//#define WIFSTOPPED(stat_val) ((stat_val) == (stat_val) ? 0 : 0)
//#define WSTOPSIG(stat_var) (0)