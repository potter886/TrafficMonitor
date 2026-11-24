#include <Windows.h>
#include <atomic>
#include <memory>
#include <limits>
#include <cstdio>
#pragma warning(push)
#pragma warning(disable:4100)
#include "include/PluginInterface.h"
#pragma warning(pop)
#include "TrafficMonitor/PdhHardwareQuery/HybridCpuFrequency.h"

class FrequencyItem : public IPluginItem
{
public:
    const wchar_t* name;
    const wchar_t* id;
    const wchar_t* label;
    std::atomic<double> ghz{-1};
    FrequencyItem(const wchar_t* n, const wchar_t* i, const wchar_t* l) : name(n), id(i), label(l) {}
    const wchar_t* GetItemName() const override { return name; }
    const wchar_t* GetItemId() const override { return id; }
    const wchar_t* GetItemLableText() const override { return label; }
    const wchar_t* GetItemValueSampleText() const override { return L"4.50GHz"; }
    const wchar_t* GetItemValueText() const override {
        thread_local wchar_t text[32];
        const auto value = ghz.load();
        if (value > 0) swprintf_s(text, L"%.2fGHz", value);
        else wcscpy_s(text, L"--GHz");
        return text;
    }
};

class HybridFrequencyPlugin : public ITMPlugin
{
    HybridCpuFrequency sampler;
    FrequencyItem all{L"CPU frequency (physical-core average)", L"hybrid_cpu_avg_v1", L"Avg: "};
    FrequencyItem p{L"P-core frequency (average)", L"hybrid_cpu_p_v1", L"P: "};
    FrequencyItem e{L"E-core frequency (includes LP E-cores, average)", L"hybrid_cpu_e_v1", L"E: "};
    std::atomic<unsigned> pCount{0}, eCount{0};
public:
    IPluginItem* GetItem(int index) override {
        switch (index) { case 0: return &all; case 1: return &p; case 2: return &e; default: return nullptr; }
    }
    void DataRequired() override {
        HybridCpuFrequency::Result result;
        sampler.Sample(result);
        all.ghz = result.all; p.ghz = result.performance; e.ghz = result.efficiency;
        BYTE lo = 255, hi = 0;
        for (const auto& core : sampler.cores) { lo = (std::min)(lo, core.efficiency); hi = (std::max)(hi, core.efficiency); }
        unsigned pc = 0, ec = 0;
        if (lo != hi) for (const auto& core : sampler.cores) { if (core.efficiency == hi) ++pc; else ++ec; }
        pCount = pc; eCount = ec;
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
        case TMI_NAME: return L"Hybrid CPU Frequency";
        case TMI_DESCRIPTION: return L"Per-processor Windows performance reference, physical-core averaging, P/E split. E includes LP E-cores.";
        case TMI_AUTHOR: return L"Local customization";
        case TMI_VERSION: return L"1.0.0";
        default: return L"";
        }
    }
    const wchar_t* GetTooltipInfo() override {
        thread_local wchar_t text[400];
        swprintf_s(text, L"Hybrid CPU: %u P-cores / %u E-cores (including LP E-cores)\n"
            L"Per-core reference frequency; physical cores weighted equally.\n"
            L"P %.2f GHz | E %.2f GHz | All %.2f GHz\n"
            L"Windows sampled performance frequency; not effective clock.\n"
            L"-- means unavailable; multiple processor groups are unsupported.",
            pCount.load(), eCount.load(), p.ghz.load(), e.ghz.load(), all.ghz.load());
        return text;
    }
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    static HybridFrequencyPlugin plugin;
    return &plugin;
}

#ifdef HYBRID_FREQUENCY_TEST
int wmain()
{
    double value = -1;
    // Regression: an E-core turbo reference must never scale a P-core ratio.
    if (!HybridCpuFrequency::ToGHz(300, 1200, value) || std::abs(value - 3.6) > 1e-8) return 1;
    if (!HybridCpuFrequency::ToGHz(30, 3600, value) || std::abs(value - 1.08) > 1e-8) return 2;
    if (HybridCpuFrequency::ToGHz(std::numeric_limits<double>::quiet_NaN(), 1200, value)) return 3;
    if (HybridCpuFrequency::ToGHz(100, 0, value) || HybridCpuFrequency::ToGHz(-1, 1200, value)) return 4;
    HybridCpuFrequency sampler;
    HybridCpuFrequency::Result result;
    sampler.Sample(result);
    unsigned successful = 0;
    for (int i = 0; i < 5; ++i) {
        Sleep(1000);
        if (sampler.Sample(result)) ++successful;
        wprintf(L"Sample %d: all=%.3f P=%.3f E=%.3f GHz\n", i, result.all, result.performance, result.efficiency);
    }
    for (const auto& core : sampler.cores)
        wprintf(L"Core: group=%u mask=%llx efficiency=%u\n", core.group, static_cast<unsigned long long>(core.mask), core.efficiency);
    wprintf(L"Regression checks passed. Live samples: %u/5\n", successful);
    return successful == 5 ? 0 : 5;
}
#endif
