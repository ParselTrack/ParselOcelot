#ifndef OCELOT_SRC_MISC_FUNCTIONS_H
#define OCELOT_SRC_MISC_FUNCTIONS_H

#include <string>
#include <cstdlib>

long strtolong(const std::string& str);
long long strtolonglong(const std::string& str);
std::string inttostr(int i);
std::string hex_decode(const std::string &in);
int timeval_subtract (timeval* result, timeval* x, timeval* y);

#endif // OCELOT_SRC_MISC_FUNCTIONS_H
