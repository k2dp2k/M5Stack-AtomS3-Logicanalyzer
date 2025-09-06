# 🔬 ESP32 AtomS3 Logic Analyzer

> **Modern high-performance logic analyzer with beautiful Gemini-style UI**

[![PlatformIO CI](https://img.shields.io/badge/PlatformIO-Ready-orange.svg)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Compatible-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![M5Stack](https://img.shields.io/badge/M5Stack-AtomS3-red.svg)](https://docs.m5stack.com/en/core/AtomS3)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A specialized, wireless logic analyzer built **exclusively for the M5Stack AtomS3** with a stunning **Gemini-inspired dark UI**. This single-channel design captures digital signals at up to **10MHz on GPIO1 only**, optimized for maximum performance with real-time visualization on both the device's 0.85" display and a beautiful web interface.

## ✨ Features

### 🎨 **Gemini-Style UI Design**
- **Dark theme** with gradient backgrounds (navy to purple)
- **Glass-morphism effects** with subtle shadows and blur
- **Minimalist layout** optimized for the 128x128 pixel display
- **Purple accent colors** throughout for consistency
- **Smooth animations** and glow effects

### ⚡ **High-Performance Single-Channel Analysis**
- **GPIO1 ONLY** - dedicated single-channel design for maximum performance
- **Up to 10MHz sampling** with microsecond precision (4x faster than multi-channel)
- **16,384 sample buffer** - 4x larger than typical multi-channel designs
- **Real-time triggering** with multiple modes (rising, falling, both, level)
- **Wireless operation** via WiFi connectivity

### 🌐 **Modern Web Interface**
- **Responsive design** with touch-friendly controls
- **Real-time status updates** and live data visualization
- **JSON/CSV export** for analysis in external tools
- **WiFi configuration** with AP fallback mode
- **Serial log viewing** for debugging

### 📱 **Specialized AtomS3 Hardware**
- **M5Stack AtomS3 ONLY** - (24×24×10mm) ultra-compact form factor
- **GPIO1 input** - single-channel optimized for maximum speed
- **0.85" TFT display** (128x128 pixels) with Gemini-style UI
- **Physical button** for start/stop capture
- **USB-C connectivity** for programming and power

## 🛠️ Quick Start

### Prerequisites
- **M5Stack AtomS3** development board
- **PlatformIO** installed (VS Code extension recommended)
- **USB-C cable** for programming

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/k2dp2k/M5Stack-AtomS3-Logicanalyzer.git
   cd M5Stack-AtomS3-Logicanalyzer
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
M5Stack AtomS3 - Single Channel Logic Analyzer:
┌───────────────────────────────┐
│ 📺 [0.85" Display] 🔘 [Button] │
│                              │
│ GPIO1 ◀── 🔌 SIGNAL INPUT       │  ← Connect your digital signal here
│                              │  ← (3.3V logic levels only)
│ GND   ◀── ⚡ COMMON GROUND        │  ← Connect signal ground
│                              │
│ 💻 USB-C (Power + Programming)  │
└───────────────────────────────┘

🔴 IMPORTANT: Only GPIO1 is used for signal capture!
🟢 This design sacrifices multi-channel for maximum single-channel performance.
```

**Signal Requirements (GPIO1 ONLY):**
- **Voltage:** 3.3V logic levels (0V = LOW, 3.3V = HIGH)
- **Frequency:** DC to 10MHz (single-channel optimized)
- **Input impedance:** High-Z with internal pull-up resistor
- **Connection:** GPIO1 pin + GND (2-wire connection)

### 🌐 Web Interface

**Main Controls:**
- **▶️ Start Capture** - Begin signal acquisition
- **⏹️ Stop Capture** - End current capture session
- **📊 Get Data** - View captured samples in JSON format
- **🗑️ Clear Data** - Reset capture buffer
- **📥 Download** - Export data as JSON or CSV

**Real-time Status:**
- **Capture State** - READY/CAPTURING with color coding
- **Sample Rate** - Current sampling frequency (up to 10MHz)
- **Buffer Usage** - Memory utilization percentage (16,384 samples)
- **GPIO1 State** - Live signal level display (HIGH/LOW with colors)

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
| **Input Channels** | 1 (GPIO1 ONLY) |
| **Pin Configuration** | GPIO1 + GND (2-wire) |
| **Trigger Modes** | Rising, Falling, Both, Level |
| **Display** | 128x128 TFT (0.85") Gemini UI |
| **Connectivity** | WiFi 802.11 b/g/n |
| **Power** | USB-C (5V) or Battery |

## 🎨 Design Philosophy

This project prioritizes **single-channel performance** over multi-channel complexity:

**Why GPIO1 ONLY design?**
- ⚡ **Maximum sampling rate** - 10MHz vs 2-5MHz typical multi-channel
- 💾 **4x Larger buffer** - 16,384 vs 4,096 samples per channel
- 🔋 **Minimal jitter** - Single-channel dedicated processing
- 🎨 **Cleaner UI** - Focused, uncluttered Gemini-style interface
- ⚡ **Optimized performance** - All resources dedicated to one channel
- 🔧 **Simpler hardware** - Just 2 wires (GPIO1 + GND)

**Gemini UI Inspiration:**
- **Dark gradients** for reduced eye strain
- **Purple accents** for modern, professional look
- **Glass-morphism** for depth and elegance
- **Minimal text** for clarity on small display
- **Smooth animations** for premium feel

## 📦 Data Export Formats

### JSON Format (GPIO1 Optimized)
```json
{
  "device": "M5Stack-AtomS3",
  "analyzer_version": "1.0.0",
  "channel_config": {
    "pin": "GPIO1",
    "channels": 1,
    "pull_up": true
  },
  "capture_info": {
    "sample_count": 1024,
    "sample_rate": 10000000,
    "buffer_size": 16384,
    "trigger_mode": "rising_edge",
    "capture_duration_us": 102
  },
  "samples": [
    {
      "timestamp_us": 1,
      "gpio1_state": 1,
      "logic_level": "HIGH"
    },
    {
      "timestamp_us": 2,
      "gpio1_state": 0,
      "logic_level": "LOW"
    }
  ]
}
```

### CSV Format (GPIO1 Single Channel)
```csv
Sample,Timestamp_us,GPIO1_Digital,GPIO1_Logic,Duration_us
1,0,1,HIGH,5
2,5,0,LOW,12
3,17,1,HIGH,8
4,25,0,LOW,3
5,28,1,HIGH,15
```

**CSV Columns:**
- `Sample`: Sequential sample number
- `Timestamp_us`: Time in microseconds from capture start
- `GPIO1_Digital`: Raw digital value (0 or 1)
- `GPIO1_Logic`: Logic level interpretation (HIGH/LOW)
- `Duration_us`: Time duration of this state

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

### Build Environment
- `m5stack-atoms3` - M5Stack AtomS3 (ONLY supported platform)

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

---

**Made with ❤️ for the maker community** 🚀
