#ifndef OCELOT_SRC_OCELOT_H
#define OCELOT_SRC_OCELOT_H

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <set>
#include <boost/thread/thread.hpp>

enum FreeType { NORMAL, FREE, NEUTRAL };

typedef struct {
  std::string userid;
  std::string peer_id;
  std::string user_agent;
  std::string ip_port;
  std::string ip;
  unsigned int port;
  long long uploaded;
  long long downloaded;
  uint64_t left;
  time_t last_announced;
  time_t first_announced;
  unsigned int announces;
} Peer;

typedef std::map<std::string, Peer> PeerList;

typedef struct {
  std::string id;
  time_t last_seeded;
  long long balance;
  int completed;
  FreeType free_torrent;
  PeerList seeders;
  PeerList leechers;
  std::string last_selected_seeder;
  std::set<std::string> tokened_users;
  time_t last_flushed;
} Torrent;

typedef struct {
  std::string id;
  bool can_leech;
} User;


typedef std::unordered_map<std::string, Torrent> TorrentList;
typedef std::unordered_map<std::string, User> UserList;
typedef std::vector<std::string> Whitelist;

#endif // OCELOT_SRC_OCELOT_H
