#include "logger.h"

Logger* Logger::instance_ = NULL;

Logger::Logger(std::string filename)
{
  Logger::log_file_.open(filename.c_str(), std::ios::out);
  if(Logger::log_file_.is_open())
  {
    instance_ = this;
  }
}

Logger::~Logger()
{
  if(log_file_.is_open())
  {
    log_file_.close();
  }
  instance_ = NULL;
}

Logger *Logger::instance() {
  if(instance_ != NULL)
  {
    return instance_;
  }
  return NULL;
}

bool Logger::Log(std::string msg) {
  boost::mutex::scoped_lock lock(log_lock_);
  if(log_file_.is_open())
  {
    log_file_ << msg << std::endl;
    log_file_.flush();
    return true;
  }
  return false;
}
