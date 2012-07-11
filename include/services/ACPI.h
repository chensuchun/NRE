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

#pragma once

#include <arch/Types.h>
#include <ipc/Connection.h>
#include <ipc/Session.h>
#include <mem/DataSpace.h>
#include <utcb/UtcbFrame.h>
#include <Exception.h>
#include <CPU.h>

namespace nre {

class ACPI {
public:
	enum Command {
		GET_MEM,
		FIND_TABLE
	};

	  /* root system descriptor table */
	struct RSDT {
		char signature[4];
		uint32_t length;
		uint8_t revision;
		uint8_t checksum;
		char oemId[6];
		char oemTableId[8];
		uint32_t oemRevision;
		char creatorId[4];
		uint32_t creatorRevision;
	} PACKED;

private:
	ACPI();
};

class ACPISession : public Session {
public:
	explicit ACPISession(Connection &con) : Session(con), _ds(), _pts(new Pt*[CPU::count()]) {
		for(cpu_t cpu = 0; cpu < CPU::count(); ++cpu)
			_pts[cpu] = con.available_on(cpu) ? new Pt(caps() + cpu) : 0;
		get_mem();
	}
	virtual ~ACPISession() {
		for(cpu_t cpu = 0; cpu < CPU::count(); ++cpu)
			delete _pts[cpu];
		delete[] _pts;
		delete _ds;
	}

	ACPI::RSDT *find_table(const String &name,uint instance = 0) const {
		UtcbFrame uf;
		uf << ACPI::FIND_TABLE << name << instance;
		_pts[CPU::current().log_id()]->call(uf);

		ErrorCode res;
		uf >> res;
		if(res != E_SUCCESS)
			throw Exception(res);
		uintptr_t offset;
		uf >> offset;
		return reinterpret_cast<ACPI::RSDT*>(offset == 0 ? 0 : _ds->virt() + offset);
	}

private:
	void get_mem() {
		UtcbFrame uf;
		uf << ACPI::GET_MEM;
		_pts[CPU::current().log_id()]->call(uf);

		ErrorCode res;
		uf >> res;
		if(res != E_SUCCESS)
			throw Exception(res);
		DataSpaceDesc desc;
		uf >> desc;
		_ds = new DataSpace(desc);
	}

	DataSpace *_ds;
	Pt **_pts;
};

}