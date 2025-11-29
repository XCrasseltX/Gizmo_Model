#include "io_logger.h"
#include "hormones.h"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>


using json = nlohmann::json;
namespace fs = std::filesystem;

void IoLogger::clear_all_io_files() {
    std::vector<std::string> files = {
        "./../../io/out/spikes.jsonl",
        "./../../io/out/log.jsonl",
        "./../../io/out/stats.jsonl"
    };

    for (const auto& f : files) {
        std::ofstream ofs(f, std::ios::trunc);
    }
}
void IoLogger::set_layer_info(int n_inputs, int n_outputs)
{
    n_inputs_  = n_inputs;
    n_outputs_ = n_outputs;
}
void IoLogger::clear_log_file(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::ofstream clear_file(path, std::ios::trunc);
    clear_file.close();
}

static void trim_file_to_last_lines(const std::string& path, size_t max_lines = 100) {
    std::ifstream fin(path, std::ios::in | std::ios::binary);
    if (!fin.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line)) {
        lines.push_back(std::move(line));
        if (lines.size() > max_lines) {
            lines.erase(lines.begin()); // älteste entfernen
        }
    }
    fin.close();

    std::ofstream fout(path, std::ios::out | std::ios::trunc | std::ios::binary);
    for (auto& l : lines) fout << l << "\n";
}

static std::string now_iso_utc() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

IoLogger& IoLogger::instance() {
    static IoLogger inst;
    return inst;
}

void IoLogger::open(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mtx_);
    fs::create_directories(dir);

    clear_all_io_files(); // leert spikes/log/stats im selben Ordner

    spikes_path_ = (fs::path(dir) / "spikes.jsonl").string();
    log_path_    = (fs::path(dir) / "log.jsonl").string();
    stats_path_  = (fs::path(dir) / "stats.jsonl").string();

    spikes_.open(spikes_path_, std::ios::out | std::ios::app);
    log_.open(log_path_,       std::ios::out | std::ios::app);
    stats_.open(stats_path_,   std::ios::out | std::ios::app);

    fd_spikes_ = ::open(spikes_path_.c_str(), O_WRONLY | O_APPEND);
    fd_log_    = ::open(log_path_.c_str(),    O_WRONLY | O_APPEND);
    fd_stats_  = ::open(stats_path_.c_str(),  O_WRONLY | O_APPEND);
}

void IoLogger::log_spike_matrix(const std::vector<uint8_t>& spikes, int timestep)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (spikes.empty()) return;

    stats_.close();
    stats_.open(stats_path_, std::ios::out | std::ios::trunc);
    if (!stats_.is_open()) return;

    const int total = spikes.size();
    const int side  = static_cast<int>(std::ceil(std::sqrt(total)));

    // Falls dein Netz die Layer-Anteile kennt:
    const int n_inputs  = total / 5;  // 20% Input
    const int n_outputs = total / 5;  // 20% Output
    const int input_end = n_inputs;
    const int output_start = total - n_outputs;

    std::ostringstream visual;
    visual << "Timestep " << timestep << "  (N=" << total << ", Grid=" << side << "×" << side << ")\n";

    for (int r = 0; r < side; ++r) {
        std::string line;
        for (int c = 0; c < side; ++c) {
            int i = r * side + c;
            if (i >= total) break;

            // kleine Layer-Grenze optisch markieren
            if (i == input_end || i == output_start)
                line += " | ";

            if (i < input_end)
                line += (spikes[i] ? "▲" : "·"); // Input
            else if (i >= output_start)
                line += (spikes[i] ? "■" : "·"); // Output
            else
                line += (spikes[i] ? "×" : "·"); // Hidden

            if (c < side - 1)
                line += "  ";
        }
        visual << line << "\n";
    }

    stats_ << visual.str();
    stats_.flush();
    if (fd_stats_ >= 0)
        fsync(fd_stats_);

    stats_.close();
    stats_.open(stats_path_, std::ios::out | std::ios::app);
}

void IoLogger::log_spike(const HormoneSystem* H, int timestep, int spike_count) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!spikes_.is_open()) return;

    nlohmann::json j;
    j["ts"] = now_iso_utc();
    j["type"] = "spike";
    j["timestep"] = timestep;
    j["spikes"] = spike_count;

    if (H) {
        // Lokale Hilfsfunktion zum Formatieren auf 2 Nachkommastellen
        auto fmt2 = [](float v) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << v;
            return ss.str();  // als String mit genau zwei Nachkommastellen
        };

        // Nur die Hormone werden formatiert als Strings
        j["hormones"] = {
            {"dopamine",      fmt2(H->current.dopamine)},
            {"serotonin",     fmt2(H->current.serotonin)},
            {"cortisol",      fmt2(H->current.cortisol)},
            {"adrenaline",    fmt2(H->current.adrenaline)},
            {"oxytocin",      fmt2(H->current.oxytocin)},
            {"melatonin",     fmt2(H->current.melatonin)},
            {"noradrenaline", fmt2(H->current.noradrenaline)},
            {"endorphin",     fmt2(H->current.endorphin)},
            {"acetylcholine", fmt2(H->current.acetylcholine)},
            {"testosterone",  fmt2(H->current.testosterone)}
        };
    }

    // Die restlichen Metadaten bleiben als „echte“ JSON-Werte
    j["ts"] = now_iso_utc();
    j["type"] = "spike";
    j["timestep"] = timestep;
    j["spikes"] = spike_count;

    // Schreiben ins File (normal, kein Pretty-Print)
    spikes_ << j.dump() << "\n";
    spikes_.flush();
    if (fd_spikes_ >= 0) fsync(fd_spikes_);
    trim_file_to_last_lines(spikes_path_, 100);
}

void IoLogger::log_status(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!log_.is_open()) return;
    json j = { {"ts", now_iso_utc()}, {"type","status"}, {"message", msg} };
    log_ << j.dump() << "\n";
    log_.flush();
    if (fd_log_ >= 0) fsync(fd_log_);
    trim_file_to_last_lines(log_path_, 100);     // trim log.jsonl
}


void IoLogger::log_error(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!log_.is_open()) return;
    json j = { {"ts", now_iso_utc()}, {"type","error"}, {"message", msg} };
    log_ << j.dump() << "\n";
    log_.flush();
    if (fd_log_ >= 0) fsync(fd_log_);
    trim_file_to_last_lines(log_path_, 100);
}

void IoLogger::log_hormone(const std::string& name, float level) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!log_.is_open()) return;                 // vorerst in log.jsonl
    json j = { {"ts", now_iso_utc()}, {"type","hormone"}, {"name", name}, {"level", level} };
    log_ << j.dump() << "\n";
    log_.flush();
    if (fd_log_ >= 0) fsync(fd_log_);
    trim_file_to_last_lines(log_path_, 100);
}

