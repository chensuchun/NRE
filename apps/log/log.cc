/*
 * (c) 2012 Nils Asmussen <nils@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <service/Service.h>
#include <service/Consumer.h>
#include <stream/Serial.h>
#include <kobj/GlobalEc.h>
#include <kobj/Sc.h>
#include <util/Cycler.h>

using namespace nul;

class LogSessionData : public SessionData {
	enum {
		MAX_LINE_LEN	= 100
	};

public:
	LogSessionData(Service *s,size_t id,capsel_t caps,Pt::portal_func func)
		: SessionData(s,id,caps,func), _bufpos(0), _buf(),
		  _ec(receiver,_cpus.next()->log_id()), _sc(), _cons(), _ds(), _sm() {
		_ec.set_tls<capsel_t>(Ec::TLS_PARAM,caps);
	}
	virtual ~LogSessionData() {
		delete _ds;
		delete _cons;
		delete _sc;
	}

	virtual void invalidate() {
		if(_cons)
			_cons->stop();
	}

	Consumer<char> *cons() {
		return _cons;
	}

	bool put(char c) {
		if(_bufpos < sizeof(_buf) - 1)
			_buf[_bufpos++] = c;
		// explicit newline
		if(_bufpos == sizeof(_buf) - 1)
			_buf[_bufpos++] = '\n';
		return c == '\n' || _bufpos == sizeof(_buf);
	}
	void flush();

protected:
	virtual void accept_ds(DataSpace *ds) {
		ScopedLock<UserSm> guard(&_sm);
		if(_ds != 0)
			throw Exception(E_EXISTS);
		_ds = ds;
		_cons = new Consumer<char>(ds);
		_sc = new Sc(&_ec,Qpd());
		_sc->start();
	}

private:
	static void receiver(void *);

	size_t _bufpos;
	char _buf[MAX_LINE_LEN + 1];
	GlobalEc _ec;
	Sc *_sc;
	Consumer<char> *_cons;
	DataSpace *_ds;
	UserSm _sm;
	static const char *_colors[];
	static Cycler<CPU::iterator,LockPolicyDefault<SpinLock> > _cpus;
};

class LogService : public Service {
public:
	LogService() : Service("log") {
	}

	UserSm &sm() {
		return _sm;
	}

private:
	virtual SessionData *create_session(size_t id,capsel_t caps,Pt::portal_func func) {
		return new LogSessionData(this,id,caps,func);
	}

	UserSm _sm;
};

const char *LogSessionData::_colors[] = {
	"31","32","33","34","35","36","37","30"
};
Cycler<CPU::iterator,LockPolicyDefault<SpinLock> > LogSessionData::_cpus(CPU::begin(),CPU::end());

static LogService *log;

void LogSessionData::flush() {
	ScopedLock<UserSm> guard(&log->sm());
	Serial::get() << "\e[0;" << _colors[id() % 8] << "m[" << id() << "] ";
	for(size_t i = 0; i < _bufpos; ++i)
		Serial::get() << _buf[i];
	Serial::get() << "\e[0m";
	_bufpos = 0;
}

void LogSessionData::receiver(void *) {
	capsel_t caps = Ec::current()->get_tls<word_t>(Ec::TLS_PARAM);
	ScopedLock<RCULock> guard(&RCU::lock());
	LogSessionData *sess = log->get_session<LogSessionData>(caps);
	Consumer<char> *cons = sess->cons();
	for(char *c; (c = cons->get()) != 0; cons->next()) {
		if(sess->put(*c))
			sess->flush();
	}
}

int main() {
	log = new LogService();
	for(CPU::iterator it = CPU::begin(); it != CPU::end(); ++it)
		log->provide_on(it->log_id());
	log->reg();
	log->wait();
	return 0;
}