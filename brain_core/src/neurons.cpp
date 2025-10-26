#include "neurons.h"
#include "hormones.h"
#include <algorithm>

void Neurons::init(int n) {
    N = n;
    V.assign(N, -0.065f);
    Vth.assign(N, -0.050f);
    Vrest.assign(N, -0.065f);
    Vreset.assign(N, -0.070f);
    ref_left.assign(N, 0.0f);
    Isyn.assign(N, 0.0f);
    spk.assign(N, 0);
}

void Neurons::apply_hormones(const HormoneSystem& H) {
    for (int i = 0; i < N; ++i) {

        float vth_change = 0.0f;
        float isyn_factor = 1.0f;
        float tau_factor  = 1.0f;
        float tref_factor = 1.0f;

        // --- Einfluss der Hormone ---
        vth_change += -0.010f * H.dopamine;
        vth_change += +0.015f * H.melatonin;
        vth_change += -0.020f * H.cortisol;
        vth_change += -0.005f * H.endorphin;
        vth_change += -0.010f * H.adrenaline;

        isyn_factor *= (1.0f + 0.5f * H.adrenaline);
        isyn_factor *= (1.0f + 0.2f * H.dopamine);
        isyn_factor *= (1.0f - 0.3f * H.oxytocin);

        tau_factor  *= (1.0f + 0.3f * H.noradrenaline - 0.2f * H.acetylcholine);
        tref_factor *= (1.0f + 0.4f * H.melatonin - 0.2f * H.endorphin);

        // --- Anwenden ---
        Vth[i] += vth_change * dt;
        Vth[i] = std::clamp(Vth[i], -0.080f, -0.030f);

        tau_m  = 0.020f * tau_factor;
        tref   = 0.002f * tref_factor;

        Isyn[i] *= isyn_factor;
    }
}

void Neurons::step() {
    for (int i = 0; i < N; ++i) {

        // Input-Neuronen überspringen, sie feuern nur extern
        if (input_neurons && std::find(input_neurons->begin(), input_neurons->end(), i) != input_neurons->end()) {
            spk[i] = 0;
            V[i] = Vrest[i];
            Isyn[i] = 0.0f;
            continue;
        }
        
        spk[i] = 0;
        if (ref_left[i] > 0.0f) {
            ref_left[i] -= dt;
            V[i] = Vreset[i];
            continue;
        }
        const float dV = (-(V[i] - Vrest[i]) + Isyn[i]) * (dt / tau_m);
        V[i] += dV;

        if (V[i] >= Vth[i]) {
            V[i] = Vreset[i];
            ref_left[i] = tref;
            spk[i] = 1;
        }
    }
    std::fill(Isyn.begin(), Isyn.end(), 0.0f);
}
