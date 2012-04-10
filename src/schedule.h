#ifndef OCELOT_SRC_SCHEDULE_H
#define OCELOT_SRC_SCHEDULE_H

#include <ev++.h>
#include <string>
#include <iostream>

class Schedule {
  public:
    Schedule(ConnectionMother *mother, Worker *worker, SiteComm *site_comm);
    void Handle(ev::timer &watcher, int events_flags);

  private:
    ConnectionMother *mother_;
    Worker *worker_;
    SiteComm *site_comm_;
    int last_opened_connections_;
    int counter_;
    
    time_t next_flush_;
    time_t next_reap_peers_;
};

#endif // OCELOT_SRC_SCHEDULE_H
