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

#pragma once

#include <util/Util.h>
#include <util/Math.h>
#include <Assert.h>

namespace nre {

class Profiler {
public:
    typedef uint64_t time_t;

    explicit Profiler() : _rdtsc(measure()), _start(0), _min(~0ULL), _max(0) {
    }
    virtual ~Profiler() {
    }

    time_t rdtsc() const {
        return _rdtsc;
    }
    time_t min() const {
        return _min;
    }
    time_t max() const {
        return _max;
    }

    virtual void start() {
        _start = Util::tsc();
    }
    virtual time_t stop() {
        time_t time = Util::tsc() - _start;
        time = time > _rdtsc ? time - _rdtsc : 0;
        _min = Math::min(_min, time);
        _max = Math::max(_max, time);
        return time;
    }

protected:
    static time_t measure() {
        time_t tic = Util::tsc();
        time_t tac = Util::tsc();
        return tac - tic;
    }

    time_t _rdtsc;
    time_t _start;
    time_t _min;
    time_t _max;
};

class AvgProfiler : public Profiler {
public:
    explicit AvgProfiler(size_t count, size_t warmup = 5, time_t outlier = ~0ULL)
        : _count(count), _warmup(warmup), _outlier(outlier), _pos(0), _results(new time_t[count]) {
    }
    virtual ~AvgProfiler() {
        delete[] _results;
    }

    time_t result(size_t i) const {
        return _results[i];
    }

    time_t min() const {
        time_t val = ~0ULL;
        for(size_t i = _warmup; i < _count; i++) {
            if(_results[i] < val)
                val = _results[i];
        }
        return val;
    }

    time_t max() const {
        time_t val = 0;
        for(size_t i = _warmup; i < _count; i++) {
            if(_results[i] > val && _results[i] <= _outlier)
                val = _results[i];
        }
        return val;
    }

    time_t avg() const {
        time_t avg = 0;
        size_t count = _count - _warmup;
        for(size_t i = _warmup; i < _count; i++) {
            if(_results[i] <= _outlier)
                avg += _results[i];
            else
                count--;
        }
        return count == 0 ? 0 : avg / count;
    }

    float variance() const {
        time_t sum = 0;
        time_t average = avg();
        size_t count = _count - _warmup;
        for(size_t i = _warmup; i < _count; i++) {
            if(_results[i] <= _outlier) {
                int64_t val = (int64_t)_results[i] - average;
                sum += val * val;
            }
            else
                count--;
        }
        return (float)sum / count;
    }

    virtual void start() {
        assert(_pos < _count);
        Profiler::start();
    }
    virtual time_t stop() {
        time_t time = Util::tsc() - _start;
        time = time > _rdtsc ? time - _rdtsc : 0;
        _results[_pos++] = time;
        return time;
    }

private:
    size_t _count;
    size_t _warmup;
    time_t _outlier;
    size_t _pos;
    time_t *_results;
};

}
