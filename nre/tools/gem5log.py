#!/usr/bin/env python

import sys
import subprocess
import re
from os.path import basename

funcs = []

def addr2func(funcs, addr):
    if len(addr2func.last) > 0:
        if addr >= addr2func.last[0] and addr < addr2func.last[0] + addr2func.last[1]:
            return addr2func.last

    for f in funcs:
        if addr >= f[0] and addr < f[0] + f[1]:
            addr2func.last = f
            return f
    return []

addr2func.last = []

def funcname(funcs, addr):
    f = addr2func(funcs, addr)
    if len(f) == 0:
        return '<Unknown>: %x' % addr
    return '\033[1m%s\033[0m @ %s+0x%x' % (basename(f[3]), f[2].decode('ASCII'), addr - f[0])

# read symbols
for i in range(1, len(sys.argv)):
    proc = subprocess.Popen(['nm', '-SC', sys.argv[i]], stdout=subprocess.PIPE)
    while True:
        line = proc.stdout.readline()
        if not line:
            break

        m = re.match(b'^([a-f0-9]+) ([a-f0-9]+) (t|T|w|W) ([A-Za-z0-9_:\.\~ ]+)', line)
        if m:
            funcs.append([
                int(m.group(1), 16),
                int(m.group(2), 16),
                m.group(4),
                sys.argv[i]
            ])

# sort symbols by address
funcs = sorted(funcs, key=lambda f: f[0])

# read log lines
while True:
    line = sys.stdin.readline()
    if not line:
        break

    m = re.match('^\s*(\d+): (system\.cpu\d*) (\S+) : 0x([a-f0-9]+)\s*(?:@\s*)?.+?\s*:\s*(.*)$', line)
    if m:
        func = funcname(funcs, int(m.group(4), 16));
        print("%7s: %s: %s : %s" % (m.group(1), m.group(2), func, m.group(5)));
    else:
        print(line.rstrip())
