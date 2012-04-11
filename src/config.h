#ifndef OCELOT_SRC_CONFIG_H
#define OCELOT_SRC_CONFIG_H

#include <string>

class Config {
public:
  Config(int argc, char **argv);

  std::string kHost;
  unsigned int kPort;
  unsigned int kMaxConnections;
  unsigned int kMaxReadBuffer;
  unsigned int kTimeoutInterval;
  unsigned int kScheduleInterval;
  unsigned int kMaxMiddlemen;
  
  unsigned int kAnnounceInterval;
  int kPeersTimeout;
		
  unsigned int kReapPeersInterval;
  
  // Site Communications
  std::string kSiteHost;
  int kSitePort;
  
  std::string kSitePassword;

  std::string kLogFile;
};

#endif // OCELOT_SRC_CONFIG_H
