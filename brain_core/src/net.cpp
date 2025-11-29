#include "net.h"
#include <iostream>
#include <cmath>
#include "io_logger.h"

void Net::build_small_demo(int N, int fan_in, int n_inputs, int n_outputs) {
    neu.init(N);
    neu.input_neurons = &input_target;

    this->n_inputs = n_inputs;
    this->n_outputs = n_outputs;

    // 1️⃣ Input- und Output-IDs festlegen
    input_target.resize(n_inputs);
    for (int i = 0; i < n_inputs; ++i)
        input_target[i] = i; // ganz links
    
    is_input.assign(N, false);
    for (int i : input_target)
        is_input[i] = true;

    output_target.resize(n_outputs);
    for (int i = 0; i < n_outputs; ++i)
        output_target[i] = N - n_outputs + i; // ganz rechts

    is_output.assign(N, false);
    for (int i : output_target)
        is_output[i] = true;


    // 20 % Inhibitoren
    int N_inh = static_cast<int>(0.2f * N);
    std::vector<bool> is_inhibitory(N, false);
    for (int i = 0; i < N_inh; ++i) is_inhibitory[i] = true;

    for (int i = 0; i < n_inputs; ++i)
    is_inhibitory[i] = false;

    std::mt19937 r2(123);
    syn.clear();
    syn.reserve(static_cast<size_t>(N) * fan_in);

    for (int post = 0; post < N; ++post) {
        for (int k = 0; k < fan_in; ++k) {
            int pre = r2() % N;
            if (pre == post) continue;

            // Optional: Output-Neuronen senden keine Signale
            if (is_output[pre]) continue;

            // Basisgewicht: schwächer als vorher
            float w = 0.1f + 0.2f * ((r2() % 100) / 100.0f); 

            // Inhibitorische Synapsen machen negative Gewichte
            if (is_inhibitory[pre])
                w *= -2.0f; // stärkere Wirkung (z.B. -0.2 bis -0.6)

            syn.push_back({ pre, post, w });
        }
    }

    syn_by_pre.resize(syn.size());
    std::iota(syn_by_pre.begin(), syn_by_pre.end(), 0);

    std::sort(syn_by_pre.begin(), syn_by_pre.end(), [&](int a, int b) {
        if (syn[a].pre != syn[b].pre) return syn[a].pre < syn[b].pre;
        return syn[a].post < syn[b].post;
    });

    pre_offsets.assign(N + 1, 0);
    for (int idx : syn_by_pre) pre_offsets[syn[idx].pre + 1]++;
    for (int i = 1; i <= N; ++i) pre_offsets[i] += pre_offsets[i - 1];

    ring_init(); 

    pre_trace.assign(syn.size(), 0.0f);
    post_trace.assign(syn.size(), 0.0f);

    for (auto& s : syn) {
        s.delay = static_cast<uint16_t>(rng() % 4);
    }
}

void Net::inject_inputs(float dt) {

    // Externe Inputs über commands.jsonl
    if (external_input_active && !external_input_pattern.empty()) {
        int count = std::min<int>(external_input_pattern.size(), neu.N);
        for (int i = 0; i < count; ++i) {
            if (external_input_pattern[i]) {
                neu.Isyn[i] += 1.0f; // kleiner Stromstoß -> Spike möglich
            }
        }
        external_input_active = false; // nur 1 Schritt aktiv
    }

    // Hintergrundrauschen NUR auf Input-Neuronen und schwächer:
    const float noise_p   = 0.0002f;  // vorher 0.0005
    const float noise_amp = 0.05f;    // vorher 0.2
    for (int idx : input_target) {
        if (uni(rng) < noise_p) neu.Isyn[idx] += noise_amp;
    }
}

void Net::route_spikes_no_delay() {
    const auto& spk = neu.spk;
    for (int pre = 0; pre < neu.N; ++pre) {
        if (!spk[pre]) continue;
        if (is_output[pre]) continue;
        if (is_input[pre]) continue;  // Input nicht weiterleiten

        int begin = pre_offsets[pre], end = pre_offsets[pre + 1];
        for (int p = begin; p < end; ++p) {
            int sidx = syn_by_pre[p];
            const auto& s = syn[sidx];

            // aktuelle "Tiefe" aus delay (0..R)
            int depth = s.delay;
            if (depth > max_propagation_depth) continue;

            float decay = powf(spike_decay_per_hop, depth);
            float val = s.w * decay;  // <--- dämpft exponentiell mit Tiefe

            uint16_t dslot = static_cast<uint16_t>((rpos + s.delay) % R);
            ring_enqueue(s.post, dslot, val);
        }
    }
}

void Net::step_once(float external_reward) {
    H.update(neu.dt);
    neu.apply_hormones(H);
    ring_collect_to_Isyn();
    inject_inputs(neu.dt);
    route_spikes_no_delay();
    neu.step();

    stdp_decay_traces();
    stdp_apply_updates();

    rpos = static_cast<uint16_t>((rpos + 1) % R);
}

void Net::ring_init() {
    ring.assign(static_cast<size_t>(neu.N) * R, 0.0f);
    rpos = 0;
}

void Net::ring_collect_to_Isyn() {
    const size_t N = static_cast<size_t>(neu.N);
    const uint16_t slot = rpos;
    for (size_t i = 0; i < N; ++i) {
        float& cell = ring[i * R + slot];
        neu.Isyn[i] += cell;
        cell = 0.0f;
    }
}

inline void Net::ring_enqueue(int post, uint16_t dslot, float val) {
    ring[static_cast<size_t>(post) * R + dslot] += val;
}

void Net::stdp_decay_traces() {
    const float dt = neu.dt;
    const float dp = std::exp(-dt / tau_pre);
    const float dq = std::exp(-dt / tau_post);
    for (size_t i = 0; i < syn.size(); ++i) {
        pre_trace[i]  *= dp;
        post_trace[i] *= dq;
    }
}

void Net::stdp_apply_updates() {
    const float mod = 1.0f + 0.5f * H.current.dopamine - 0.3f * H.current.cortisol;

    for (size_t si = 0; si < syn.size(); ++si) {
        // Skip inhibitory synapses
        if (syn[si].w < 0.0f) continue;

        const auto& s = syn[si];
        const bool pre_sp  = (s.pre  >=0 && s.pre  < neu.N) ? (neu.spk[s.pre]  != 0) : false;
        const bool post_sp = (s.post >=0 && s.post < neu.N) ? (neu.spk[s.post] != 0) : false;

        if (pre_sp)  pre_trace[si]  += 1.0f;
        if (post_sp) post_trace[si] += 1.0f;

        float dw = 0.0f;
        if (post_sp) dw += learning_rate * Aplus  * pre_trace[si]  * mod; 
        if (pre_sp)  dw -= learning_rate * Aminus * post_trace[si] * mod; 

        syn[si].w = std::clamp(syn[si].w + dw, wmin, wmax);
    }
}
