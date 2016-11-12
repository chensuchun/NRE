/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <arch/Types.h>
#include <arch/Elf.h>
#include <Compiler.h>

#include "serial.h"

#define ARRAY_SIZE(array)		(sizeof((array)) / sizeof((array)[0]))

struct BootModule {
	uint32_t modStart;
	uint32_t modEnd;
	uint32_t name;		/* may be 0 */
	uint32_t : 32;		/* reserved */
} PACKED;

struct BootMemMap {
	uint32_t size;
	uint64_t baseAddr;
	uint64_t length;
	uint32_t type;
} PACKED;

struct BootAPMTable {
	uint16_t version;
	uint16_t cseg;
	uint16_t offset;
	uint16_t cseg16;
	uint16_t dseg;
	uint16_t flags;
	uint16_t csegLen;
	uint16_t cseg16Len;
	uint16_t dsegLen;
} PACKED;

struct BootDrive {
	uint32_t size;
	uint8_t number;
	uint8_t mode;
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectors;
	uint8_t ports[];
} PACKED;

/* multi-boot-information */
struct MultiBootInfo {
	uint32_t flags;
	uint32_t memLower;				/* present if flags[0] is set */
	uint32_t memUpper;				/* present if flags[0] is set */
	struct {
		uint8_t partition3;			/* sub-sub-partition (0xFF = not used) */
		uint8_t partition2;			/* sub-partition (0xFF = not used) */
		uint8_t partition1;			/* top-level partition-number (0xFF = not used) */
		uint8_t drive;				/* contains the bios drive number as understood by the bios
									   INT 0x13 low-level disk interface: e.g. 0x00 for the first
									   floppy disk or 0x80 for the first hard disk */
	} PACKED bootDevice;			/* present if flags[1] is set */
	uint32_t cmdLine;				/* present if flags[2] is set */
	uint32_t modsCount;				/* present if flags[3] is set */
	uint32_t modsAddr;				/* present if flags[3] is set */
	union {
		struct {
			uint32_t tabSize;
			uint32_t strSize;
			uint32_t addr;
			uint32_t reserved;
		} PACKED aDotOut;			/* present if flags[4] is set */
		struct {
			uint32_t num;
			uint32_t size;
			uint32_t addr;
			uint32_t shndx;
		} PACKED ELF;				/* present if flags[5] is set */
	} syms;
	uint32_t mmapLength;			/* present if flags[6] is set */
	uint32_t mmapAddr;				/* present if flags[6] is set */
	uint32_t drivesLength;			/* present if flags[7] is set */
	uint32_t drivesAddr;			/* present if flags[7] is set */
	uint32_t configTable;			/* present if flags[8] is set */
	uint32_t bootLoaderName;		/* present if flags[9] is set */
	uint32_t apmTable;				/* present if flags[10] is set */
} PACKED;

enum {
	MAX_MMOD_ENTRIES	= 8,
	MAX_MMAP_ENTRIES	= 16,
};

typedef struct E820Entry {
	uint64_t baseAddr;
	uint64_t length;
	uint32_t type;
} PACKED E820Entry;

#if defined(NDEBUG)
ALIGNED(4096) static const uint8_t dump_kernel[] = {
#	include "../../build/x86_64-release/hypervisor-elf64.dump"
};
ALIGNED(4096) static const uint8_t dump_root[] = {
#	include "../../build/x86_64-release/root.dump"
};
ALIGNED(4096) static const uint8_t dump_unittests[] = {
#	include "../../build/x86_64-release/unittests.dump"
};
#else
ALIGNED(4096) static const uint8_t dump_kernel[] = {
#	include "../../build/x86_64-debug/hypervisor-elf64.dump"
};
ALIGNED(4096) static const uint8_t dump_root[] = {
#	include "../../build/x86_64-debug/root.dump"
};
ALIGNED(4096) static const uint8_t dump_unittests[] = {
#	include "../../build/x86_64-debug/unittests.dump"
};
#endif

struct Module {
	const char cmdline[64];
	const uint8_t *dump;
	size_t size;
};

__attribute__((section(".mb"))) static Module mods[] = {
	{"root", 		dump_root, 			sizeof(dump_root)},
	{"unittests", 	dump_unittests, 	sizeof(dump_unittests)},
};

__attribute__((section(".mb"))) static BootModule mmod[MAX_MMOD_ENTRIES];
__attribute__((section(".mb"))) static BootMemMap mmap[MAX_MMAP_ENTRIES];
__attribute__((section(".mb"))) MultiBootInfo mbi;

#pragma GCC optimize ("no-tree-loop-distribute-patterns")

static void memcopy(void *dst, const void *src, size_t len) {
	const uint8_t *s = reinterpret_cast<const uint8_t*>(src);
	uint8_t *d = reinterpret_cast<uint8_t*>(dst);
	while(len-- > 0)
		*d++ = *s++;
}

static void memclear(void *dst, size_t len) {
	uint8_t *d = reinterpret_cast<uint8_t*>(dst);
	while(len-- > 0)
		*d = 0;
}

static uintptr_t load(const void *elf) {
	const nre::ElfEh *header = (const nre::ElfEh*)elf;

	// check magic
	if(header->e_ident[0] != '\177' ||
		header->e_ident[1] != 'E' ||
		header->e_ident[2] != 'L' ||
		header->e_ident[3] != 'F') {
		puts("Invalid ELF magic\n");
		return 0;
	}

	// load the LOAD segments.
	const nre::ElfPh *pheader = (const nre::ElfPh*)((char*)elf + header->e_phoff);
	for(int j = 0; j < header->e_phnum; j++) {
		// PT_LOAD
		if(pheader->p_type == 1) {
			memcopy((void*)pheader->p_paddr, (const char*)elf + pheader->p_offset, pheader->p_filesz);
			memclear((void*)(pheader->p_paddr + pheader->p_filesz), pheader->p_memsz - pheader->p_filesz);
		}

		// to next
		pheader = (const nre::ElfPh*)((char*)pheader + header->e_phentsize);
	}

	return header->e_entry;
}

extern "C" uintptr_t loader(const char *cmdline,size_t mapCount,const E820Entry *map);

uintptr_t loader(const char *cmdline,size_t mapCount,const E820Entry *map) {
	initSerial();

	puts("Loading kernel...");
	uintptr_t entry = load(dump_kernel);
	if(!entry) {
		while(1)
			;
	}
	puts("done\n");

	// prepare multiboot info
	mbi.flags = (1 << 2) | (1 << 3) | (1 << 6);	// cmdline, modules and mmap
	mbi.cmdLine = (uintptr_t)cmdline;

	mbi.modsCount = ARRAY_SIZE(mods);
	mbi.modsAddr = (uintptr_t)mmod;
	for(size_t i = 0; i < ARRAY_SIZE(mods); ++i) {
		mmod[i].modStart = (uintptr_t)mods[i].dump;
		mmod[i].modEnd = (uintptr_t)mods[i].dump + mods[i].size;
		mmod[i].name = (uintptr_t)mods[i].cmdline;
	}

	mbi.mmapLength = mapCount * sizeof(BootMemMap);
	mbi.mmapAddr = (uintptr_t)mmap;
	for(size_t i = 0; i < mapCount; ++i) {
		mmap[i].baseAddr = map[i].baseAddr;
		mmap[i].length = map[i].length;
		mmap[i].type = map[i].type == 1 ? 1 : 2;
		mmap[i].size = 20;
	}

	return entry;
}
