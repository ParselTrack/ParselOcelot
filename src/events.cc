#include <cerrno>
#include "ocelot.h"
#include "config.h"
#include "site_comm.h"
#include "worker.h"
#include "events.h"
#include "schedule.h"



// Define the connection mother (first half) and connection middlemen (second half)

//TODO Better errors

//---------- Connection mother - spawns middlemen and lets them deal with the connection

ConnectionMother::ConnectionMother(Worker *worker, SiteComm *site_comm) : worker_(worker), site_comm_(site_comm) {
	open_connections_ = 0;
	opened_connections_ = 0;
	
	memset(&address_, 0, sizeof(address_));
	addr_len_ = sizeof(address_);
	
	listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	
	// Stop old sockets from hogging the port
	int yes = 1;
	if(setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		std::cout << "Could not reuse socket" << std::endl;
	}
	
	// Create libev event loop
	ev::io event_loop_watcher;
	
	event_loop_watcher.set<ConnectionMother, &ConnectionMother::HandleConnect>(this);
	event_loop_watcher.start(listen_socket_, ev::READ);
	
	// Get ready to bind
	address_.sin_family = AF_INET;
	//address.sin_addr.s_addr = inet_addr(conf->host.c_str()); // htonl(INADDR_ANY)
	address_.sin_addr.s_addr = htonl(INADDR_ANY);
	address_.sin_port = htons(Config::kPort);
	
	// Bind
	if(bind(listen_socket_, (sockaddr *) &address_, sizeof(address_)) == -1) {
		std::cout << "Bind failed " << errno << std::endl;
	}
	
	// Listen
	if(listen(listen_socket_, Config::kMaxConnections) == -1) {
		std::cout << "Listen failed" << std::endl;
	}
	
	// Set non-blocking
	int flags = fcntl(listen_socket_, F_GETFL);
	if(flags == -1) {
		std::cout << "Could not get socket flags" << std::endl;
	}
	if(fcntl(listen_socket_, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cout << "Could not set non-blocking" << std::endl;
	}
	
	// Create libev timer
	Schedule timer(this, worker_, site_comm);
	
	schedule_event_.set<Schedule, &Schedule::Handle>(&timer);
	schedule_event_.set(Config::kScheduleInterval, Config::kScheduleInterval); // After interval, every interval
	schedule_event_.start();
	
	std::cout << "Sockets up, starting event loop!" << std::endl;
	ev_loop(ev_default_loop(0), 0);
}


void ConnectionMother::HandleConnect(ev::io &watcher, int events_flags) {
	// Spawn a new middleman
	if(open_connections_ < Config::kMaxMiddlemen) {
		opened_connections_++;
		new ConnectionMiddleman(listen_socket_, address_, addr_len_, worker_, this);
	}
}

ConnectionMother::~ConnectionMother()
{
	close(listen_socket_);
}







//---------- Connection middlemen - these little guys live until their connection is closed

ConnectionMiddleman::ConnectionMiddleman(int &listen_socket, sockaddr_in &address, socklen_t &addr_len, Worker *worker, ConnectionMother *mother): 
	mother_(mother), worker_(worker) {
	
	connect_sock_ = accept(listen_socket, (sockaddr *) &address, &addr_len);
	if(connect_sock_ == -1) {
		std::cout << "Accept failed, errno " << errno << ": " << strerror(errno) << std::endl;
		mother->IncrementOpenConnections(); // destructor decrements open connections
		delete this;
		return;
	}
	
	// Set non-blocking
	int flags = fcntl(connect_sock_, F_GETFL);
	if(flags == -1) {
		std::cout << "Could not get connect socket flags" << std::endl;
	}
	if(fcntl(connect_sock_, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cout << "Could not set non-blocking" << std::endl;
	}
	
	// Get their info
	if(getpeername(connect_sock_, (sockaddr *) &client_addr_, &addr_len) == -1) {
		//std::cout << "Could not get client info" << std::endl;
	}
	
	
	read_event_.set<ConnectionMiddleman, &ConnectionMiddleman::HandleRead>(this);
	read_event_.start(connect_sock_, ev::READ);
	
	// Let the socket timeout in timeout_interval seconds
	timeout_event_.set<ConnectionMiddleman, &ConnectionMiddleman::HandleTimeout>(this);
	timeout_event_.set(Config::kTimeoutInterval, 0);
	timeout_event_.start();
	
	mother->IncrementOpenConnections();
}

ConnectionMiddleman::~ConnectionMiddleman() {
	close(connect_sock_);
	mother_->DecrementOpenConnections();
}

// Handler to read data from the socket, called by event loop when socket is readable
void ConnectionMiddleman::HandleRead(ev::io &watcher, int events_flags) {
	read_event_.stop();
	
	char buffer[Config::kMaxReadBuffer + 1];
	memset(buffer, 0, Config::kMaxReadBuffer + 1);
	int status = recv(connect_sock_, &buffer, Config::kMaxReadBuffer, 0);
	
	if(status == -1) {
		delete this;
		return;
	}
	
	std::string stringbuf = buffer;
	
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(client_addr_.sin_addr), ip, INET_ADDRSTRLEN);
	std::string ip_str = ip;
	
	//--- CALL WORKER
	response_ = worker_->Work(stringbuf, ip_str);
	
	// Find out when the socket is writeable. 
	// The loop in connection_mother will call handle_write when it is. 
	write_event_.set<ConnectionMiddleman, &ConnectionMiddleman::HandleWrite>(this);
	write_event_.start(connect_sock_, ev::WRITE);
}

// Handler to write data to the socket, called by event loop when socket is writeable
void ConnectionMiddleman::HandleWrite(ev::io &watcher, int events_flags) {
	write_event_.stop();
	timeout_event_.stop();
	std::string http_response = "HTTP/1.1 200\r\nServer: Ocelot 1.0\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n";
	http_response += response_;
	send(connect_sock_, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	delete this;
}

// After a middleman has been alive for timout_interval seconds, this is called
void ConnectionMiddleman::HandleTimeout(ev::timer &watcher, int events_flags) {
	timeout_event_.stop();
	read_event_.stop();
	write_event_.stop();
	delete this;
}
