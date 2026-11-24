#include "../TrafficMonitor/NetworkSpeedFormat.h"
#include <cstdio>
#include <limits>
int wmain() {
    struct Test { double input; const wchar_t* output; };
    const Test tests[] = {{0,L"0.00"},{1.2345,L"1.23"},{9.999,L"10.0"},
        {19.64,L"19.6"},{99.99,L"100"},{123.45,L"123"},{0.012345,L"0.01"},
        {0.001,L"0.00"},{12345,L"12300"},{-1,L"--"},{std::numeric_limits<double>::infinity(),L"--"}};
    for (const auto& test : tests) {
        const auto actual = NetworkSpeedFormat::ThreeSignificantDigits(test.input);
        if (actual != test.output) { wprintf(L"FAIL %.8g: %s, expected %s\n",test.input,actual.c_str(),test.output); return 1; }
    }
    const auto before = NetworkSpeedFormat::Automatic(999.4*1024);
    const auto carry = NetworkSpeedFormat::Automatic(999.5*1024);
    if (before.unit != 0 || carry.unit != 1 || NetworkSpeedFormat::ThreeSignificantDigits(carry.value) != L"0.98") return 2;
    wprintf(L"Three-significant-digit and unit-boundary checks passed.\n");
}
