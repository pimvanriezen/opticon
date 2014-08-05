#ifndef _UTIL_H
#define _UTIL_H 1

#include <datatypes.h>

int uuidcmp (uuid_t first, uuid_t second);
uuid_t mkuuid (const char *str);
void id2str (meterid_t id, char *into);
meterid_t makeid (const char *label, metertype_t type, int pos);

#endif
