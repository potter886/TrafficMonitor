# Hybrid CPU customization

This branch keeps the standard TrafficMonitor application with updated CPU frequency and temperature monitoring. It also adds a fixed taskbar width for network rates rounded to up to three significant digits.

## Changes

- Calculate frequency from each logical processor's `% Processor Performance` and its own `MaxMhz` reference. Average SMT siblings before averaging physical cores. A package-wide maximum must not scale a hybrid CPU's aggregate performance percentage.
- Add the Hybrid CPU Frequency plugin with separate P-core and E-core averages. E includes low-power E-cores. The classification uses Windows `EfficiencyClass`; the highest class is P. Multiple processor groups are currently unsupported and return unavailable values.
- Use LibreHardwareMonitor 0.9.6 with PawnIO for CPU package temperature. Missing sensor values stay unavailable; distance to TjMax is not averaged as an actual temperature. Prefer the library's Core Average when an average is requested.
- Use a fixed `88.8MB/s` width sample for the three-digit network format. Column positions stay fixed as live values change.
- Optionally round network rates to up to three significant digits, with at most two decimal places for small rates. Automatic units promote before rounding would produce 1000. For example, `19.64 → 19.6`, `123.45 → 123`, and `9.999 → 10.0`.

Frequency values are Windows performance-counter samples, not effective clocks. Validated on an Intel Core Ultra 5 125H with 4 P-cores and 10 E-family cores, including 2 low-power E-cores.

## Build

Use Windows x64 with Visual Studio 2022 or newer. Install Desktop development with C++, MFC, C++/CLI support, and the .NET Framework 4.7.2 targeting pack. MFC must match the selected MSVC tools version.

```powershell
.\build-hybrid.ps1 -Test
```

The script selects an installed MFC toolset, verifies the pinned LibreHardwareMonitor release archive, builds the standard x64 application and plugin, copies required runtime dependencies, and runs the frequency and network-format checks. Output is in `Bin/x64/Release`. Downloads are cached under `.deps` and are not committed.

The script downloads the signed PawnIO installer but does not install a driver. Install `Bin/x64/Release/PawnIO_setup.exe` before using CPU temperature monitoring. Run TrafficMonitor as administrator for sensor access. Do not overwrite an active installation; close it and back up its executable, DLLs, and configuration first.

The historical LibreHardwareMonitor DLL retained by the upstream repository is not the dependency used by this branch. The project references the verified version under `.deps/LibreHardwareMonitor`; run the build script before opening/building the project directly.

## Settings

Close TrafficMonitor, then add these entries to the existing `[task_bar]` section of `config.ini`:

```ini
item_space = 8
digits_number = 3
three_digit_net_speed = true
```

`digits_number` in upstream TrafficMonitor controls reserved width, not numerical precision. `three_digit_net_speed` controls rounding and selects the matching fixed width sample. It defaults to false, preserving the upstream format when absent. Layout does not measure live network values.

For CPU package temperature, enable CPU hardware monitoring in General Settings, select `CPU Package`, and enable CPU temperature in taskbar Display Settings. Load `plugins/HybridCpuFrequency.dll` and select its P-core and E-core items for separate frequencies. Low-power E-cores are included in E; the plugin does not infer a separate LP E class.

The upstream README describes moving temperature monitoring to a plugin, while this repository still contains the standard project and its built-in sensor integration. This branch builds and updates that retained implementation.

## Dependency provenance

- [LibreHardwareMonitor v0.9.6](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases/tag/v0.9.6), archive SHA-256 `086d9f1b5a99e643edc2cfaaac16051685b551e4c5ac0b32a57c58c0e529c001`.
- PawnIO installer from `LibreHardwareMonitor/Resources/PawnIO_setup.exe` at the same tag, SHA-256 `a3a46226c5e2824f4cdd42be0eecbabfc672c86f7889710f5ab1e6ad385b47a0`. Authenticode publisher: namazso.
- [LibreHardwareMonitor license](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/v0.9.6/LICENSE) and [third-party notices](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/v0.9.6/THIRD-PARTY-NOTICES.txt).

TrafficMonitor's original license and attribution remain unchanged.
