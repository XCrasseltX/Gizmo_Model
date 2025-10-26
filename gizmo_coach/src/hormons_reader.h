#pragma once
#include <string>

struct Hormones {
    float dopamine=0, serotonin=0, cortisol=0, adrenaline=0, oxytocin=0,
          melatonin=0, noradrenaline=0, endorphin=0, acetylcholine=0, testosterone=0;
};

bool read_latest_hormones(const std::string& spikes_path, Hormones& H);
