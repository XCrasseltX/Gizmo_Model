#pragma once
#include <vector>
#include <cstdint>
#include "hormones.h"

class Neurons {
public:
    int N = 0;

    std::vector<float> V, Vth, Vrest, Vreset, ref_left;
    std::vector<float> Isyn;
    std::vector<uint8_t> spk;
    std::vector<int>* input_neurons = nullptr;

    float tau_m = 0.020f;  
    float tref  = 0.002f;  
    float dt    = 0.001f;  

    void init(int n);
    void apply_hormones(const HormoneSystem& H);
    void step();

};
