#include "ocelot.h"
#include "config.h"
#include "worker.h"
#include "events.h"
#include "schedule.h"
#include "logger.h"
#include "site_comm.h"

static ConnectionMother *mother;
static Worker *worker;
static Logger *logger;
static SiteComm *site_comm;

static void sig_handler(int sig)
{
  std::cout << "Caught SIGINT/SIGTERM" << std::endl;
  
  if (worker->Signal(sig))
  {
    exit(0);
  }
}

int main() {
  #ifdef ENABLE_UPDATE
  std::cout << "WARNING: This version of ParselOcelot was compiled with UPDATE support enabled!" << std::endl;
  #endif // ENABLE_UPDATE

  Config config;
  
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  
  logger = new Logger(config.kLogFile);
  
  SiteComm sc(config);
  site_comm = &sc;
  
  Whitelist whitelist = site_comm->GetWhitelist();
  std::cout << "Loaded " << whitelist.size() << " clients into the whitelist" << std::endl;
  if(whitelist.size() == 0)
  {
    std::cout << "Assuming no whitelist desired, disabling" << std::endl;
  }
	
  UserList users_list = site_comm->GetUsers();
  std::cout << "Loaded " << users_list.size() << " users" << std::endl;
  
  TorrentList torrents_list = site_comm->GetTorrents();
  std::cout << "Loaded " << torrents_list.size() << " torrents" << std::endl;
  
  site_comm->LoadTokens(torrents_list);
  
  // Create worker object, which handles announces and scrapes and all that jazz
  worker = new Worker(torrents_list, users_list, whitelist, &config, site_comm);
  
  // Create connection mother, which binds to its socket and handles the event stuff
  mother = new ConnectionMother(worker, &config, site_comm);
  
  return 0;
}
