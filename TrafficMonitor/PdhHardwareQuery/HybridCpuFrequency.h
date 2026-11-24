#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <PdhMsg.h>
#include <powerbase.h>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cwchar>
#include <algorithm>

// Each processor has its own performance reference on a hybrid CPU.
class HybridCpuFrequency
{
public:
    struct Core { WORD group; KAFFINITY mask; BYTE efficiency; };
    struct Result { double all = -1, performance = -1, efficiency = -1; };
    std::vector<Core> cores;
    static bool ToGHz(double percent, ULONG referenceMHz, double& result)
    {
        if (!std::isfinite(percent) || percent <= 0 || referenceMHz == 0) return false;
        const double value = percent * referenceMHz / 100000.0;
        if (!std::isfinite(value) || value <= 0 || value > 20) return false;
        result = value;
        return true;
    }
    HybridCpuFrequency() = default;
    HybridCpuFrequency(const HybridCpuFrequency&) = delete;
    HybridCpuFrequency& operator=(const HybridCpuFrequency&) = delete;
    ~HybridCpuFrequency() { if (query) PdhCloseQuery(query); }
    bool Sample(Result& result)
    {
        result = {};
        if (!query && !Initialize()) return false;
        if (PdhCollectQueryData(query) != ERROR_SUCCESS) return false;
        DWORD bytes = 0, count = 0;
        const DWORD format = PDH_FMT_DOUBLE | PDH_FMT_NOCAP100;
        if (PdhGetFormattedCounterArrayW(counter, format, &bytes, &count, nullptr) != PDH_MORE_DATA) return false;
        std::vector<unsigned char> buffer(bytes);
        auto items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, format, &bytes, &count, items) != ERROR_SUCCESS) return false;
        std::vector<double> sums(cores.size());
        std::vector<unsigned> samples(cores.size());
        for (DWORD i = 0; i < count; ++i)
        {
            unsigned group = 0, number = 0;
            wchar_t trailing = 0;
            if (swscanf_s(items[i].szName, L"%u,%u%c", &group, &number, &trailing, 1u) != 2) continue;
            const auto found = processors.find({group, number});
            if (found == processors.end()) continue;
            const auto& value = items[i].FmtValue;
            if (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA) return false;
            double ghz = 0;
            if (!ToGHz(value.doubleValue, found->second.referenceMHz, ghz)) return false;
            sums[found->second.core] += ghz;
            ++samples[found->second.core];
        }
        double total = 0, pSum = 0, eSum = 0;
        unsigned pCount = 0, eCount = 0;
        BYTE low = 255, high = 0;
        for (const auto& core : cores) {
            low = (std::min)(low, core.efficiency);
            high = (std::max)(high, core.efficiency);
        }
        for (size_t i = 0; i < cores.size(); ++i)
        {
            // Average SMT siblings first, then weight physical cores equally.
            if (samples[i] == 0) return false;
            const double value = sums[i] / samples[i];
            total += value;
            if (high != low) {
                if (cores[i].efficiency == high) { pSum += value; ++pCount; }
                else { eSum += value; ++eCount; }
            }
        }
        if (cores.empty()) return false;
        result.all = total / cores.size();
        if (pCount) result.performance = pSum / pCount;
        if (eCount) result.efficiency = eSum / eCount;
        return true;
    }
private:
    struct PowerInfo { ULONG number, maxMHz, currentMHz, limitMHz, maxIdle, currentIdle; };
    struct Processor { ULONG referenceMHz; size_t core; };
    std::map<std::pair<unsigned, unsigned>, Processor> processors;
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    bool Initialize()
    {
        cores.clear(); processors.clear();
        // Power API numbering across multiple groups is ambiguous: fail closed.
        if (GetActiveProcessorGroupCount() != 1) return false;
        const DWORD logicalCount = GetActiveProcessorCount(0);
        std::vector<PowerInfo> power(logicalCount);
        if (!logicalCount || CallNtPowerInformation(ProcessorInformation, nullptr, 0, power.data(),
            static_cast<ULONG>(power.size() * sizeof(PowerInfo))) != 0) return false;
        DWORD bytes = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
        if (!bytes) return false;
        std::vector<unsigned char> topology(bytes);
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(topology.data()), &bytes)) return false;
        for (DWORD offset = 0; offset < bytes;)
        {
            auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(topology.data() + offset);
            if (!info->Size || offset + info->Size > bytes) return false;
            const auto& proc = info->Processor;
            for (WORD g = 0; g < proc.GroupCount; ++g)
            {
                const auto& affinity = proc.GroupMask[g];
                const size_t coreIndex = cores.size();
                cores.push_back({affinity.Group, affinity.Mask, proc.EfficiencyClass});
                for (unsigned n = 0; n < sizeof(KAFFINITY) * 8; ++n)
                {
                    if (!(affinity.Mask & (KAFFINITY(1) << n))) continue;
                    if (affinity.Group != 0 || n >= power.size() || power[n].number != n || !power[n].maxMHz) return false;
                    processors[{affinity.Group, n}] = {power[n].maxMHz, coreIndex};
                }
            }
            offset += info->Size;
        }
        if (processors.size() != logicalCount) return false;
        PDH_HQUERY opened = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &opened) != ERROR_SUCCESS) return false;
        if (PdhAddEnglishCounterW(opened, L"\\Processor Information(*)\\% Processor Performance", 0, &counter) != ERROR_SUCCESS
            || PdhCollectQueryData(opened) != ERROR_SUCCESS)
        {
            PdhCloseQuery(opened); counter = nullptr;
            return false;
        }
        query = opened;
        return true;
    }
};
