#include "hormons_reader.h"
#include <fstream>
#include <nlohmann/json.hpp>

bool read_latest_hormones(const std::string& spikes_path, Hormones& H) {
    std::ifstream f(spikes_path);
    if (!f.is_open()) return false;

    std::string line, last;
    while (std::getline(f, line)) if (!line.empty()) last = std::move(line);
    if (last.empty()) return false;

    auto j = nlohmann::json::parse(last, nullptr, false);
    if (j.is_discarded() || j.value("type","") != "spike" || !j.contains("hormones")) return false;

    const auto& h = j["hormones"];
    auto to_f = [&](const char* k){ return h.contains(k) ? std::stof(h[k].get<std::string>()) : 0.0f; };

    H.dopamine      = to_f("dopamine");
    H.serotonin     = to_f("serotonin");
    H.cortisol      = to_f("cortisol");
    H.adrenaline    = to_f("adrenaline");
    H.oxytocin      = to_f("oxytocin");
    H.melatonin     = to_f("melatonin");
    H.noradrenaline = to_f("noradrenaline");
    H.endorphin     = to_f("endorphin");
    H.acetylcholine = to_f("acetylcholine");
    H.testosterone  = to_f("testosterone");
    return true;
}
