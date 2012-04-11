#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmath>
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <list>
#include <vector>
#include <set>
#include <algorithm>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/bind.hpp>
#include "ocelot.h"
#include "config.h"
#include "worker.h"
#include "misc_functions.h"
#include "site_comm.h"
#include "logger.h"

//---------- Worker - does stuff with input
Worker::Worker(TorrentList &torrents, UserList &users, std::vector<std::string> &whitelist, SiteComm *site_comm, Config *config) :
		torrents_list_(torrents), users_list_(users), whitelist_(whitelist), site_comm_(site_comm), config_(config) {
	status_ = OPEN;
}

bool Worker::Signal(int sig) {
	if (status_ == OPEN) {
		status_ = CLOSING;
		std::cout << "closing tracker... press Ctrl-C again to terminate" << std::endl;
		return false;
	} else if (status_ == CLOSING) {
		std::cout << "shutting down uncleanly" << std::endl;
		return true;
	} else {
		return false;
	}
}
std::string Worker::Work(std::string &input, std::string &clientip) {
	unsigned int input_length = input.length();
	std::string ip = clientip;
	
	//---------- Parse request - ugly but fast. Using substr exploded.
	if(input_length < 60) { // Way too short to be anything useful
		return this->Error("GET string too short");
	}
	
	size_t pos = 5; // skip GET /
	
	// Get the passkey
	std::string passkey;
	passkey.reserve(32);
	if(input[37] != '/') {
		return this->Error("Malformed announce");
	}
	
	for(; pos < 37; pos++) {
		passkey.push_back(input[pos]);
	}
	
	pos = 38;
	
	// Get the action
	enum action_t {
		INVALID = 0, ANNOUNCE, SCRAPE, UPDATE
	};
	action_t action = INVALID;
	
	switch(input[pos]) {
		case 'a':
			action = ANNOUNCE;
			pos += 9;
			break;
		case 's':
			action = SCRAPE;
			pos += 7;
			break;
		#ifdef ENABLE_UPDATE
		case 'u':
			action = UPDATE;
			pos += 7;
			break;
		#endif // ENABLE_UPDATE
	}
	if(action == INVALID) {
		return this->Error("invalid action");
	}

	if ((status_ != OPEN) && (action != UPDATE)) {
		return this->Error("The tracker is temporarily unavailable.");
	}
	
	// Parse URL params
	std::list<std::string> infohashes; // For scrape only
	
	std::unordered_map<std::string, std::string> params;
	std::string key;
	std::string value;
	bool parsing_key = true; // true = key, false = value
	
	for(; pos < input_length; ++pos) {
		if(input[pos] == '=') {
			parsing_key = false;
		} else if(input[pos] == '&' || input[pos] == ' ') {
			parsing_key = true;
			if(action == SCRAPE && key == "info_hash") {
				infohashes.push_back(value);
			} else {

				params[key] = value;
			}
			key.clear();
			value.clear();
			if(input[pos] == ' ') {
				break;
			}
		} else {
			if(parsing_key) {
				key.push_back(input[pos]);
			} else {
				value.push_back(input[pos]);
			}
		}
	}
	
	pos += 10; // skip HTTP/1.1 - should probably be +=11, but just in case a client doesn't send \r
	
	// Parse headers
	std::unordered_map<std::string, std::string> headers;
	parsing_key = true;
	bool found_data = false;
	
	for(; pos < input_length; ++pos) {
		if(input[pos] == ':') {
			parsing_key = false;
			++pos; // skip space after :
		} else if(input[pos] == '\n' || input[pos] == '\r') {
			parsing_key = true;
			
			if(found_data) {
				found_data = false; // dodge for getting around \r\n or just \n
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);
				headers[key] = value;
				key.clear();
				value.clear();
			}
		} else {
			found_data = true;
			if(parsing_key) {
				key.push_back(input[pos]);
			} else {
				value.push_back(input[pos]);
			}
		}
	}

	// Deal with X-Real-IP/X-Forwarded-For
	if (config_->kAllowReverseProxies) {
		bool allowed = false;

		// Check if the clientip is an allowed reverse proxy
		for (auto itr = config_->kTrustedProxyIps.begin();
				 itr != config_->kTrustedProxyIps.end(); ++itr) {
			if (*itr == clientip) {
				allowed = true;
				break;
			}
		}

		// From a trusted reverse proxy
		if (allowed) {
			auto ip_itr = headers.find("x-real-ip");

			if (ip_itr != headers.end()) {
				ip = ip_itr->second;
			} else {
				ip_itr = headers.find("x-forwarded-for");

				if (ip_itr != headers.end()) {
					ip = ip_itr->second;
				}
			}
		}
	}
	
	#ifdef ENABLE_UPDATE
	if(action == UPDATE) {
		if(passkey == config_->kSitePassword) {
			return this->Update(params);
		} else {
			return this->Error("Authentication failure");
		}
	}
	#endif // ENABLE_UPDATE
	
	// Either a scrape or an announce
	
	UserList::iterator u = users_list_.find(passkey);
	if(u == users_list_.end()) {
		return this->Error("passkey not found");
	}
	
	if(action == ANNOUNCE) {
		boost::mutex::scoped_lock lock(site_comm_->torrent_list_mutex);
		// Let's translate the infohash into something nice
		// info_hash is a url encoded (hex) base 20 number
		std::string info_hash_decoded = hex_decode(params["info_hash"]);
		TorrentList::iterator tor = torrents_list_.find(info_hash_decoded);
		if(tor == torrents_list_.end()) {
			return this->Error("unregistered torrent");
		}
		return this->Announce(tor->second, u->second, params, headers, ip);
	} else {
		return this->Scrape(infohashes);
	}
}

