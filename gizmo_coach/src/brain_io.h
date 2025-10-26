#pragma once
#include <string>
#include <vector>
#include "coach_logic.h"
#include <nlohmann/json.hpp>

struct CommandMeta {
    double ts;
    int seq;
    std::string source;
    std::string cmd;
};

void write_command(const std::string& path, const CommandMeta& meta, const nlohmann::json& data);

void send_set_hormones(const std::string& path, float dopa, float cort, float adre, int seq = 0);
void apply_feedback(const std::string& path, const Decision& d, int seq = 0);
void send_input_pattern(const std::string& path, const std::vector<int>& pat, int seq = 0);