#pragma once
#include <algorithm>

struct HormoneSet {
    float dopamine = 0.0f;
    float serotonin = 0.0f;
    float cortisol = 0.0f;
    float adrenaline = 0.0f;
    float oxytocin = 0.0f;
    float melatonin = 0.0f;
    float noradrenaline = 0.0f;
    float endorphin = 0.0f;
    float acetylcholine = 0.0f;
    float testosterone = 0.0f;
};

class HormoneSystem {
public:
    // Die aktuellen Werte (Das ist, was der Prompt sieht)
    HormoneSet current;

    // Die Konfiguration (Ricks Persönlichkeit)
    HormoneSet base_config;

    // Das aktuelle temporäre Ziel (wohin wir gerade driften)
    HormoneSet target;

    // Timer für den nächsten Stimmungsschwank
    float event_timer = 0.0f;

    // Konstruktor: Setzt Rick als Standard
    HormoneSystem();

    // Update Loop
    void update(float dt);

    // Inputs vom SNN (Drives)
    float drive_dopamine   = 0.0f;
    float drive_cortisol   = 0.0f;
    float drive_adrenaline = 0.0f;

    void set_dopamine_drive(float v)   { drive_dopamine   = std::clamp(v, 0.0f, 2.0f); } // Nur positiv addieren
    void set_cortisol_drive(float v)   { drive_cortisol   = std::clamp(v, 0.0f, 2.0f); }
    void set_adrenaline_drive(float v) { drive_adrenaline = std::clamp(v, 0.0f, 2.0f); }
};
