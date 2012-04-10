#ifndef OCELOT_SRC_CONFIG_H
#define OCELOT_SRC_CONFIG_H

#include <string>

class Config {
public:
  static const constexpr char *kHost = "127.0.0.1";
  static const unsigned int kPort = 34000;
  static const unsigned int kMaxConnections = 512;
  static const unsigned int kMaxReadBuffer = 4096;
  static const unsigned int kTimeoutInterval = 20;
  static const unsigned int kScheduleInterval = 3;
  static const unsigned int kMaxMiddlemen = 5000;
  
  static const unsigned int kAnnounceInterval = 1800;
  static const int kPeersTimeout = kAnnounceInterval * 1.5;
		
  static const unsigned int kReapPeersInterval = 1800;
  
  // Site Communications
  static const constexpr char *kSiteHost = "127.0.0.1";
  static const int kSitePort = 80;
  
  static const constexpr char *kSitePassword = "********************************";

  static const constexpr char *kLogFile = "debug.log";

  Config();
};

#endif // OCELOT_SRC_CONFIG_H
