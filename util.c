#include <datatypes.h>
#include <util.h>

int uuidcmp (uuid_t first, uuid_t second) {
	return (first.msb == second.msb && first.lsb == second.lsb);
}

uuid_t mkuuid (const char *str) {
	uuid_t res = {0,0};
	const char *crsr = str;
	char c;
	int out = 0;
	uint64_t rout = 0;
	int respos = 0;
	int majpos = 0;
	if (! crsr) return res;
	
	while (*crsr && (majpos<2)) {
		c = *crsr;
		out = -1;
		if ((c>='0')&&(c<='9')) out = c-'0';
		else if ((c>='a')&&(c<='f')) out = (c-'a')+10;
		else if ((c>='A')&&(c<='F')) out = (c-'A')+10;
		if (out >= 0) {
			rout = out;
			rout = rout << (4*(15-respos));
			if (majpos) res.msb |= rout;
			else res.lsb |= rout;
			respos++;
			if (respos>15) {
				respos = 0;
				majpos++;
			}
		}
		crsr++;
	}
	return res;
}

/* 5 bit character mapping for ids */
const char *IDTABLE = " abcdefghijklmnopqrstuvwxyz.-_/@";
uint64_t ASCIITABLE[] = {
	// 0x00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	// 0x10
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	// 0x20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 27, 30,
	// 0x30
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	// 0x40
	31, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	// 0x50
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 29,
	// 0x60
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	// 0x70
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 0
};

meterid_t makeid (const char *label, metertype_t type, int pos) {
	meterid_t res = (meterid_t) type;
	int bshift = 56;
	const char *crsr = label;
	char c;
	while ((c = *crsr) && bshift > 1) {
		if (c>0) {
			res |= (ASCIITABLE[c] << bshift);
			bshift -= 5;
		}
		crsr++;
	}
	return res;
}

void id2str (meterid_t id, char *into) {
	char *out = into;
	char *end = into;
	char t;
	uint64_t tmp;
	int bshift = 56;
	*out = 0;
	while (bshift > 1) {
		tmp = ((id & MMASK_NAME) >> bshift) & 0x1f;
		t = IDTABLE[tmp];
		*out = t;
		if (t != ' ') end = out;
		out++;
		bshift -= 5;
	}
	*(end+1) = 0;
}
