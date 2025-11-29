#include "hormones.h"
#include <cmath>
#include <cstdlib> // für rand()

// Zufallszahl zwischen min und max
static float random_range(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

// Lineare Interpolation (Sanftes Gleiten)
static float lerp(float current, float target, float speed, float dt) {
    return current + (target - current) * speed * dt;
}

HormoneSystem::HormoneSystem() {
    // --- RICK SANCHEZ BASELINE ---
    // Intelligent, Zynisch, Stressresistent, Wenig Bindung
    base_config.dopamine      = 0.30f; // Eher gelangweilt
    base_config.serotonin     = 0.70f; // Hohes Selbstbewusstsein (Arroganz)
    base_config.cortisol      = 0.10f; // Cool unter Druck
    base_config.adrenaline    = 0.20f; // Grundwachheit
    base_config.oxytocin      = 0.05f; // Soziale Distanz
    base_config.melatonin     = 0.05f; // Kaum müde
    base_config.noradrenaline = 0.40f; // Fokus
    base_config.endorphin     = 0.10f; 
    base_config.acetylcholine = 0.85f; // Maximale kognitive Leistung
    base_config.testosterone  = 0.60f; // Dominant

    // Startwerte setzen
    current = base_config;
    target  = base_config;
    event_timer = 2.0f; // Erster Event in 2 Sekunden
}

void HormoneSystem::update(float dt) {
    
    // 1. TIMING: Wann kommt der nächste "Gedanke" / Stimmungsschwank?
    event_timer -= dt;

    if (event_timer <= 0.0f) {
        // Neuen Timer setzen (Random 2 bis 7 Sekunden)
        event_timer = random_range(2.0f, 7.0f);

        // ENTSCHEIDUNG: Zurück zur Basis oder Chaos?
        float dice = random_range(0.0f, 1.0f);

        if (dice < 0.4f) {
            // 40% Chance: "Reset to Base" (Rick fängt sich wieder)
            target = base_config; 
        } 
        else if (dice < 0.7f) {
            // 30% Chance: Leichte Variation (Tagesform)
            target.dopamine      = base_config.dopamine      + random_range(-0.1f, 0.2f);
            target.serotonin     = base_config.serotonin     + random_range(-0.1f, 0.1f);
            target.adrenaline    = base_config.adrenaline    + random_range(-0.05f, 0.2f);
            target.acetylcholine = base_config.acetylcholine + random_range(-0.1f, 0.1f);
            // Rest bleibt grob gleich
        } 
        else {
            // 30% Chance: Starker "Micro-Mood" (Zufälliger Impuls)
            // Wir würfeln EINEN starken emotionalen Zustand
            int mood = rand() % 4;
            switch(mood) {
                case 0: // "Eureka!" (Idee)
                    target.dopamine = 0.9f; target.acetylcholine = 0.95f; target.adrenaline = 0.5f;
                    break;
                case 1: // "Genervt" (Cortisol Spike)
                    target.cortisol = 0.6f; target.serotonin = 0.2f; target.dopamine = 0.1f;
                    break;
                case 2: // "Manisch" (Action)
                    target.adrenaline = 0.8f; target.testosterone = 0.8f; target.noradrenaline = 0.7f;
                    break;
                case 3: // "Absturz" (Müde/Bored)
                    target.dopamine = 0.05f; target.melatonin = 0.4f; target.acetylcholine = 0.3f;
                    break;
            }
        }
    }

    // 2. INPUT VOM SNN (Die Spikes)
    // Die Spikes überschreiben das Ziel temporär. 
    // Wenn Spikes kommen, MÜSSEN die Werte hoch, egal was der Timer sagt.
    
    // Wir addieren die Drives auf das aktuelle Ziel drauf
    HormoneSet effective_target = target;
    
    if (drive_dopamine > 0.01f)   effective_target.dopamine   += drive_dopamine;
    if (drive_adrenaline > 0.01f) effective_target.adrenaline += drive_adrenaline;
    if (drive_cortisol > 0.01f)   effective_target.cortisol   += drive_cortisol;

    // Wenn Stress (Cortisol) hoch ist, sinkt Serotonin automatisch (Gegenspieler Logik simple)
    if (effective_target.cortisol > 0.5f) effective_target.serotonin *= 0.5f;


    // 3. BEWEGUNG (Lerp)
    // Wir bewegen uns vom 'current' zum 'effective_target'.
    // speed bestimmt, wie träge das System ist.
    float speed = 0.05f; // Ziemlich zügig reagieren

    current.dopamine      = lerp(current.dopamine,      effective_target.dopamine,      speed, dt);
    current.serotonin     = lerp(current.serotonin,     effective_target.serotonin,     speed, dt);
    current.cortisol      = lerp(current.cortisol,      effective_target.cortisol,      speed, dt);
    current.adrenaline    = lerp(current.adrenaline,    effective_target.adrenaline,    speed, dt);
    current.oxytocin      = lerp(current.oxytocin,      effective_target.oxytocin,      speed, dt);
    current.melatonin     = lerp(current.melatonin,     effective_target.melatonin,     speed, dt);
    current.noradrenaline = lerp(current.noradrenaline, effective_target.noradrenaline, speed, dt);
    current.endorphin     = lerp(current.endorphin,     effective_target.endorphin,     speed, dt);
    current.acetylcholine = lerp(current.acetylcholine, effective_target.acetylcholine, speed, dt);
    current.testosterone  = lerp(current.testosterone,  effective_target.testosterone,  speed, dt);

    // 4. CLIPPING (Sicherheit)
    // Werte zwischen 0.01 und 0.99 halten
    auto clip = [](float &x) { x = std::max(0.01f, std::min(0.99f, x)); };
    
    clip(current.dopamine);
    clip(current.serotonin);
    clip(current.cortisol);
    clip(current.adrenaline);
    clip(current.oxytocin);
    clip(current.melatonin);
    clip(current.noradrenaline);
    clip(current.endorphin);
    clip(current.acetylcholine);
    clip(current.testosterone);
}
