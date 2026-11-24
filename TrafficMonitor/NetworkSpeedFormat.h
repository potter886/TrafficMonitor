#pragma once
#include <cmath>
#include <cwchar>
#include <string>

namespace NetworkSpeedFormat
{
    inline double RoundThree(double value)
    {
        if (!std::isfinite(value) || value <= 0) return 0;
        const double step = std::pow(10.0, std::floor(std::log10(value)) - 2);
        return std::round(value / step) * step;
    }

    inline std::wstring ThreeSignificantDigits(double value)
    {
        if (!std::isfinite(value) || value < 0) return L"--";
        const double rounded = RoundThree(value);
        int decimals = rounded > 0 ? static_cast<int>(2 - std::floor(std::log10(rounded))) : 2;
        if (decimals < 0) decimals = 0;
        // Tiny rates do not need extra columns of leading zeroes on a taskbar.
        if (decimals > 2) decimals = 2;
        wchar_t text[128];
        swprintf_s(text, L"%.*f", decimals, rounded);
        return text;
    }

    // Start at K, as the existing network formatter does. Promote before a
    // rounded value becomes 1000 so the automatic display stays compact.
    struct Scaled { double value; unsigned unit; };
    inline Scaled Automatic(double bytesOrBits)
    {
        Scaled result{bytesOrBits / 1024.0, 0};
        while (result.unit < 3 && RoundThree(result.value) >= 1000) {
            result.value /= 1024.0;
            ++result.unit;
        }
        return result;
    }
}
