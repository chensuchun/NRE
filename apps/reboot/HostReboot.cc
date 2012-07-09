/*
* (c) 2012 Nils Asmussen <nils@os.inf.tu-dresden.de>
*     economic rights: Technische Universität Dresden (Germany)
*
* This file is part of TUD:OS and distributed under the terms of the
* GNU General Public License 2.
* Please see the COPYING-GPL-2 file for details.
*/

#include "HostReboot.h"

using namespace nul;

HostRebootKeyboard::HostRebootKeyboard() : HostRebootMethod(), _con("keyboard"), _sess(_con) {
}

void HostRebootKeyboard::reboot() {
	Serial::get() << "Trying reboot via PS2 keyboard...\n";
	_sess.reboot();
}

HostRebootFastGate::HostRebootFastGate() : HostRebootMethod(), _port(0x92,1) {
}

void HostRebootFastGate::reboot() {
	Serial::get() << "Trying reboot via fastgate...\n";
	_port.out<uint8_t>(0x01);
}

HostRebootPCIReset::HostRebootPCIReset() : HostRebootMethod(), _con("pcicfg"), _sess(_con) {
}

void HostRebootPCIReset::reboot() {
	Serial::get() << "Trying reboot via PCI...\n";
	_sess.reboot();
}

HostRebootACPI::HostRebootACPI() : HostRebootMethod(), _method(), _value(), _addr(),
		_con("acpi"), _sess(_con), _ports(), _ds() {
	Serial::get() << "Trying reboot via ACPI...\n";
	ACPI::RSDT *rsdt = _sess.find_table(String("FACP"));
	if(!rsdt)
		throw Exception(E_NOT_FOUND,"FACP not found");

	char *table = reinterpret_cast<char*>(rsdt);
	if(rsdt->length < 129)
		throw Exception(E_NOT_FOUND,"FACP too small");
	if(~table[113] & 0x4)
		throw Exception(E_NOT_FOUND,"Reset unsupported");
	if(table[117] != 8)
		throw Exception(E_NOT_FOUND,"Register width invalid");
	if(table[118] != 0)
		throw Exception(E_NOT_FOUND,"Register offset invalid");
	if(table[119] > 1)
		throw Exception(E_NOT_FOUND,"Byte access needed");

	_method = table[116];
	_value = table[128];
	_addr = *reinterpret_cast<uint64_t*>(table + 120);
	switch(_method) {
		case 0:
			_ds = new DataSpace(ExecEnv::PAGE_SIZE,DataSpaceDesc::LOCKED,DataSpaceDesc::RW,_addr);
			break;
		case 1:
			if(_addr >= 0x10000)
				throw Exception(E_NOT_FOUND,"PIO out of range");
			_ports = new Ports(_addr,1);
			break;
		case 2:
			_ports = new Ports(0xcf8,8);
			break;
		default:
			throw Exception(E_NOT_FOUND,"Wrong access type");
	}
}

HostRebootACPI::~HostRebootACPI() {
	delete _ports;
	delete _ds;
}

void HostRebootACPI::reboot() {
	switch(_method) {
		case 0:
			*reinterpret_cast<volatile uint8_t*>(_ds->virt()) = _value;
			break;
		case 1:
			_ports->out<uint8_t>(_value);
			break;
		case 2:
			uint32_t value = (_addr & 0x1f00000000ull) >> (32 - 11);
			value |= (_addr & 0x70000) >> (16 - 8);
			value |= _addr & 0x3c;
			_ports->out<uint32_t>(value,0);
			_ports->out<uint8_t>(_value,4 | (_addr & 0x3));
			break;
	}
}