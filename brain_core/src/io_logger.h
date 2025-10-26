#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include "hormones.h"

// Sehr einfache Thread-sichere JSONL-Logger-Klasse
class IoLogger {
public:
    static IoLogger& instance();
    void open(const std::string& dir = "./../../io/out/");
    void clear_log_file(const std::string& path);
    void log_spike_matrix(const std::vector<uint8_t>& spikes, int timestep);
    void log_spike(const HormoneSystem* H, int timestep, int spike_count);
    void log_status(const std::string& msg);
    void log_error(const std::string& msg);
    void log_hormone(const std::string& name, float level);
    void clear_all_io_files();
    void set_layer_info(int n_inputs, int n_outputs);

private:
    std::ofstream spikes_, log_, stats_;
    int fd_spikes_ = -1, fd_log_ = -1, fd_stats_ = -1;
    std::mutex mtx_;
    std::string spikes_path_, log_path_, stats_path_;
    int n_inputs_  = 0;
    int n_outputs_ = 0;
};
