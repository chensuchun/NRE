/*
 * TODO comment me
 *
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <cstring>

void* memcpy(void *dest,const void *src,size_t len) {
	uchar *bdest,*bsrc;
	/* copy dwords first with loop-unrolling */
	uint *ddest = (uint*)dest;
	uint *dsrc = (uint*)src;
	while(len >= sizeof(uint) * 8) {
		*ddest = *dsrc;
		*(ddest + 1) = *(dsrc + 1);
		*(ddest + 2) = *(dsrc + 2);
		*(ddest + 3) = *(dsrc + 3);
		*(ddest + 4) = *(dsrc + 4);
		*(ddest + 5) = *(dsrc + 5);
		*(ddest + 6) = *(dsrc + 6);
		*(ddest + 7) = *(dsrc + 7);
		ddest += 8;
		dsrc += 8;
		len -= sizeof(uint) * 8;
	}
	/* copy remaining dwords */
	while(len >= sizeof(uint)) {
		*ddest++ = *dsrc++;
		len -= sizeof(uint);
	}

	/* copy remaining bytes */
	bdest = (uchar*)ddest;
	bsrc = (uchar*)dsrc;
	while(len-- > 0)
		*bdest++ = *bsrc++;
	return dest;
}

void *memmove(void *dest,const void *src,size_t count) {
	uchar *s,*d;
	/* nothing to do? */
	if((uchar*)dest == (uchar*)src || count == 0)
		return dest;

	/* moving forward */
	if((uintptr_t)dest > (uintptr_t)src) {
		uint *dsrc = (uint*)((uintptr_t)src + count - sizeof(uint));
		uint *ddest = (uint*)((uintptr_t)dest + count - sizeof(uint));
		while(count >= sizeof(uint)) {
			*ddest-- = *dsrc--;
			count -= sizeof(uint);
		}
		s = (uchar*)dsrc + (sizeof(uint) - 1);
		d = (uchar*)ddest + (sizeof(uint) - 1);
		while(count-- > 0)
			*d-- = *s--;
	}
	/* moving backwards */
	else
		memcpy(dest,src,count);

	return dest;
}

void *memset(void *addr,int value,size_t count) {
	uchar *baddr;
	uint dwval = (value << 24) | (value << 16) | (value << 8) | value;
	uint *dwaddr = (uint*)addr;
	while(count >= sizeof(uint)) {
		*dwaddr++ = dwval;
		count -= sizeof(uint);
	}

	baddr = (uchar*)dwaddr;
	while(count-- > 0)
		*baddr++ = value;
	return addr;
}

size_t strlen(const char *src) {
	size_t len = 0;
	while(*src++)
		len++;
	return len;
}

int memcmp(const void *str1,const void *str2,size_t count) {
	const uchar *s1 = (const uchar*)str1;
	const uchar *s2 = (const uchar*)str2;
	while(count-- > 0) {
		if(*s1++ != *s2++)
			return s1[-1] < s2[-1] ? -1 : 1;
	}
	return 0;
}

int strcmp(const char *dst,const char *src) {
	return memcmp(dst,src,strlen(dst));
}