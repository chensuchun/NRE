/*
 * (c) 2012 Nils Asmussen <nils@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <kobj/Sm.h>
#include <kobj/Pt.h>
#include <utcb/UtcbFrame.h>
#include <util/ScopedCapSels.h>
#include <Syscalls.h>
#include <CPU.h>

namespace nul {

/**
 * Represents a global system interrupt. Creation of a GSI allocates it from the parent and
 * destruction release it again. By doing a down() or zero() on this object you can wait for
 * the interrupt.
 */
class Gsi : public Sm {
public:
	enum Op {
		ALLOC,
		RELEASE
	};

	/**
	 * Allocates the given GSI from the parent
	 *
	 * @param gsi the GSI
	 * @param cpu the CPU to route the gsi to
	 */
	explicit Gsi(uint gsi,cpu_t cpu = CPU::current().log_id()) : Sm(alloc(gsi,0,cpu),true) {
		// neither keep the cap nor the selector
		set_flags(0);
	}

	/**
	 * Allocates a new GSI for the device specified by the given PCI configuration space location
	 *
	 * @param pcicfg the location in the PCI configuration space
	 * @param cpu the CPU to route the gsi to
	 */
	explicit Gsi(void *pcicfg,cpu_t cpu = CPU::current().log_id()) : Sm(alloc(0,pcicfg,cpu),true) {
		// neither keep the cap nor the selector
		set_flags(0);
	}

	/**
	 * Releases the GSI
	 */
	virtual ~Gsi() {
		release();
	}

	uint64_t msi_addr() const {
		return _msi_addr;
	}
	word_t msi_value() const {
		return _msi_value;
	}

	/**
	 * @return the GSI
	 */
	uint gsi() const {
		return _gsi;
	}

private:
	capsel_t alloc(uint gsi,void *pcicfg,cpu_t cpu) {
		UtcbFrame uf;
		ScopedCapSels cap;
		uf.set_receive_crd(Crd(cap.get(),0,Crd::OBJ_ALL));
		uf << ALLOC << gsi << pcicfg;
		CPU::current().gsi_pt->call(uf);

		ErrorCode res;
		uf >> res;
		if(res != E_SUCCESS)
			throw Exception(res);
		uf >> _gsi;
		Syscalls::assign_gsi(cap.get(),CPU::get(cpu).phys_id(),pcicfg,&_msi_addr,&_msi_value);
		return cap.release();
	}
	void release() {
		try {
			UtcbFrame uf;
			uf << RELEASE << _gsi;
			CPU::current().gsi_pt->call(uf);
		}
		catch(...) {
			// ignore
		}
	}

	uint64_t _msi_addr;
	word_t _msi_value;
	uint _gsi;
};

}