#ifndef OCELOT_SRC_LOGGER_H
#define OCELOT_SRC_LOGGER_H

#include <string>
#include <iostream>
#include <fstream>

#include <boost/thread/mutex.hpp>

// Logs to a given file
//   Logger *log = Logger.get_instance();
//   log->Log("Hello World!");
class Logger {
public:
  Logger(std::string filename);
  virtual ~Logger();
  bool Log(std::string msg);
  static Logger* instance();
  
private:
  static Logger* instance_;
  boost::mutex log_lock_;
  std::ofstream log_file_;
};

#endif // OCELOT_SRC_LOGGER_H
