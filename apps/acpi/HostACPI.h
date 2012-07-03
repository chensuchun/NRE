/*
 * (c) 2012 Nils Asmussen <nils@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <arch/Types.h>
#include <dev/ACPI.h>
#include <Compiler.h>

class HostACPI {
	enum {
		BIOS_MEM_ADDR	= 0xe0000,
		BIOS_MEM_SIZE	= 0x20000,
		BIOS_ADDR		= 0x0,
		BIOS_SIZE		= 0x1000,
		BIOS_EBDA_OFF	= 0x40E,
		BIOS_EBDA_SIZE	= 1024
	};

	/* root system descriptor pointer */
	struct RSDP {
		uint32_t signature[2];
		uint8_t checksum;
		char oemId[6];
		uint8_t revision;
		uint32_t rsdtAddr;
		/* since 2.0 */
		uint32_t length;
		uint64_t xsdtAddr;
		uint8_t xchecksum;
	} PACKED;

public:
	explicit HostACPI();

	const nul::DataSpace &mem() const {
		return *_ds;
	}
	uintptr_t find(const char *name,uint instance);

private:
	static char checksum(char *table,unsigned count) {
		char res = 0;
		while(count--)
			res += table[count];
		return res;
	}
	static RSDP *get_rsdp();

private:
	size_t _count;
	nul::ACPI::RSDT **_tables;
	RSDP *_rsdp;
	nul::DataSpace *_ds;
};