std::string Worker::Error(std::string err) {
	std::string output = "d14:failure reason";
	output += inttostr(err.length());
	output += ':';
	output += err;
	output += 'e';
	return output;
}

std::string Worker::Announce(Torrent &torrent, User &user, std::unordered_map<std::string, std::string> &params, std::unordered_map<std::string, std::string> &headers, std::string &ip) {
	time_t cur_time = time(NULL);
	
	if(params["compact"] != "1") {
		return this->Error("Your client does not support compact announces");
	}
	
	long long left = strtolonglong(params["left"]);
	long long uploaded = std::max(0ll, strtolonglong(params["uploaded"]));
	long long downloaded = std::max(0ll, strtolonglong(params["downloaded"]));
	
	bool inserted = false; // If we insert the peer as opposed to update
	bool update_torrent = false; // Whether or not we should update the torrent in the DB
	bool expire_token = false; // Whether or not to expire a token after torrent completion
	
	std::unordered_map<std::string, std::string>::const_iterator peer_id_iterator = params.find("peer_id");
	if(peer_id_iterator == params.end()) {
		return this->Error("no peer id");
	}
	std::string peer_id = peer_id_iterator->second;
	peer_id = hex_decode(peer_id);
	
	if(whitelist_.size() > 0) {
		bool found = false; // Found client in whitelist?
		for(unsigned int i = 0; i < whitelist_.size(); i++) {
			if(peer_id.find(whitelist_[i]) == 0) {
				found = true;
				break;
			}
		}

		if(!found) {
			return this->Error("Your client is not on the whitelist");
		}
	}
	
	Peer *p;
	PeerList::iterator i;
	// Insert/find the peer in the torrent list
	if(left > 0 || params["event"] == "completed") {
		if(user.can_leech == false) {
			return this->Error("Access denied, leeching forbidden");
		}
		
		i = torrent.leechers.find(peer_id);
		if(i == torrent.leechers.end()) {
			Peer new_peer;
			std::pair<PeerList::iterator, bool> insert 
			= torrent.leechers.insert(std::pair<std::string, Peer>(peer_id, new_peer));
			
			p = &(insert.first->second);
			inserted = true;
		} else {
			p = &i->second;
		}
	} else {
		i = torrent.seeders.find(peer_id);
		if(i == torrent.seeders.end()) {
			Peer new_peer;
			std::pair<PeerList::iterator, bool> insert 
			= torrent.seeders.insert(std::pair<std::string, Peer>(peer_id, new_peer));
			
			p = &(insert.first->second);
			inserted = true;
		} else {
			p = &i->second;
		}
		
		torrent.last_seeded = cur_time;
	}
	
	// Update the peer
	p->left = left;
	long long upspeed = 0;
	long long downspeed = 0;
	long long real_uploaded_change = 0;
	long long real_downloaded_change = 0;
	
	if(inserted || params["event"] == "started" || uploaded < p->uploaded || downloaded < p->downloaded) {
		//New peer on this torrent
		update_torrent = true;
		p->userid = user.id;
		p->peer_id = peer_id;
		p->user_agent = headers["user-agent"];
		p->first_announced = cur_time;
		p->last_announced = 0;
		p->uploaded = uploaded;
		p->downloaded = downloaded;
		p->announces = 1;
	} else {
		long long uploaded_change = 0;
		long long downloaded_change = 0;
		p->announces++;
		
		if(uploaded != p->uploaded) {
			uploaded_change = uploaded - p->uploaded;
			real_uploaded_change = uploaded_change;
			p->uploaded = uploaded;
		}
		if(downloaded != p->downloaded) {
			downloaded_change = downloaded - p->downloaded;
			real_downloaded_change = downloaded_change;
			p->downloaded = downloaded;
		}
		if(uploaded_change || downloaded_change) {
			long corrupt = strtolong(params["corrupt"]);
			torrent.balance += uploaded_change;
			torrent.balance -= downloaded_change;
			torrent.balance -= corrupt;
			update_torrent = true;
			
			if(cur_time > p->last_announced) {
				upspeed = uploaded_change / (cur_time - p->last_announced);
				downspeed = downloaded_change / (cur_time - p->last_announced);
			}
			std::set<std::string>::iterator sit = torrent.tokened_users.find(user.id);
			if (torrent.free_torrent == NEUTRAL) {
				downloaded_change = 0;
				uploaded_change = 0;
			} else if(torrent.free_torrent == FREE || sit != torrent.tokened_users.end()) {
				if(sit != torrent.tokened_users.end()) {
					expire_token = true;
					std::stringstream record;
					record << '(' << user.id << ',' << torrent.id << ',' << downloaded_change << ')';
					std::string record_str = record.str();
					site_comm_->RecordToken(record_str);
				}
				downloaded_change = 0;
			}
			
			if(uploaded_change || downloaded_change) {
				
				std::stringstream record;
				record << '(' << user.id << ',' << uploaded_change << ',' << downloaded_change << ')';
				std::string record_str = record.str();
				site_comm_->RecordUser(record_str);
			}
		}
	}
	p->last_announced = cur_time;
	
	std::unordered_map<std::string, std::string>::const_iterator param_ip = params.find("ip");
	if(param_ip != params.end()) {
		ip = param_ip->second;
	} else {
		param_ip = params.find("ipv4");
		if(param_ip != params.end()) {
			ip = param_ip->second;
		}
	}
	
	unsigned int port = strtolong(params["port"]);
	// Generate compact ip/port string
	if(inserted || port != p->port || ip != p->ip) {
		p->port = port;
		p->ip = ip;
		p->ip_port = "";
		char x = 0;
		for(size_t pos = 0, end = ip.length(); pos < end; pos++) {
			if(ip[pos] == '.') {
				p->ip_port.push_back(x);
				x = 0;
				continue;
			} else if(!isdigit(ip[pos])) {
				return this->Error("Unexpected character in IP address. Only IPv4 is currently supported");
			}
			x = x * 10 + ip[pos] - '0';
		}
		p->ip_port.push_back(x);
		p->ip_port.push_back(port >> 8);
		p->ip_port.push_back(port & 0xFF);
		if(p->ip_port.length() != 6) {
			return this->Error("Specified IP address is of a bad length");
		}
	}
	
	// Select peers!
	unsigned int numwant;
	std::unordered_map<std::string, std::string>::const_iterator param_numwant = params.find("numwant");
	if(param_numwant == params.end()) {
		numwant = 50;
	} else {
		numwant = std::min(50l, strtolong(param_numwant->second));
	}

	int snatches = 0;
	int active = 1;
	if(params["event"] == "stopped") {
		update_torrent = true;
		active = 0;
		numwant = 0;

		if(left > 0) {
			if(torrent.leechers.erase(peer_id) == 0) {
				std::cout << "Tried and failed to remove seeder from torrent " << torrent.id << std::endl;
			}
		} else {
			if(torrent.seeders.erase(peer_id) == 0) {
				std::cout << "Tried and failed to remove leecher from torrent " << torrent.id << std::endl;
			}
		}
	} else if(params["event"] == "completed") {
		snatches = 1;
		update_torrent = true;
		torrent.completed++;
		
		std::stringstream record;
		record << '(' << user.id << ',' << torrent.id << ',' << cur_time << ", '" << ip << "')";
		std::string record_str = record.str();
		site_comm_->RecordSnatch(record_str);
		
		// User is a seeder now!
		torrent.seeders.insert(std::pair<std::string, Peer>(peer_id, *p));
		torrent.leechers.erase(peer_id);
		if(expire_token) {
			site_comm_->ExpireToken(torrent.id, user.id);
			torrent.tokened_users.erase(user.id);
		}
		// do cache expire
	}

	std::string peers;
	if(numwant > 0) {
		peers.reserve(300);
		unsigned int found_peers = 0;
		if(left > 0) { // Show seeders to leechers first
			if(torrent.seeders.size() > 0) {
				// We do this complicated stuff to cycle through the seeder list, so all seeders will get shown to leechers
				
				// Find out where to begin in the seeder list
				PeerList::const_iterator i;
				if(torrent.last_selected_seeder == "") {
					i = torrent.seeders.begin();
				} else {
					i = torrent.seeders.find(torrent.last_selected_seeder);
					i++;
					if(i == torrent.seeders.end()) {
						i = torrent.seeders.begin();
					}
				}
				
				// Find out where to end in the seeder list
				PeerList::const_iterator end;
				if(i == torrent.seeders.begin()) {
					end = torrent.seeders.end();
				} else {
					end = i;
					end--;
				}
				
				// Add seeders
				while(i != end && found_peers < numwant) {
					if(i == torrent.seeders.end()) {
						i = torrent.seeders.begin();
					}
					peers.append(i->second.ip_port);
					found_peers++;
					torrent.last_selected_seeder = i->second.peer_id;
					i++;
				}
			}

			if(found_peers < numwant && torrent.leechers.size() > 1) {
				for(PeerList::const_iterator i = torrent.leechers.begin(); i != torrent.leechers.end() && found_peers < numwant; i++) {
					if(i->second.ip_port == p->ip_port) { // Don't show leechers themselves
						continue; 
					}
					found_peers++;
					peers.append(i->second.ip_port);
				}
				
			}
		} else if(torrent.leechers.size() > 0) { // User is a seeder, and we have leechers!
			for(PeerList::const_iterator i = torrent.leechers.begin(); i != torrent.leechers.end() && found_peers < numwant; i++) {
				found_peers++;
				peers.append(i->second.ip_port);
			}
		}
	}
	
	if(update_torrent || torrent.last_flushed + 3600 < cur_time) {
		torrent.last_flushed = cur_time;
		
		std::stringstream record;
		record << '(' << torrent.id << ',' << torrent.seeders.size() << ',' << torrent.leechers.size() << ',' << snatches << ',' << torrent.balance << ')';
		std::string record_str = record.str();
		site_comm_->RecordTorrent(record_str);
	}
	
	std::stringstream record;
	record << '(' << user.id << ',' << torrent.id << ',' << active << ',' << uploaded << ',' << downloaded << ',' << upspeed << ',' << downspeed << ',' << left << ',' << (cur_time - p->first_announced) << ',' << p->announces << ',';
	std::string record_str = record.str();
	site_comm_->RecordPeer(record_str, ip, peer_id, headers["user-agent"]);

	if (real_uploaded_change > 0 || real_downloaded_change > 0) {
		record.str("");
		record << '(' << user.id << ',' << downloaded << ',' << left << ',' << uploaded << ',' << upspeed << ',' << downspeed << ',' << (cur_time - p->first_announced);
		record_str = record.str();
		site_comm_->RecordPeerHist(record_str, peer_id, torrent.id);
	}
	
	std::string response = "d8:intervali";
	response.reserve(350);
	response += inttostr(config_->kAnnounceInterval+std::min((size_t)600, torrent.seeders.size())); // ensure a more even distribution of announces/second
	response += "e12:min intervali";
	response += inttostr(config_->kAnnounceInterval);
	response += "e5:peers";
	if(peers.length() == 0) {
		response += "0:";
	} else {
		response += inttostr(peers.length());
		response += ":";
		response += peers;
	}
	response += "8:completei";
	response += inttostr(torrent.seeders.size());
	response += "e10:incompletei";
	response += inttostr(torrent.leechers.size());
	response += "e10:downloadedi";
	response += inttostr(torrent.completed);
	response += "ee";
	
	return response;
}

