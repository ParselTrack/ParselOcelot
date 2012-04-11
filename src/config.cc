#include "config.h"
#include <iostream>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/config.hpp>
#include <boost/program_options/environment_iterator.hpp>
#include <boost/program_options/eof_iterator.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/version.hpp> 

namespace po = boost::program_options;

Config::Config(int argc, char **argv)
{
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h",                                                              "produce help message")
    ("host,n",                  po::value<std::string>(),                        "set listen host")
    ("port,p",                  po::value<unsigned int>(),                       "set listen port")
    ("max-connections,c",       po::value<unsigned int>(),               "set maximum connections")
    ("max-read-buffer,b",       po::value<unsigned int>(),               "set maximum read buffer")
    ("timeout-interval,t",      po::value<unsigned int>(),                  "set timeout interval")
    ("schedule-interval,s",     po::value<unsigned int>(),                 "set schedule interval")
    ("max-middlemen,m",         po::value<unsigned int>(),                 "set maxmium middlemen")
    ("announce-interval,a",     po::value<unsigned int>(),                 "set announce interval")
    ("peers-timeout,e",         po::value<int>(),                              "set peers timeout")
    ("reap-peers-interval,r",   po::value<unsigned int>(),               "set reap peers interval")
    ("site-host,s",             po::value<std::string>(),                          "set site host")
    ("site-port,d",             po::value<int>(),                                  "set site port")
    #if ENABLE_UPDATE
    ("site-password,w",         po::value<std::string>(),                      "set site password")
    #endif // ENABLE_UPDATE
    ("log-file,l",              po::value<std::string>(),                           "set log file")
    ("allow-reverse-proxies,x",                                        "set allow reverse proxies")
    ("trust-proxy-ip,z",        po::value<std::vector<std::string> >(), "trust IP's proxy headers")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << std::endl;
    exit(1);
  }

  (vm.count("host"))?
    kHost = vm["host"].as<std::string>() :
    kHost = "127.0.0.1";

  (vm.count("port"))?
    kPort = vm["port"].as<unsigned int>() :
    kPort = 34000;

  (vm.count("max-connections"))?
    kMaxConnections = vm["max-connections"].as<unsigned int>() :
    kMaxConnections = 512;

  (vm.count("max-read-buffer"))?
    kMaxReadBuffer = vm["max-read-buffer"].as<unsigned int>() :
    kMaxReadBuffer = 4096;

  (vm.count("timeout-interval"))?
    kTimeoutInterval = vm["timeout-interval"].as<unsigned int>() :
    kTimeoutInterval = 20;

  (vm.count("schedule-interval"))?
    kScheduleInterval = vm["schedule-interval"].as<unsigned int>() :
    kScheduleInterval = 3;

  (vm.count("max-middlemen"))?
    kMaxMiddlemen = vm["max-middlemen"].as<unsigned int>() :
    kMaxMiddlemen = 5000;

  (vm.count("announce-interval"))?
    kAnnounceInterval = vm["announce-interval"].as<unsigned int>() :
    kAnnounceInterval = 1800;

  (vm.count("peers-timeout"))?
    kPeersTimeout = vm["peers-timeout"].as<int>() :
    kPeersTimeout = kAnnounceInterval * 1.5;

  (vm.count("reap-peers-interval"))?
    kReapPeersInterval = vm["reap-peers-interval"].as<int>() :
    kReapPeersInterval = 1800;

  (vm.count("site-host"))?
    kSiteHost = vm["site-host"].as<std::string>() :
    kSiteHost = "127.0.0.1";

  (vm.count("site-port"))?
    kSitePort = vm["site-port"].as<int>() :
    kSitePort = 80;

  #ifdef ENABLE_UPDATE
  (vm.count("site-password"))?
    kSitePassword = vm["site-password"].as<std::string>() :
    kSitePassword = "********************************";
  #endif

  (vm.count("log-file"))?
    kLogFile = vm["log-file"].as<std::string>() :
    kLogFile = "debug.log";

  (vm.count("allow-reverse-proxies"))?
    kAllowReverseProxies = true :
    kAllowReverseProxies = false;

  (vm.count("trust-proxy-ip"))?
    kTrustedProxyIps = vm["trust-proxy-ip"].as<std::vector<std::string> >() :
    kTrustedProxyIps = std::vector<std::string>();
}