#ifndef OCELOT_SRC_EVENTS_H
#define OCELOT_SRC_EVENTS_H

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <cstring>
#include <ev++.h>
#include "site_comm.h"
#include "worker.h"
#include "config.h"

/*
We have three classes - the mother, the middlemen, and the worker
THE MOTHER
  The mother is called when a client opens a connection to the server. 
  It creates a middleman for every new connection, which will be called
  when its socket is ready for reading.
THE MIDDLEMEN
  Each middleman hang around until data is written to its socket. It then
  reads the data and sends it to the worker. When it gets the response, it
  gets called to write its data back to the client.
THE WORKER
  The worker gets data from the middleman, and returns the response. It 
  doesn't concern itself with silly things like sockets. 
  
  see worker.h for the worker.
*/




// THE MOTHER - Spawns connection middlemen
class ConnectionMother {
  public: 
    ConnectionMother(Worker *worker, Config *config, SiteComm *site_comm);
    ~ConnectionMother();
    
    void IncrementOpenConnections() { open_connections_++; }
    void DecrementOpenConnections() { open_connections_--; }
    
    int open_connections() { return open_connections_; }
    int opened_connections() { return opened_connections_; }

    void HandleConnect(ev::io &watcher, int events_flags);

  private:
    int listen_socket_;
    sockaddr_in address_;
    socklen_t addr_len_;
    Worker *worker_;
    Config *config_;
    SiteComm *site_comm_;
    ev::timer schedule_event_;
    
    unsigned long opened_connections_;
    unsigned int open_connections_;
};

// THE MIDDLEMAN
// Created by connection_mother
// Add their own watchers to see when sockets become readable
class ConnectionMiddleman {
  public:
    ConnectionMiddleman(int &listen_socket, sockaddr_in &address, socklen_t &addr_len, Worker *worker, ConnectionMother *mother, Config *config);
    ~ConnectionMiddleman();
  
    void HandleRead(ev::io &watcher, int events_flags);
    void HandleWrite(ev::io &watcher, int events_flags);
    void HandleTimeout(ev::timer &watcher, int events_flags);

  private:
    int connect_sock_;
    ev::io read_event_;
    ev::io write_event_;
    ev::timer timeout_event_;
    std::string response_;
    
    Config *config_;
    ConnectionMother *mother_;
    Worker *worker_;
    sockaddr_in client_addr_;
};

#endif // OCELOT_SRC_EVENTS_H