std::string Worker::Scrape(const std::list<std::string> &infohashes) {
	std::string output = "d5:filesd";
	for(std::list<std::string>::const_iterator i = infohashes.begin(); i != infohashes.end(); i++) {
		std::string infohash = *i;
		infohash = hex_decode(infohash);
		
		TorrentList::iterator tor = torrents_list_.find(infohash);
		if(tor == torrents_list_.end()) {
			continue;
		}
		Torrent *t = &(tor->second);
		
		output += inttostr(infohash.length());
		output += ':';
		output += infohash;
		output += "d8:completei";
		output += inttostr(t->seeders.size());
		output += "e10:incompletei";
		output += inttostr(t->leechers.size());
		output += "e10:downloadedi";
		output += inttostr(t->completed);
		output += "ee";
	}
	output+="ee";
	return output;
}

#ifdef ENABLE_UPDATE

//TODO: Restrict to local IPs
std::string Worker::Update(std::unordered_map<std::string, std::string> &params) {
	if(params["action"] == "change_passkey") {
		std::string oldpasskey = params["oldpasskey"];
		std::string newpasskey = params["newpasskey"];
		UserList::iterator i = users_list_.find(oldpasskey);
		if (i == users_list_.end()) {
			std::cout << "No user with passkey " << oldpasskey << " exists when attempting to change passkey to " << newpasskey << std::endl;
		} else {
			users_list_[newpasskey] = i->second;
			users_list_.erase(oldpasskey);
			std::cout << "changed passkey from " << oldpasskey << " to " << newpasskey << " for user " << i->second.id << std::endl;
		}
	} else if(params["action"] == "add_torrent") {
		Torrent t;
		t.id = strtolong(params["id"]);
		std::string info_hash = params["info_hash"];
		info_hash = hex_decode(info_hash);
		if(params["freetorrent"] == "0") {
			t.free_torrent = NORMAL;
		} else if(params["freetorrent"] == "1") {
			t.free_torrent = FREE;
		} else {
			t.free_torrent = NEUTRAL;
		}
		t.balance = 0;
		t.completed = 0;
		t.last_selected_seeder = "";
		torrents_list_[info_hash] = t;
		std::cout << "Added torrent " << t.id << ". FL: " << t.free_torrent << " " << params["freetorrent"] << std::endl;
	} else if(params["action"] == "update_torrent") {
		std::string info_hash = params["info_hash"];
		info_hash = hex_decode(info_hash);
		FreeType fl;
		if(params["freetorrent"] == "0") {
			fl = NORMAL;
		} else if(params["freetorrent"] == "1") {
			fl = FREE;
		} else {
			fl = NEUTRAL;
		}
		auto torrent_it = torrents_list_.find(info_hash);
		if (torrent_it != torrents_list_.end()) {
			torrent_it->second.free_torrent = fl;
			std::cout << "Updated torrent " << torrent_it->second.id << " to FL " << fl << std::endl;
		} else {
			std::cout << "Failed to find torrent " << info_hash << " to FL " << fl << std::endl;
		}
	} else if(params["action"] == "update_torrents") {
		// Each decoded infohash is exactly 20 characters long.
		std::string info_hashes = params["info_hashes"];
		info_hashes = hex_decode(info_hashes);
		FreeType fl;
		if(params["freetorrent"] == "0") {
			fl = NORMAL;
		} else if(params["freetorrent"] == "1") {
			fl = FREE;
		} else {
			fl = NEUTRAL;
		}
		for(unsigned int pos = 0; pos < info_hashes.length(); pos += 20) {
			std::string info_hash = info_hashes.substr(pos, 20);
			auto torrent_it = torrents_list_.find(info_hash);
			if (torrent_it != torrents_list_.end()) {
				torrent_it->second.free_torrent = fl;
				std::cout << "Updated torrent " << torrent_it->second.id << " to FL " << fl << std::endl;
			} else {
				std::cout << "Failed to find torrent " << info_hash << " to FL " << fl << std::endl;
			}
		}
	} else if(params["action"] == "add_token") {
		std::string info_hash = hex_decode(params["info_hash"]);
		std::string user_id = params["userid"];
		auto torrent_it = torrents_list_.find(info_hash);
		if (torrent_it != torrents_list_.end()) {
			torrent_it->second.tokened_users.insert(user_id);
		} else {
			std::cout << "Failed to find torrent to add a token for user " << user_id << std::endl;
		}
	} else if(params["action"] == "remove_token") {
		std::string info_hash = hex_decode(params["info_hash"]);
		std::string user_id = params["userid"];
		auto torrent_it = torrents_list_.find(info_hash);
		if (torrent_it != torrents_list_.end()) {
			torrent_it->second.tokened_users.erase(user_id);
		} else {
			std::cout << "Failed to find torrent " << info_hash << " to remove token for user " << user_id << std::endl;
		}
	} else if(params["action"] == "delete_torrent") {
		std::string info_hash = params["info_hash"];
		info_hash = hex_decode(info_hash);
		auto torrent_it = torrents_list_.find(info_hash);
		if (torrent_it != torrents_list_.end()) {
			std::cout << "Deleting torrent " << torrent_it->second.id << std::endl;
			torrents_list_.erase(torrent_it);
		} else {
			std::cout << "Failed to find torrent " << info_hash << " to delete " << std::endl;
		}
	} else if(params["action"] == "add_user") {
		std::string passkey = params["passkey"];
		User u;
		u.id = params["id"];
		u.can_leech = 1;
		users_list_[passkey] = u;
		std::cout << "Added user " << params["id"] << std::endl;
	} else if(params["action"] == "remove_user") {
		std::string passkey = params["passkey"];
		users_list_.erase(passkey);
		std::cout << "Removed user " << passkey << std::endl;
	} else if(params["action"] == "remove_users") {
		// Each passkey is exactly 32 characters long.
		std::string passkeys = params["passkeys"];
		for(unsigned int pos = 0; pos < passkeys.length(); pos += 32){
			std::string passkey = passkeys.substr(pos, 32);
			users_list_.erase(passkey);
			std::cout << "Removed user " << passkey << std::endl;
		}
	} else if(params["action"] == "update_user") {
		std::string passkey = params["passkey"];
		bool can_leech = true;
		if(params["can_leech"] == "0") {
			can_leech = false;
		}
		UserList::iterator i = users_list_.find(passkey);
		if (i == users_list_.end()) {
			std::cout << "No user with passkey " << passkey << " found when attempting to change leeching status!" << std::endl;
		} else {
			users_list_[passkey].can_leech = can_leech;
			std::cout << "Updated user " << passkey << std::endl;
		}
	} else if(params["action"] == "add_whitelist") {
		std::string peer_id = params["peer_id"];
		whitelist_.push_back(peer_id);
		std::cout << "Whitelisted " << peer_id << std::endl;
	} else if(params["action"] == "remove_whitelist") {
		std::string peer_id = params["peer_id"];
		for(unsigned int i = 0; i < whitelist_.size(); i++) {
			if(whitelist_[i].compare(peer_id) == 0) {
				whitelist_.erase(whitelist_.begin() + i);
				break;
			}
		}
		std::cout << "De-whitelisted " << peer_id << std::endl;
	} else if(params["action"] == "edit_whitelist") {
		std::string new_peer_id = params["new_peer_id"];
		std::string old_peer_id = params["old_peer_id"];
		for(unsigned int i = 0; i < whitelist_.size(); i++) {
			if(whitelist_[i].compare(old_peer_id) == 0) {
				whitelist_.erase(whitelist_.begin() + i);
				break;
			}
		}
		whitelist_.push_back(new_peer_id);
		std::cout << "Edited whitelist item from " << old_peer_id << " to " << new_peer_id << std::endl;
	} /*else if(params["action"] == "update_announce_interval") {
		unsigned int interval = strtolong(params["new_announce_interval"]);
		config_->kAnnounceInterval = interval;
		std::cout << "Edited announce interval to " << interval << std::endl;
	}*/ else if(params["action"] == "info_torrent") {
		std::string info_hash_hex = params["info_hash"];
		std::string info_hash = hex_decode(info_hash_hex);
		std::cout << "Info for torrent '" << info_hash_hex << "'" << std::endl;
		auto torrent_it = torrents_list_.find(info_hash);
		if (torrent_it != torrents_list_.end()) {
			std::cout << "Torrent " << torrent_it->second.id
				<< ", freetorrent = " << torrent_it->second.free_torrent << std::endl;
		} else {
			std::cout << "Failed to find torrent " << info_hash_hex << std::endl;
		}
	}
	return "success";
}

