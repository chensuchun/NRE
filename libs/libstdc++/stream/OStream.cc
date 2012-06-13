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

#include <stream/OStream.h>
#include <arch/Startup.h>
#include <Digits.h>
#include <cstring>

namespace nul {

char OStream::_hexchars_big[] = "0123456789ABCDEF";
char OStream::_hexchars_small[] = "0123456789abcdef";

void OStream::vwritef(const char *fmt, va_list ap) {
	char c,b;
	char *s;
	llong n;
	ullong u;
	size_t size;
	uint pad,width,base,flags;
	bool readFlags;

	// don't try to output something during initialization
	if(!_startup_info.done)
		return;

	while(1) {
		/* wait for a '%' */
		while((c = *fmt++) != '%') {
			write(c);
			/* finished? */
			if(c == '\0')
				return;
		}

		/* read flags */
		flags = 0;
		pad = 0;
		readFlags = true;
		while(readFlags) {
			switch(*fmt) {
				case '-':
					flags |= PADRIGHT;
					fmt++;
					break;
				case '+':
					flags |= FORCESIGN;
					fmt++;
					break;
				case ' ':
					flags |= SPACESIGN;
					fmt++;
					break;
				case '#':
					flags |= PRINTBASE;
					fmt++;
					break;
				case '0':
					flags |= PADZEROS;
					fmt++;
					break;
				case '*':
					pad = va_arg(ap, ulong);
					fmt++;
					break;
				default:
					readFlags = false;
					break;
			}
		}

		/* read pad-width */
		if(pad == 0) {
			while(*fmt >= '0' && *fmt <= '9') {
				pad = pad * 10 + (*fmt - '0');
				fmt++;
			}
		}

		/* read length */
		switch(*fmt) {
			case 'l':
				flags |= LONG;
				fmt++;
				break;
			case 'L':
				flags |= LONGLONG;
				fmt++;
				break;
			case 'z':
				flags |= SIZE_T;
				fmt++;
				break;
			case 'P':
				flags |= INTPTR_T;
				fmt++;
				break;
		}

		/* determine OStream */
		switch(c = *fmt++) {
			/* signed integer */
			case 'd':
			case 'i':
				if(flags & LONGLONG)
					n = va_arg(ap, llong);
				else if(flags & LONG)
					n = va_arg(ap, long);
				else if(flags & SIZE_T)
					n = va_arg(ap, ssize_t);
				else if(flags & INTPTR_T)
					n = va_arg(ap, intptr_t);
				else
					n = va_arg(ap, int);
				printnpad(n,pad,flags);
				break;

			/* pointer */
			case 'p':
				size = sizeof(uintptr_t);
				u = va_arg(ap, uintptr_t);
				flags |= PADZEROS;
				/* 2 hex-digits per byte and a ':' every 2 bytes */
				pad = size * 2 + size / 2;
				while(size > 0) {
					printupad((u >> (size * 8 - 16)) & 0xFFFF,16,4,flags);
					size -= 2;
					if(size > 0)
						write(':');
				}
				break;

			/* unsigned integer */
			case 'b':
			case 'u':
			case 'o':
			case 'x':
			case 'X':
				base = c == 'o' ? 8 : ((c == 'x' || c == 'X') ? 16 : (c == 'b' ? 2 : 10));
				if(c == 'X')
					flags |= CAPHEX;
				if(flags & LONGLONG)
					u = va_arg(ap, ullong);
				else if(flags & LONG)
					u = va_arg(ap, ulong);
				else if(flags & SIZE_T)
					u = va_arg(ap, size_t);
				else if(flags & INTPTR_T)
					u = va_arg(ap, uintptr_t);
				else
					u = va_arg(ap, uint);
				printupad(u,base,pad,flags);
				break;

			/* string */
			case 's':
				s = va_arg(ap, char*);
				if(pad > 0 && !(flags & PADRIGHT)) {
					width = strlen(s);
					printpad(pad - width,flags);
				}
				n = puts(s);
				if(pad > 0 && (flags & PADRIGHT))
					printpad(pad - n,flags);
				break;

			/* character */
			case 'c':
				b = (char)va_arg(ap, uint);
				write(b);
				break;

			default:
				write(c);
				break;
		}
	}
}

void OStream::printnpad(llong n,uint pad,uint flags) {
	int count = 0;
	/* pad left */
	if(!(flags & PADRIGHT) && pad > 0) {
		size_t width = Digits::count_signed(n,10);
		if(n > 0 && (flags & (FORCESIGN | SPACESIGN)))
			width++;
		count += printpad(pad - width,flags);
	}
	/* print '+' or ' ' instead of '-' */
	if(n > 0) {
		if((flags & FORCESIGN)) {
			write('+');
			count++;
		}
		else if(((flags) & SPACESIGN)) {
			write(' ');
			count++;
		}
	}
	/* print number */
	count += printn(n);
	/* pad right */
	if((flags & PADRIGHT) && pad > 0)
		printpad(pad - count,flags);
}

void OStream::printupad(ullong u,uint base,uint pad,uint flags) {
	int count = 0;
	/* pad left - spaces */
	if(!(flags & PADRIGHT) && !(flags & PADZEROS) && pad > 0) {
		size_t width = Digits::count_unsigned(u,base);
		count += printpad(pad - width,flags);
	}
	/* print base-prefix */
	if((flags & PRINTBASE)) {
		if(base == 16 || base == 8) {
			write('0');
			count++;
		}
		if(base == 16) {
			char c = (flags & CAPHEX) ? 'X' : 'x';
			write(c);
			count++;
		}
	}
	/* pad left - zeros */
	if(!(flags & PADRIGHT) && (flags & PADZEROS) && pad > 0) {
		size_t width = Digits::count_unsigned(u,base);
		count += printpad(pad - width,flags);
	}
	/* print number */
	if(flags & CAPHEX)
		count += printu(u,base,_hexchars_big);
	else
		count += printu(u,base,_hexchars_small);
	/* pad right */
	if((flags & PADRIGHT) && pad > 0)
		printpad(pad - count,flags);
}

int OStream::printpad(int count,uint flags) {
	int res = count;
	char c = flags & PADZEROS ? '0' : ' ';
	while(count-- > 0)
		write(c);
	return res;
}

int OStream::printu(ullong n,uint base,char *chars) {
	int res = 0;
	if(n >= base)
		res += printu(n / base,base,chars);
	write(chars[(n % base)]);
	return res + 1;
}

int OStream::printn(llong n) {
	int res = 0;
	if(n < 0) {
		write('-');
		n = -n;
		res++;
	}

	if(n >= 10)
		res += printn(n / 10);
	write('0' + n % 10);
	return res + 1;
}

int OStream::puts(const char *str) {
	const char *begin = str;
	char c;
	while((c = *str)) {
		write(c);
		str++;
	}
	return str - begin;
}

}
