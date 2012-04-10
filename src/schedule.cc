#include "ocelot.h"
#include "config.h"
#include "worker.h"
#include "events.h"
#include "schedule.h"
#include "site_comm.h"


Schedule::Schedule(ConnectionMother *mother, Worker *worker, SiteComm *site_comm) : mother_(mother), worker_(worker), site_comm_(site_comm) {
	counter_ = 0;
	last_opened_connections_ = 0;
	
	next_reap_peers_ = time(NULL) + Config::kReapPeersInterval + 40;
}

//---------- Schedule - gets called every schedule_interval seconds
void Schedule::Handle(ev::timer &watcher, int events_flags) {
	
	if(counter_ % 20 == 0) {
		std::cout << "Schedule run #" << counter_ << " - open: " << mother_->open_connections() << ", opened: " 
		<< mother_->opened_connections() << ", speed: "
		<< ((mother_->opened_connections()-last_opened_connections_)/Config::kScheduleInterval) << "/s" << std::endl;
	}

	if ((worker_->status() == CLOSING) && site_comm_->AllClear()) {
		std::cout << "all clear, shutting down" << std::endl;
		exit(0);
	}

	last_opened_connections_ = mother_->opened_connections();
	
	site_comm_->Flush();

	time_t cur_time = time(NULL);

	if(cur_time > next_reap_peers_) {
		worker_->ReapPeers();
		next_reap_peers_ = cur_time + Config::kReapPeersInterval;
	}

	counter_++;
}
