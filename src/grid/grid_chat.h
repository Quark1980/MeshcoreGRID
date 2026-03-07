#ifndef GRID_CHAT_H
#define GRID_CHAT_H

#include "grid_launcher.h"

#include <string>
#include <vector>


extern GridApp chat_app;
extern std::vector<std::string> hashtags;
extern std::vector<std::string> contacts;

void switch_to_channel(const char *name);

#endif // GRID_CHAT_H
