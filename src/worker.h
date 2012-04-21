#ifndef OCELOT_SRC_WORKER_H
#define OCELOT_SRC_WORKER_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <list>
#include <boost/unordered_map.hpp>
#include <iostream>
#include <fstream>
#include "site_comm.h"

enum TrackerStatus { OPEN, PAUSED, CLOSING }; // tracker status

class Worker {
  public:
    Worker(TorrentList &torrents, UserList &users, std::vector<std::string> &whitelist, SiteComm *site_comm, Config *config);
    std::string Work(std::string &input, std::string &ip);
    std::string Error(std::string err);
    std::string Announce(Torrent &torrent, User &user, boost::unordered_map<std::string, std::string> &params, boost::unordered_map<std::string, std::string> &headers, std::string &ip);
    std::string Scrape(const std::list<std::string> &infohashes);
    #ifdef ENABLE_UPDATE
    std::string Update(boost::unordered_map<std::string, std::string> &params);
    #endif // ENABLE_UPDATE

    bool Signal(int sig);

    TrackerStatus status() { return status_; }

    void ReapPeers();

  private:
    void DoReapPeers();

    TorrentList torrents_list_;
    UserList users_list_;
    std::vector<std::string> whitelist_;
    TrackerStatus status_;
    SiteComm *site_comm_;
    Config *config_;
};

#endif // OCELOT_SRC_WORKER_H
