#include "pattern_gen.h"
#include <random>
#include <algorithm>

std::vector<int> text_to_pattern(const std::string& text, int n_inputs) {
    std::vector<int> pat(n_inputs, 0);
    std::seed_seq seed(text.begin(), text.end());
    std::mt19937 rng(seed);
    float density = std::clamp(0.10f + 0.15f*(text.size()/100.0f), 0.05f, 0.35f);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);
    for (int i=0; i<n_inputs; i++)
        pat[i] = (U(rng) < density) ? 1 : 0;
    return pat;
}
