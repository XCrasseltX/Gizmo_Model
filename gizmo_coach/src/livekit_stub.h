#pragma once
#include <string>

void send_to_livekit(const std::string& reply);

void handle_from_livekit(const std::string& json_in);