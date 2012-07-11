/*
 * (c) 2012 Nils Asmussen <nils@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <ipc/Connection.h>
#include <stream/OStringStream.h>
#include <services/Timer.h>
#include <util/Clock.h>

#include "ViewSwitcher.h"
#include "ConsoleService.h"
#include "ConsoleSessionData.h"

using namespace nre;

char ViewSwitcher::_backup[Screen::COLS * 2];
char ViewSwitcher::_buffer[Screen::COLS + 1];

ViewSwitcher::ViewSwitcher() : _ds(DS_SIZE,DataSpaceDesc::ANONYMOUS,DataSpaceDesc::RW),
		_prod(&_ds,true,true), _cons(&_ds,false), _ec(switch_thread,CPU::current().log_id()),
		_sc(&_ec,Qpd()) {
	_ec.set_tls<ViewSwitcher*>(Thread::TLS_PARAM,this);
}

void ViewSwitcher::switch_to(ConsoleSessionData *from,ConsoleSessionData *to) {
	SwitchCommand cmd;
	cmd.oldsessid = from ? from->id() : -1;
	cmd.sessid = to->id();
	_prod.produce(cmd);
}

void ViewSwitcher::switch_thread(void*) {
	ViewSwitcher *vs = Thread::current()->get_tls<ViewSwitcher*>(Thread::TLS_PARAM);
	// note that we can do that here (although we're created from ConsoleService) because we start
	// this thread after the instance has been created (in ConsoleService::init).
	ConsoleService *srv = ConsoleService::get();
	Clock clock(1000);
	Connection con("timer");
	TimerSession timer(con);
	timevalue_t until = 0;
	size_t sessid = 0;
	bool tag_done = false;
	while(1) {
		// are we finished?
		if(until && clock.source_time() >= until) {
			ScopedLock<RCULock> guard(&RCU::lock());
			ConsoleSessionData *sess = srv->get_session_by_id<ConsoleSessionData>(sessid);
			if(sess) {
				// finally swap to that session. i.e. give him direct screen access
				sess->to_front();
			}
			until = 0;
		}

		// either block until the next request, or - if we're switching - check for new requests
		if(until == 0 || vs->_cons.has_data()) {
			SwitchCommand *cmd = vs->_cons.get();
			// if there is an old one, make a backup and detach him from screen
			if(cmd->oldsessid != (size_t)-1) {
				assert(cmd->oldsessid == sessid);
				ScopedLock<RCULock> guard(&RCU::lock());
				ConsoleSessionData *old = srv->get_session_by_id<ConsoleSessionData>(cmd->oldsessid);
				if(old) {
					// if we have already given him the screen, take it away
					if(until == 0)
						old->to_back();
				}
			}
			sessid = cmd->sessid;
			// show the tag for 1sec
			until = clock.source_time(SWITCH_TIME);
			tag_done = false;
			vs->_cons.next();
		}

		{
			ScopedLock<RCULock> guard(&RCU::lock());
			ConsoleSessionData *sess = srv->get_session_by_id<ConsoleSessionData>(sessid);
			// if the session is dead, stop switching to it
			if(!sess) {
				until = 0;
				continue;
			}

			// repaint all lines from the buffer except the first
			uintptr_t offset = Screen::TEXT_OFF + sess->page() * Screen::PAGE_SIZE;
			if(sess->out_ds()) {
				memcpy(reinterpret_cast<void*>(srv->screen()->mem().virt() + offset + Screen::COLS * 2),
						reinterpret_cast<void*>(sess->out_ds()->virt() + offset + Screen::COLS * 2),
						Screen::PAGE_SIZE - Screen::COLS * 2);
			}

			if(!tag_done) {
				// write tag into buffer
				memset(_buffer,0,sizeof(_buffer));
				OStringStream os(_buffer,sizeof(_buffer));
				os << "Console " << sessid << "," << sess->page();

				// write console tag
				char *screen = reinterpret_cast<char*>(srv->screen()->mem().virt() + offset);
				char *s = _buffer;
				for(uint x = 0; x < Screen::COLS; ++x, ++s) {
					screen[x * 2] = *s ? *s : ' ';
					screen[x * 2 + 1] = COLOR;
				}
				sess->activate();
				tag_done = true;
			}
		}

		// wait 25ms
		timer.wait_until(clock.source_time(REFRESH_DELAY));
	}
}