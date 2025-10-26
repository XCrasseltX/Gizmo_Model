#include "brain_io.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <algorithm>

// -------------------------------------------------------------
// Hilfsfunktion: schreibt einheitliches JSON mit Metadaten
// -------------------------------------------------------------
void write_command(const std::string& path, const CommandMeta& meta, const nlohmann::json& data)
{
    nlohmann::json j = {
        {"ts", meta.ts},
        {"seq", meta.seq},
        {"source", meta.source},
        {"cmd", meta.cmd},
        {"data", data}
    };

    std::ofstream out(path, std::ios::app);
    if (out.is_open())
        out << j.dump() << "\n";
}

// -------------------------------------------------------------
// Zeitstempel-Helfer
// -------------------------------------------------------------
static double now_seconds()
{
    using namespace std::chrono;
    return duration_cast<duration<double>>(system_clock::now().time_since_epoch()).count();
}

// -------------------------------------------------------------
// Set-Hormones-Befehl
// -------------------------------------------------------------
void send_set_hormones(const std::string& path, float dopa, float cort, float adre, int seq)
{
    nlohmann::json data = {
        {"dopamine",   std::clamp(dopa, -1.0f, 1.0f)},
        {"cortisol",   std::clamp(cort, -1.0f, 1.0f)},
        {"adrenaline", std::clamp(adre, -1.0f, 1.0f)}
    };

    CommandMeta meta{
        now_seconds(),
        seq,
        "coach",
        "set_hormones"
    };

    write_command(path, meta, data);
}

// -------------------------------------------------------------
// Feedback -> ruft send_set_hormones() auf mit Mapping
// -------------------------------------------------------------
void apply_feedback(const std::string& path, const Decision& d, int seq)
{
    float I = std::clamp(d.intensity, 0.0f, 1.0f);

    if (d.feedback == "reward")
        send_set_hormones(path, +0.3f + 0.7f * I, -0.1f * I, 0.1f * I, seq);
    else if (d.feedback == "punish")
        send_set_hormones(path, -0.2f * I, +0.4f + 0.6f * I, 0.05f * I, seq);
    else
        send_set_hormones(path, 0.0f, 0.0f, 0.05f * I, seq);
}

// -------------------------------------------------------------
// Input-Pattern
// -------------------------------------------------------------
void send_input_pattern(const std::string& path, const std::vector<int>& pat, int seq)
{
    nlohmann::json data = { {"pattern", pat} };

    CommandMeta meta{
        now_seconds(),
        seq,
        "coach",
        "input_pattern"
    };

    write_command(path, meta, data);
}
