# powergov

A dynamic CPU frequency governor manager for Linux systems that automatically adjusts CPU scaling governors based on real-time CPU load. powergov implements an intelligent state machine with implicit hysteresis to optimize power consumption while maintaining system responsiveness.

## Overview

powergov is a lightweight daemon that monitors CPU utilization and dynamically switches between CPU frequency governors (`powersave`, `schedutil`, and `performance`) to balance power efficiency and performance. The system operates as a background service that continuously evaluates CPU load and makes governor transitions according to a state machine with built-in hysteresis to prevent rapid oscillations.

## Functionality

powergov operates through a continuous monitoring loop that:

1. **Monitors CPU Load**: Samples CPU utilization by reading `/proc/stat` and calculating usage over a 200ms measurement window
2. **State Management**: Maintains a three-state system (POWERSAVE, BALANCED, PERFORMANCE) representing different power/performance profiles
3. **Governor Switching**: Dynamically sets the appropriate CPU frequency governor for all CPU cores via `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`
4. **Battery Monitoring**: When battery-safe mode is enabled, monitors battery level and prevents performance mode when battery is low
5. **Dynamic Configuration**: Supports runtime configuration changes via Unix domain socket, allowing threshold adjustments without service restart
6. **Background Operation**: Runs as a daemon process, operating independently as a system service

### State Transitions

The system implements the following state machine:

- **POWERSAVE → BALANCED**: Triggered when CPU load exceeds 25%
- **BALANCED → PERFORMANCE**: Triggered when CPU load exceeds 75%
- **PERFORMANCE → BALANCED**: Triggered when CPU load falls below 60%
- **BALANCED → POWERSAVE**: Triggered when CPU load falls below 25%

## Algorithmic Features: Implicit Hysteresis

powergov implements **implicit hysteresis** through asymmetric transition thresholds. This design prevents rapid governor switching (thrashing) that would occur with symmetric thresholds.

### How Implicit Hysteresis Works

The hysteresis mechanism is implicit in the state transition logic:

- **Performance threshold asymmetry**: To enter PERFORMANCE state, load must exceed 75%, but to exit it, load must drop below 60%. This creates a 15% dead zone (60-75%) that prevents oscillation when load fluctuates around a single threshold.

- **State-dependent behavior**: The transition conditions depend on the current state, not just the absolute load value. This ensures that once a state is entered, it requires a different load level to exit, naturally creating hysteresis.

### Benefits of Implicit Hysteresis

1. **Stability**: Prevents rapid switching between governors when CPU load hovers around transition points
2. **Reduced overhead**: Minimizes the computational cost and system disruption from frequent governor changes
3. **Predictable behavior**: Provides consistent system response characteristics
4. **Battery optimization**: Allows the system to remain in power-saving states longer, as it requires sustained higher load to transition upward

## Power Efficiency and Battery Savings

powergov can significantly improve battery life on laptops and mobile devices through several mechanisms:

1. **Aggressive Power Saving**: When CPU load is below 25%, the system uses the `powersave` governor, which keeps CPU frequencies at minimum levels, reducing power consumption.

2. **Dynamic Scaling**: Instead of running at fixed high frequencies (as with `performance` governor) or always using balanced scaling, powergov adapts to actual workload demands, scaling up only when necessary.

3. **Reduced CPU Voltage**: Lower CPU frequencies require lower operating voltages. Since power consumption scales quadratically with voltage (P ∝ V²f), even small frequency reductions yield substantial power savings.

4. **Thermal Efficiency**: Lower power consumption reduces heat generation, which can allow the system to maintain lower fan speeds or passive cooling, further reducing overall system power draw.

5. **Idle Optimization**: During periods of low activity, the system remains in POWERSAVE state, allowing the CPU to spend more time in deeper idle states (C-states), which consume minimal power.

The actual battery savings depend on workload characteristics, but typical improvements range from 10-30% for mixed usage patterns, with higher gains during light workloads.

## License

This project is licensed under the **GNU General Public License version 3 (GPL-3.0)**.

### What is GPL-3?

The GNU General Public License version 3 is a copyleft license that ensures software remains free and open. Key provisions include:

