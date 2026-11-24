#pragma once
#include "HybridCpuFrequency.h"

class CPdhCpuFreq
{
public:
    CPdhCpuFreq();
    // GHz averaged per physical core. The first sample initializes PDH.
    bool GetCpuFreq(float& freq);
private:
    HybridCpuFrequency sampler;
};
