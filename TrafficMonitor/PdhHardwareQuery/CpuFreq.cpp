#include "stdafx.h"
#include "CpuFreq.h"

CPdhCpuFreq::CPdhCpuFreq() = default;

bool CPdhCpuFreq::GetCpuFreq(float& freq)
{
    HybridCpuFrequency::Result result;
    if (!sampler.Sample(result))
    {
        freq = -1;
        return false;
    }
    freq = static_cast<float>(result.all);
    return true;
}