#endif // ENABLE_UPDATE

void Worker::ReapPeers() {
	std::cout << "started reaper" << std::endl;
	boost::thread thread(&Worker::DoReapPeers, this);
}

void Worker::DoReapPeers() {
	Logger::instance()->Log("Began Worker::DoReapPeers()");
	time_t cur_time = time(NULL);
	unsigned int reaped = 0;
	std::unordered_map<std::string, Torrent>::iterator i = torrents_list_.begin();
	for(; i != torrents_list_.end(); i++) {
		std::map<std::string, Peer>::iterator p = i->second.leechers.begin();
		std::map<std::string, Peer>::iterator del_p;
		while(p != i->second.leechers.end()) {
			if(p->second.last_announced + config_->kPeersTimeout < cur_time) {
				del_p = p;
				p++;
				boost::mutex::scoped_lock lock(site_comm_->torrent_list_mutex);
				i->second.leechers.erase(del_p);
				reaped++;
			} else {
				p++;
			}
		}
		p = i->second.seeders.begin();
		while(p != i->second.seeders.end()) {
			if(p->second.last_announced + config_->kPeersTimeout < cur_time) {
				del_p = p;
				p++;
				boost::mutex::scoped_lock lock(site_comm_->torrent_list_mutex);
				i->second.seeders.erase(del_p);
				reaped++;
			} else {
				p++;
			}
		}
	}
	std::cout << "Reaped " << reaped << " peers" << std::endl;
	Logger::instance()->Log("Completed worker::do_reap_peers()");
}
