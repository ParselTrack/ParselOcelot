#ifndef OCELOT_SRC_SITE_COMM_H
#define OCELOT_SRC_SITE_COMM_H

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "ocelot.h"
#include "config.h"

class SiteComm {
public:
  SiteComm();
  ~SiteComm();
  Whitelist GetWhitelist();
  TorrentList GetTorrents();
  UserList GetUsers();
  void LoadTokens(const TorrentList &torrent_list);
  bool ExpireToken(std::string torrent, std::string user);
  bool AllClear();
  void Flush();

  void RecordUser(std::string &record); 
  void RecordTorrent(std::string &record);
  void RecordSnatch(std::string &record);
  void RecordPeer(std::string &record, std::string &ip, std::string &peer_id, std::string &useragent);
  void RecordToken(std::string &record);
  void RecordPeerHist(std::string &record, std::string &peer_id, std::string tid);

  boost::mutex torrent_list_mutex;

private:
  Config config_;
};

#endif // OCELOT_SRC_SITE_COMM_H
