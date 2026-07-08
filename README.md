# powergov

**Modular laptop power management for Linux** — a lightweight userspace daemon that coordinates CPU governors, EPP, turbo, frequency caps, ACPI platform profile, PCI/USB runtime PM, disk APM/ALPM, PCIe ASPM, Bluetooth power save, and WiFi/audio peripherals. Policy is **reactive** (CPU load + mode + battery + context → `device_aggression` 0–3), not fixed AC/BAT rules like TLP. Includes **GTK 3 UI** and **AppImage**.

Current release: **1.13.0** (`VERSION`).

## What it does

powergov monitors CPU load (200 ms sample, ~2 s policy tick) and power source (AC/battery), optionally lid state and logind session idle, then applies a **coordinated policy** through standard sysfs and a few external tools (`iw` for WiFi power save). A three-state machine (POWERSAVE → BALANCED → PERFORMANCE) uses **temporal hysteresis** (three consecutive samples) and **asymmetric thresholds** to avoid governor thrashing.

| User mode | Typical use on battery |
|-----------|-------------------------|
| **max-battery** (default) | Aggressive saving: no performance governor, turbo off, low EPP, freq cap, runtime PM, platform low-power |
| **balanced** | Moderate saving; performance blocked unless battery-safe override |
| **performance** | Allows performance governor and turbo on battery (explicit choice) |
| **custom** | Fine-grained toggles: allow performance, aggressive runtime PM, **on-demand peripherals** |

Subsytems are individually enable/disable via `FEATURES` in `/etc/powergov.conf` or `powergov feature <name> on|off`.

### Context boost (v1.13)

On battery, optional **lid** and **display idle** triggers can raise `device_aggression` to maximum when CPU load is low — e.g. laptop closed or screen locked with light background load. See [Documentation/contexto-reactivo.md](Documentation/contexto-reactivo.md).

### vs TLP (philosophy)

| | **TLP** | **powergov** |
|--|---------|--------------|
| Model | Static AC/BAT profiles | **Reactive** `device_aggression` from load + gov state + context |
| Config | Large `tlp.conf` | Modes + feature toggles; tuning in UI |
| Device PM | Always on BAT profile | Relaxes when load rises; boosts on lid/idle when configured |
| Coexistence | — | Defers device PM to TLP when TLP active; viable **alternative** when TLP off |

Benchmark: `scripts/bench-battery-session.sh powergov|tlp`.

## Desktop UI (`powergov-ui`)

- **User tab:** profile selection, battery-safe threshold, status summary, install/uninstall service, update check (GitHub Releases).
- **Information tab:** system context (lid, session idle), compatibility (TLP, ppd), metrics, log, feature toggles, custom tuning, context checkboxes.
- **About tab:** version (same layout as `pack -v` / `powergov -v`), license, source link, daemon service version.
- **Optimistic pending:** feature/tuning checkboxes show immediate feedback while the daemon confirms.
- **System tray:** closing the window hides to tray (lightning icon); `POWERGOV_NO_TRAY=1` disables tray behaviour.
- **i18n:** English default; Spanish when `LANG`/`LC_MESSAGES` starts with `es`, or via EN/ES toggle.

Requires the **daemon** running (`systemd` or `sudo powergov on`). The AppImage bundles the UI; first-run wizard can install the backend with admin password once.

## Quick start (AppImage — recommended)

```bash
chmod +x PowerGov-1.13.0-x86_64.AppImage
./PowerGov-1.13.0-x86_64.AppImage
```

On first launch: menu/desktop entry, language detection, optional one-time service install. Updates install to `~/.local/share/powergov/PowerGov.AppImage` without duplicate copies in Downloads.

Download from [GitHub Releases](https://github.com/IRodriguez13/powergov/releases) (tag `v1.13.0`).

## Quick start (from source)

```bash
git clone https://github.com/IRodriguez13/powergov.git
cd powergov
make
sudo make install-service    # binary + /etc/powergov.conf + systemd + man pages
powergov status

# Optional GTK UI
make powergov-ui
sudo make install-ui install-ui-policy install-ui-helper
powergov-ui
```

**Build deps:** `build-essential`, `pkg-config`, `libgtk-3-dev` (UI), `zenity` (install scripts). **Runtime:** root for governor/sysfs writes; `iw` optional for WiFi peripheral PM; systemd-logind for session idle hint.

## Coexistence with TLP and power-profiles-daemon

| Tool | powergov behaviour |
|------|-------------------|
| **power-profiles-daemon** | Detected automatically; **platform_profile** control is skipped to avoid conflicting writes. CPU/EPP/turbo/runtime PM remain active unless disabled. |
| **TLP** | When TLP is active, powergov **defers** `runtime_pm`, `peripheral_pm`, `disk_pm`, `pcie_aspm`, `bluetooth_pm`. Use powergov alone for full device PM stack. |

See `platform/tlp_compat.c` and `Documentation/operacion.md`.

## CLI essentials

```bash
sudo powergov on | off
powergov status
sudo powergov mode max-battery|balanced|performance|custom
sudo powergov feature governor|epp|freq_cap|turbo|platform|runtime_pm|peripheral_pm|disk_pm|pcie_aspm|bluetooth_pm on|off
sudo powergov --battery-safe 30    # 0 = off
powergov dev-metrics               # apply/verify counters
sudo powergov dev-log --tail 50
powergov --version
```

Persistent config: **`/etc/powergov.conf`**. Runtime changes via Unix socket **`/run/powergov/powergov.sock`** (no restart).

## Documentation

| Resource | Content |
|----------|---------|
| [Documentation/README.md](Documentation/README.md) | Index (architecture, config, context, operation, sysfs, metrics) |
| [Documentation/contexto-reactivo.md](Documentation/contexto-reactivo.md) | Lid/idle boost (v1.13) |
| [ui/README.md](ui/README.md) | GTK UI build, AppImage, tray, pending UI |
| `man powergov` / `man 8 powergov` / `man powergov-ui` / `man 5 powergov.conf` | Manual pages (`doc/`) |

## Project layout

```
powergov/
├── main.c                 # CLI entry
├── core/                  # loop, sysfs, info, socket protocol
├── cpu/                   # governor, epp, freq_cap, turbo
├── platform/              # ACPI platform_profile, TLP detection
├── devices/               # runtime_pm, peripheral_pm, disk_pm, pcie_aspm, bluetooth_pm
├── power/                 # battery/AC, lid, session_idle, effective profile
├── config/                # /etc/powergov.conf parser
├── client/                # libpowergov.so (UI + tools)
├── ui/                    # GTK 3 desktop application
├── scripts/               # install, AppImage, XDG helpers, release
├── doc/                   # mandoc pages (.1, .5, .8)
└── Documentation/         # design and operation (Spanish)
```

## Release and AppImage (maintainers)

```bash
make pack              # dist/powergov-<version>.tar.gz
make appimage          # dist/PowerGov-<version>-x86_64.AppImage
make release           # gh release create/upload (requires gh auth)
```

## License

**GPL-3.0** — see [LICENSE](LICENSE). Modifications distributed under the same license.

## Disclaimer

This software modifies system-level power management (CPU frequency, platform profile, device PM). Misconfiguration or overlap with other tools may affect performance or stability. Use at your own risk.

**Author:** Iván Ezequiel Rodriguez