- **Freedom to use**: Anyone can run the software for any purpose
- **Freedom to study**: Source code must be available, allowing users to understand how the software works
- **Freedom to modify**: Users can change the software to suit their needs
- **Freedom to distribute**: Users can share the software, including modified versions

**Copyleft requirement**: If you distribute modified versions of GPL-3 licensed software, you must also license your modifications under GPL-3, ensuring the software and its derivatives remain free and open.

This license ensures that powergov and any improvements to it remain available to the community under the same terms.

## Building and Installation

Full operational documentation: **[Documentation/README.md](Documentation/README.md)** (architecture, configuration, modules, metrics, theoretical battery impact).

### Prerequisites

- GCC compiler
- Linux kernel with CPU frequency scaling support
- Root/sudo access (required for governor switching)

### Build

```bash
make
```

### Install

```bash
sudo make install
```

### Install as System Service

```bash
sudo make install-service
sudo systemctl enable powergov.service
sudo systemctl start powergov.service
```

### Desktop UI (GTK)

```bash
make powergov-ui
sudo make install-ui install-ui-policy install-ui-helper
powergov-ui
```

Requires `gtk+-3.0` (pkg-config). Installs icon, `.desktop` entry and Polkit policy for diagnostic mode.

## Usage

### Basic Commands

**Start powergov:**
```bash
sudo powergov on
```

**Stop powergov:**
```bash
sudo powergov off
```

**Display help:**
```bash
powergov --help
```

### Battery-Safe Mode

powergov supports dynamic battery-aware configuration to prevent the system from using performance mode when battery is low.

**Enable battery-safe mode with threshold:**
```bash
sudo powergov --battery-safe <percent>
```

This command configures powergov to disable the `performance` governor when battery level falls below the specified threshold. The configuration is applied immediately to the running process without requiring a restart.

**Examples:**
```bash
sudo powergov --battery-safe 50    # Disable performance mode when battery <= 50%
sudo powergov --battery-safe 20    # Disable performance mode when battery <= 20%
sudo powergov --battery-safe 0     # Disable battery-safe mode (allow performance at any battery level)
```

When battery-safe mode is enabled:
- If battery level is above the threshold: normal operation, performance mode can be used
- If battery level is at or below the threshold: performance governor is blocked, system stays in `schedutil` or `powersave`
- Configuration can be changed on-the-fly while powergov is running
- Battery level is checked every ~10 seconds

### Service Management

If installed as a systemd service:

```bash
sudo systemctl start powergov    # Start the service
sudo systemctl stop powergov     # Stop the service
sudo systemctl status powergov   # Check service status
sudo systemctl restart powergov  # Restart the service
```

## Technical Details

- **Sampling interval**: 2 seconds
- **CPU load measurement window**: 200ms
- **Transition thresholds**: 
  - Low: 25% (0.25)
  - Medium: 60% (0.60)
  - High: 75% (0.75)
- **Supported governors**: powersave, schedutil, performance
- **CPU detection**: Automatically detects and manages all available CPU cores

## Requirements

- Linux operating system
- Kernel with CPU frequency scaling subsystem (`CONFIG_CPU_FREQ`)
- `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` interface
- `/proc/stat` for CPU load monitoring
- Root privileges for governor switching

## Architecture

```
powergov/
├── main.c              # CLI entry point and daemon management
├── governor/
│   ├── governor.c      # CPU governor switching implementation
│   ├── governor.h
│   ├── loop.c          # Main state machine and monitoring loop
│   └── loop.h
├── cpu/
│   ├── cpu_load.c      # CPU utilization measurement
│   └── cpu_load.h
├── Battery/
│   ├── battery.c       # Battery level detection
│   └── battery.h
└── service/
    └── powergov.service # systemd service file
```

## Contributing

Contributions are welcome. Please ensure that any modifications maintain compatibility with the GPL-3 license and include appropriate documentation.

## Disclaimer

This software modifies system-level CPU frequency scaling behavior. While designed to be safe, users should understand that incorrect governor settings or system misconfiguration could affect system performance. Use at your own risk.

