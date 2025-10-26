
#pragma once
#include <string>
#include "hormons_reader.h"

struct Decision {
    std::string reply;
    std::string feedback; // reward|punish|none
    float intensity = 0.0f;
};

std::string build_prompt();
bool parse_decision(const std::string& raw, Decision& d);
