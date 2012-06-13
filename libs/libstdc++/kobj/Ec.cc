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

#include <kobj/Ec.h>
#include <Compiler.h>
#include <RCU.h>

namespace nul {

void Ec::create(Pd *pd,Syscalls::ECType type,void *sp) {
	CapHolder ch;
	Syscalls::create_ec(ch.get(),_utcb,sp,_cpu,_event_base,type,pd->sel());
	if(pd == Pd::current())
		RCU::announce(this);
	sel(ch.release());
}

// TODO arch-dependent; note that this assumes that these addresses are not already occupied by e.g.
// the utcb selected by the parent at startup
uintptr_t Ec::_utcb_addr = 0x7FFFE000;

}
