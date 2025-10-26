#pragma once
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <cstdint>
#include "neurons.h"
#include "hormones.h"

// einfache Synapse (ohne Delay)
struct Synapse {
    int   pre;
    int   post;
    float w;
    uint16_t delay; // in "Ticks" (dt-Schritten)
};

class Net {
public:
    Neurons neu;
    std::vector<Synapse> syn;

    // Ganz oben in der Klasse Net:
    std::vector<uint8_t> external_input_pattern; // temporäres Muster (0/1)
    bool external_input_active = false;

    // PRE-gruppierte Adjazenz (CSR-ähnlich)
    std::vector<int> pre_offsets; 
    std::vector<int> syn_by_pre;  

    // Externe Inputs
    int n_inputs = 3;

    int n_outputs = 0;
    std::vector<int> output_target; // IDs der Output-Neuronen
    std::vector<bool> is_output;

    std::vector<float> input_rate_hz;
    std::vector<int>   input_target;
    std::vector<bool> is_input;

    std::mt19937 rng{42};
    std::uniform_real_distribution<float> uni{0.0f, 1.0f};

    HormoneSystem H;
    float learning_rate = 0.005f; 

    // --- Delay-Ringpuffer ---
    uint16_t R = 16;   
    uint16_t rpos = 0; 
    std::vector<float> ring; 

    void ring_init();                 
    void ring_collect_to_Isyn();      
    void ring_enqueue(int post, uint16_t dslot, float val); 

    std::vector<float> pre_trace;   
    std::vector<float> post_trace;  

    // STDP-Parameter
    float tau_pre  = 0.020f;    
    float tau_post = 0.020f;    
    float Aplus    = 0.0001f;    
    float Aminus   = 0.00012f;   
    float wmin     = 0.0f;
    float wmax     = 0.2f;

    float spike_decay_per_hop = 0.1f;  // 30% Signal bleibt übrig
    int max_propagation_depth = 5;     // danach keine Weiterleitung mehr

    void stdp_decay_traces();          
    void stdp_apply_updates();    

public:
    void build_small_demo(int N, int fan_in, int n_inputs, int n_outputs);
    void inject_inputs(float dt);
    void route_spikes_no_delay();
    void step_once(float external_reward);
};
