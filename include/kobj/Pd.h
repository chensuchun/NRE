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

#include <arch/Startup.h>
#include <kobj/KObject.h>
#include <cap/CapSpace.h>
#include <cap/CapHolder.h>
#include <Syscalls.h>

namespace nul {

class Pd : public KObject {
	friend void ::_setup();

	explicit Pd(cap_t cap,bool) : KObject(cap) {
	}

public:
	static Pd *current();

	explicit Pd(Crd crd = Crd(0),Pd *pd = Pd::current()) : KObject() {
		CapHolder ch;
		Syscalls::create_pd(ch.get(),crd,pd->cap());
		cap(ch.release());
	}

private:
	Pd(const Pd&);
	Pd& operator=(const Pd&);
};

}