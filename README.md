# CAN Error Utilities

Simulate and monitor [CAN bus](https://en.wikipedia.org/wiki/CAN_bus) error messages using Linux and [SocketCAN](https://en.wikipedia.org/wiki/SocketCAN).

[![License](https://img.shields.io/badge/License-LGPL%202.1%20or%20later%20%7C%20BSD--3--Clause-blue.svg)](https://spdx.org/licenses/)



## Tools Overview

### canerrsim

**CAN Error Frame Simulator** - Generates custom CAN error frames for testing and development purposes.

Key features:
- Simulates 30+ different CAN error conditions
- Supports error class, arbitration loss, protocol errors, transceiver faults
- Customizable error counters and data payload
- Real-time error frame generation

### canerrdump
**CAN Error Frame Monitor** - Displays detailed error frame information from CAN interfaces

Key features:
- Flexible error filtering options
- Human-readable error descriptions
- Real-time error monitoring
- Protocol violation location decoding



## Installation

### Dependencies
- Linux kernel with SocketCAN support (in general any newer then 2009)
- Ubuntu/Debian development tools:
  ```bash
  sudo apt-get install build-essential
- Optional: If you do not have real CAN adapter then Virtual CAN adapter vcan0 can be set up like this:
  ```bash
  sudo modprobe vcan
  sudo ip link add dev vcan0 type vcan                         
  sudo ip link set vcan0 mtu 72              # needed for CAN FD
  sudo ip link set vcan0 up

### Download and Compilation
```
# Download
git clone https://github.com/zeljkoavramovic/canerrsim.git
cd canerrsim

# Build both tools
gcc canerrsim.c -o canerrsim
gcc canerrdump.c -o canerrdump

# Set execute permissions
chmod +x canerrsim canerrdump
```



## Documentation

Start **canerrsim** or **canerrdump** without any parameters to see documentation for their usage.



## Usage Examples

### canerrsim

```
# Simulate bus-off with arbitration loss on vcan0
./canerrsim vcan0 BusOff LostArBit=12

# Generate protocol error with custom data
./canerrsim can0 FrameFormat Data3=AA Data5=FF ShowBits

# Complex error scenario
./canerrsim vcan0 TxTimeout NoAck CanHiShortToGND WarningRX
```

### canerrdump

```
# Monitor all errors on can0
./canerrdump can0

# Filter specific errors on vcan0
./canerrdump vcan0 IgnoreTxTimeout IgnoreCounters

# Show error mask bits on can1
./canerrdump can1 ShowBits
```

### Combined Usage

```
# In terminal 1:
./canerrdump vcan0

# In terminal 2:
./canerrsim vcan0 LostArBit=09 Data4=AA TX BusOff NoAck ShowBits
```



## License

Dual-licensed under:

- [LGPL-2.1-or-later](https://spdx.org/licenses/LGPL-2.1-or-later.html)
- [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause.html)



## History

Originally, **canerrsim** and **canerrdump** were [developed in FreePascal](https://forum.lazarus.freepascal.org/index.php/topic,39858.msg403874.html#msg403874) as part of my SocketCAN wrappers. However, since I frequently use [**can-utils**](https://github.com/linux-can/can-utils) and install them on every CAN-enabled Linux system, I wanted to include these tools in the suite as well. To achieve this, I rewrote both tools in C and [submitted a patch](https://github.com/linux-can/can-utils/issues/525). While only **canerrsim** was accepted into the official repository, I decided to keep both tools here because **canerrdump** offers a more human-friendly output compared to **candump**.
