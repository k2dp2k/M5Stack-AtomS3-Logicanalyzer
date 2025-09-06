# 🔬 ESP32 AtomS3 Logic Analyzer

> **Modern high-performance logic analyzer with beautiful Gemini-style UI**

[![PlatformIO CI](https://img.shields.io/badge/PlatformIO-Ready-orange.svg)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Compatible-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![M5Stack](https://img.shields.io/badge/M5Stack-AtomS3-red.svg)](https://docs.m5stack.com/en/core/AtomS3)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A sleek, wireless logic analyzer built for the **M5Stack AtomS3** with a stunning **Gemini-inspired dark UI**. Capture digital signals at up to **10MHz** on GPIO1 with real-time visualization on both the device's 0.85" display and a beautiful web interface.

## ✨ Features

### 🎨 **Gemini-Style UI Design**
- **Dark theme** with gradient backgrounds (navy to purple)
- **Glass-morphism effects** with subtle shadows and blur
- **Minimalist layout** optimized for the 128x128 pixel display
- **Purple accent colors** throughout for consistency
- **Smooth animations** and glow effects

### ⚡ **High-Performance Analysis**
- **Single GPIO1 channel** optimized for maximum sampling rate
- **Up to 10MHz sampling** with microsecond precision
- **16,384 sample buffer** for extended capture sessions
- **Real-time triggering** with multiple modes
- **Wireless operation** via WiFi connectivity

### 🌐 **Modern Web Interface**
- **Responsive design** with touch-friendly controls
- **Real-time status updates** and live data visualization
- **JSON/CSV export** for analysis in external tools
- **WiFi configuration** with AP fallback mode
- **Serial log viewing** for debugging

### 📱 **Compact Hardware**
- **M5Stack AtomS3** (24×24×10mm) ultra-compact form factor
- **0.85" TFT display** (128x128 pixels) with crisp visuals
- **Physical button** for start/stop capture
- **USB-C connectivity** for programming and power
- **WiFi connectivity** for wireless operation

## 🛠️ Quick Start

### Prerequisites
- **M5Stack AtomS3** development board
- **PlatformIO** installed (VS Code extension recommended)
- **USB-C cable** for programming

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/YOUR_USERNAME/esp32-logicanalyzer.git
   cd esp32-logicanalyzer
   ```

2. **Build and upload** (M5Stack AtomS3)
   ```bash
   pio run --environment m5stack-atoms3
   pio run --target upload --environment m5stack-atoms3
   ```

3. **Monitor the device**
   ```bash
   pio device monitor --environment m5stack-atoms3
   ```

### 📶 WiFi Setup

**First-time setup:**
- Device creates AP: `AtomS3-Logic-XXXXXX` (password: `logicanalyzer`)
- Connect and go to `http://192.168.4.1/config`
- Enter your WiFi credentials
- Device will restart and connect to your network

**Web interface access:**
- Connected mode: Check device display for IP address
- Open browser to `http://[DEVICE_IP]`

## 📊 Usage

### 🔌 Hardware Setup

```
M5Stack AtomS3 Pinout:
┌───────────────────┐
│  [Display] [Button]  │
│                    │
│ GPIO1 ◀─── Signal In │  ← Connect your signal here
│                    │
│ GND   ◀─── Ground    │  ← Common ground
└───────────────────┘
```

**Signal Requirements:**
- **Voltage:** 3.3V logic levels (0V = LOW, 3.3V = HIGH)
- **Frequency:** DC to 10MHz
- **Input impedance:** High-Z with internal pull-up

### 🌐 Web Interface

**Main Controls:**
- **▶️ Start Capture** - Begin signal acquisition
- **⏹️ Stop Capture** - End current capture session
- **📊 Get Data** - View captured samples in JSON format
- **🗑️ Clear Data** - Reset capture buffer
- **📥 Download** - Export data as JSON or CSV

**Real-time Status:**
- **Capture State** - READY/CAPTURING with color coding
- **Sample Rate** - Current sampling frequency
- **Buffer Usage** - Memory utilization percentage
- **GPIO1 State** - Live signal level (HIGH/LOW)

### 📱 Device Display

**Information shown:**
- **GPIO1 ANALYZER** - Main title with status indicators
- **READY/CAPTURING** - Current operation state
- **Rate: X.XMHz** - Sampling frequency
- **Buf: XX%** - Buffer usage with purple progress bar
- **WiFi/GPIO1** - Connection and signal status cards

**Physical Controls:**
- **Button press** - Start/stop capture toggle
- **Status LEDs** - Visual feedback for operation state

## 🛠️ Configuration

### Sample Rate Settings
```cpp
// Available sample rates
#define MIN_SAMPLE_RATE 1000         // 1kHz minimum
#define MAX_SAMPLE_RATE 10000000     // 10MHz maximum  
#define DEFAULT_SAMPLE_RATE 1000000  // 1MHz default
```

### Buffer Configuration
```cpp
#define BUFFER_SIZE 16384  // 16K samples for GPIO1
#define MAX_CHANNELS 1     // Single channel optimization
```

### WiFi Settings
- **AP SSID Format:** `AtomS3-Logic-XXXXXX` (XXXXXX = MAC suffix)
- **AP Password:** `logicanalyzer` (default)
- **Web Server Port:** 80 (HTTP)
- **Connection Timeout:** 15 seconds

## 📊 Technical Specifications

| Specification | Value |
|---------------|-------|
| **MCU** | ESP32-S3 (240MHz dual-core) |
| **RAM Usage** | ~178KB (54% of 320KB) |
| **Flash Usage** | ~950KB (30% of 3MB) |
| **Sample Buffer** | 16,384 samples |
| **Max Sample Rate** | 10MHz (GPIO1 optimized) |
| **Timing Precision** | 1μs resolution |
| **Input Channels** | 1 (GPIO1) |
| **Trigger Modes** | Rising, Falling, Both, Level |
| **Display** | 128x128 TFT (0.85") |
| **Connectivity** | WiFi 802.11 b/g/n |
| **Power** | USB-C (5V) or Battery |

## 🎨 Design Philosophy

This project prioritizes **single-channel performance** over multi-channel complexity:

**Why GPIO1 only?**
- ⚡ **Maximum sampling rate** - 10MHz vs 5MHz multi-channel
- 💾 **Larger buffer** - 16K vs 4K samples per channel
- 🔋 **Lower jitter** - Dedicated processing power
- 🎨 **Cleaner UI** - Focused, uncluttered interface
- ⚡ **Better performance** - Optimized memory usage

**Gemini UI Inspiration:**
- **Dark gradients** for reduced eye strain
- **Purple accents** for modern, professional look
- **Glass-morphism** for depth and elegance
- **Minimal text** for clarity on small display
- **Smooth animations** for premium feel

## 📦 Data Export Formats

### JSON Format
```json
{
  "sample_count": 1024,
  "sample_rate": 1000000,
  "gpio_pin": 1,
  "buffer_size": 16384,
  "trigger_mode": 0,
  "samples": [
    {
      "timestamp": 1000,
      "gpio1": true,
      "state": "HIGH"
    }
  ]
}
```

### CSV Format
```csv
Sample,Timestamp_us,GPIO1_Digital,GPIO1_State
1,1000,1,HIGH
2,2000,0,LOW
3,3000,1,HIGH
```

## 🔧 Development

### Project Structure
```
esp32-logicanalyzer/
├── include/
│   └── logic_analyzer.h      # Core analyzer class
├── src/
│   ├── main.cpp              # Web server & WiFi
│   └── logic_analyzer.cpp    # Signal capture logic
├── platformio.ini          # Build configuration
├── WARP.md                 # AI assistant guidance
└── README.md               # This file
```

### Build Environments
- `m5stack-atoms3` - M5Stack AtomS3 (recommended)
- `esp32dev` - Generic ESP32 (compatibility mode)

### Key Libraries
- **M5AtomS3** - Hardware abstraction
- **M5GFX** - Display graphics
- **ESPAsyncWebServer** - HTTP server
- **ArduinoJson** - Data serialization
- **WiFi** - Network connectivity

## 🕰️ Performance Tips

**For maximum sampling rate:**
1. Use **GPIO1 only** (this design)
2. Minimize **WiFi activity** during capture
3. Use **appropriate buffer size** for your signals
4. Consider **trigger modes** to capture events of interest
5. Monitor **buffer usage** to avoid overruns

**For best WiFi performance:**
- Place device **close to router** during capture
- Use **5GHz WiFi** if available for less congestion
- Avoid **simultaneous web browsing** during capture
- Consider **offline capture** then connect for data download

## 🐛 Troubleshooting

**Device not connecting to WiFi?**
- Check SSID/password in configuration
- Verify 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Try AP mode: `AtomS3-Logic-XXXXXX`
- Check router compatibility (WPA2)

**Capture not working?**
- Verify signal voltage (3.3V logic)
- Check GPIO1 connection and ground
- Ensure sample rate is appropriate
- Try different trigger modes
- Monitor buffer usage

**Display issues?**
- Check M5AtomS3 libraries are installed
- Verify correct board selection in platformio.ini
- Try different USB cable/port
- Reset device and reflash firmware

**Web interface problems?**
- Clear browser cache
- Try different browser (Chrome recommended)
- Check device IP address on display
- Verify firewall settings
- Try incognito/private browsing mode

## 📜 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🚀 Contributing

Contributions welcome! Please feel free to submit pull requests or open issues.

**Areas for improvement:**
- Additional trigger modes
- Protocol decoders (I2C, SPI, UART)
- Signal analysis features
- Mobile app companion
- Hardware designs for breakout boards

## 👥 Acknowledgments

- **M5Stack** for the excellent AtomS3 hardware
- **Espressif** for the powerful ESP32-S3 chip
- **Google** for Gemini UI design inspiration
- **PlatformIO** for the amazing development environment
- **Open source community** for the fantastic libraries

---

**Made with ❤️ for the maker community** 🚀

A logic analyzer implementation using the ESP32 microcontroller platform.

## Overview

This project implements a logic analyzer using the ESP32's high-speed GPIO capabilities and internal memory to capture and analyze digital signals. The ESP32's dual-core architecture and built-in WiFi/Bluetooth connectivity make it an ideal platform for a portable, wireless logic analyzer.

## Features (Planned)

- Multi-channel digital signal capture
- High-speed sampling using ESP32's hardware capabilities
- Real-time signal analysis
- Web-based interface for remote monitoring
- WiFi connectivity for wireless operation
- Data export capabilities
- Trigger mechanisms for signal capture

## Hardware Requirements

- ESP32 development board (ESP32-DevKitC, ESP32-WROOM-32, or similar)
- Input protection circuitry (optional but recommended)
- Level shifters for non-3.3V signals (if needed)

## Software Requirements

- PlatformIO (recommended) or Arduino IDE
- ESP32 board package

## Getting Started

1. Clone this repository
2. Open the project in PlatformIO
3. Build and upload to your ESP32 device

## Project Structure

```
├── src/           # Main source code
├── include/       # Header files
├── lib/           # Local libraries
├── test/          # Unit tests
├── docs/          # Documentation
└── platformio.ini # PlatformIO configuration
```

## License

MIT License - see LICENSE file for details

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting pull requests.
