/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <util/Profiler.h>
#include <Syscalls.h>
#include <Test.h>
#include <cstdlib>

#include "Syscall.h"

using namespace nre;
using namespace nre::test;

static void test_syscall();

static const uint COUNT = 1000;

const TestCase syscall = {
    "Reference syscalls", test_syscall
};

static void test_syscall() {
    AvgProfiler prof(COUNT);
    for(uint i = 0; i < COUNT; i++) {
        prof.start();
        Syscalls::noop();
        prof.stop();
    }
    WVPRINT("Using portal_empty:");
    WVPERF(prof.avg(), "cycles");
    WVPRINT("min: " << prof.min());
    WVPRINT("max: " << prof.max());
}
