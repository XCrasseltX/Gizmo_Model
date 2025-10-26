#include "hormones.h"
#include <cmath>

static inline float sclip(float x, float lo, float hi) {
    return std::max(lo, std::min(hi, x));
}

void HormoneSystem::update(float dt) {
    const float tau = 10.0f;         // Trägheit der Hormone
    const float damping = 0.1f;      // Dämpfung, verhindert Explosion
    const float noise_amp = 0.002f;  // leichtes Rauschen
    const float homeo = 0.2f;       // zieht zum Basiswert zurück

    const float base_dopa = 0.40f, base_ser = 0.25f, base_cor = 0.12f, base_adr = 0.25f;
    const float base_oxy  = 0.10f, base_mel = 0.02f, base_nor = 0.30f, base_end = 0.20f;
    const float base_ach  = 0.35f, base_tes = 0.30f;

    auto noise = [&]() { return noise_amp * ((float)rand() / RAND_MAX - 0.5f); };
    auto clip = [](float x){ return std::max(0.0f, std::min(1.0f, x)); };

    // Lokale "Geschwindigkeiten" für jedes Hormon (Persistent in der Klasse anlegen!)
    static float v_dopa=0,v_ser=0,v_cor=0,v_adr=0,v_oxy=0,v_mel=0,v_nor=0,v_end=0,v_ach=0,v_tes=0;

    // Homeostase + Rückkopplung
    auto dyn = [&](float &x, float &v, float base, float drive, float excite, float inhibit) {
        // Nichtlineare Rückstellkraft: stärker bei großen Abweichungen
        float dx = x - base;
        float F = -homeo * dx * (1.0f + 2.0f * std::abs(dx))   // nonlinear restoring
                + 0.3f * drive
                + 0.2f * (excite - inhibit)
                + noise();

        // Dämpfung leicht erhöht
        v += dt * (F - damping * v);
        x += dt / tau * v;

        // Reflexion + Reibung
        if (x > 1.0f) {
            x = 1.0f;
            v *= -0.3f;
        } else if (x < 0.0f) {
            x = 0.0f;
            v *= -0.3f;
        }
    };

    // gegenseitige Wechselwirkungen
    dyn(dopamine,     v_dopa, base_dopa, drive_dopamine,   endorphin, cortisol);
    dyn(serotonin,    v_ser,  base_ser,  0.0f,             oxytocin,  adrenaline);
    dyn(cortisol,     v_cor,  base_cor,  drive_cortisol,   adrenaline, dopamine);
    dyn(adrenaline,   v_adr,  base_adr,  drive_adrenaline, cortisol,   serotonin);
    dyn(oxytocin,     v_oxy,  base_oxy,  0.0f,             serotonin, cortisol);
    dyn(melatonin,    v_mel,  base_mel,  0.0f,             serotonin, adrenaline);
    dyn(noradrenaline,v_nor,  base_nor,  0.0f,             adrenaline, cortisol);
    dyn(endorphin,    v_end,  base_end,  0.0f,             dopamine,   cortisol);
    dyn(acetylcholine,v_ach,  base_ach,  0.0f,             serotonin,  cortisol);
    dyn(testosterone, v_tes,  base_tes,  0.0f,             adrenaline, cortisol);

    // Energieausgleich – verhindert Drift des Gesamtsystems
    float total = dopamine + serotonin + cortisol + adrenaline + oxytocin +
                  melatonin + noradrenaline + endorphin + acetylcholine + testosterone;

    float avg = total / 10.0f;
    float correction = (0.5f - avg) * 0.002f;  // sanft zur Mitte
    dopamine += correction;
    serotonin += correction;
    cortisol += correction;
    adrenaline += correction;
    oxytocin += correction;
    melatonin += correction;
    noradrenaline += correction;
    endorphin += correction;
    acetylcholine += correction;
    testosterone += correction;

    dopamine = clip(dopamine);
    serotonin = clip(serotonin);
    cortisol = clip(cortisol);
    adrenaline = clip(adrenaline);
    oxytocin = clip(oxytocin);
    melatonin = clip(melatonin);
    noradrenaline = clip(noradrenaline);
    endorphin = clip(endorphin);
    acetylcholine = clip(acetylcholine);
    testosterone = clip(testosterone);
}
