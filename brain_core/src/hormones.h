#pragma once
#include <algorithm>

class HormoneSystem {
public:
    // --- Hormonwerte ---
    float dopamine      = 0.15f;
    float serotonin     = 0.35f;
    float cortisol      = 0.08f;
    float adrenaline    = 0.12f;
    float oxytocin      = 0.25f;
    float melatonin     = 0.05f;
    float noradrenaline = 0.20f;
    float endorphin     = 0.10f;
    float acetylcholine = 0.20f;
    float testosterone  = 0.25f;

    // --- Update (automatische Anpassung) ---
    void update(float dt);

    // --- Steuerbare Drives (Eingänge) ---
    float drive_dopamine   = 0.0f;
    float drive_cortisol   = 0.0f;
    float drive_adrenaline = 0.0f;

    void set_dopamine_drive(float v)   { drive_dopamine   = std::clamp(v, -1.0f, 1.0f); }
    void set_cortisol_drive(float v)   { drive_cortisol   = std::clamp(v, -1.0f, 1.0f); }
    void set_adrenaline_drive(float v) { drive_adrenaline = std::clamp(v, -1.0f, 1.0f); }
};
