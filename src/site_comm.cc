#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include "config.h"
#include "site_comm.h"

using boost::asio::ip::tcp;

SiteComm::SiteComm(Config *config) : config_(config)
{
}

SiteComm::~SiteComm()
{
}

void SiteComm::Init()
{
}

Whitelist SiteComm::GetWhitelist()
{
  return Whitelist();
}

TorrentList SiteComm::GetTorrents()
{
  return TorrentList();
}

UserList SiteComm::GetUsers()
{
  return UserList();
}

void SiteComm::LoadTokens(const TorrentList &torrent_list)
{
}

bool SiteComm::ExpireToken(std::string torrent, std::string user)
{
  return true;
}

bool SiteComm::AllClear()
{
  return true;
}

void SiteComm::Flush()
{
}

void SiteComm::RecordUser(std::string &record)
{
}

void SiteComm::RecordTorrent(std::string &record)
{
}

void SiteComm::RecordSnatch(std::string &record)
{
}

void SiteComm::RecordPeer(std::string &record, std::string &ip, std::string &peer_id, std::string &useragent)
{
}

void SiteComm::RecordToken(std::string &record)
{
}

void SiteComm::RecordPeerHist(std::string &record, std::string &peer_id, std::string tid)
{
}
