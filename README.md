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
- **UART monitoring** for communication analysis

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

## 🔌 Usage

### 📡 Configurable External UART Monitoring

The AtomS3 Logic Analyzer features a professional external UART monitoring system that can analyze communication between any two devices with full parameter configuration via web interface.

**Key Features:**
- **Real-time monitoring** of UART RX/TX data
- **Automatic logging** of all serial communication
- **200-entry buffer** for extended monitoring sessions
- **Timestamped entries** for precise timing analysis
- **Text export** for external analysis tools

**How it works:**
1. Configure UART parameters via web interface
2. Connect external device TX → AtomS3 RX (GPIO43)
3. Connect external device RX → AtomS3 TX (GPIO44) 
4. Share common ground between devices
5. Start monitoring to capture all communication
6. View real-time data with statistics and timestamps

**Professional Use Cases:**
- **Protocol Analysis**: Debug I2C, SPI, Modbus over UART
- **Device Communication**: Monitor Arduino, ESP32, STM32 serial communication
- **Sensor Networks**: Analyze sensor data streams and responses
- **Industrial Systems**: Monitor PLC and HMI communication
- **IoT Development**: Debug wireless module AT commands
- **Reverse Engineering**: Document unknown communication protocols

### 🔌 Hardware Setup

```
M5Stack AtomS3 - Logic + UART Communication Analyzer:
┌───────────────────────────────────────────┐
│ 📺 [0.85" Display] 🔘 [Button]          │
│                                        │
│ GPIO1  ◀── 🔌 LOGIC SIGNAL INPUT     │  ← Digital logic analysis (10MHz)
│ GPIO43 ◀── 📡 UART RX (configurable)  │  ← External device TX connection
│ GPIO44  ─▶ 📡 UART TX (configurable)  │  ← External device RX connection
│ GND    ◀── ⚡ COMMON GROUND           │  ← Shared ground for all signals
│                                        │
│ 💻 USB-C (Power + Programming)        │
└───────────────────────────────────────────┘

🔴 DUAL FUNCTIONALITY:
• GPIO1: High-speed logic analysis (single-channel, up to 10MHz)
• GPIO43/44: Configurable external UART monitoring
• All pins configurable via web interface
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

**UART Monitoring & Configuration:**
- **▶️ Start UART** - Begin external UART communication monitoring
- **⏹️ Stop UART** - End UART monitoring session
- **⚙️ Configure** - Open UART parameter configuration panel
- **🗑️ Clear UART** - Clear UART communication logs
- **📥 Download UART** - Export UART logs as text file

**UART Configuration Panel:**
- **Baudrate Selection** - 9600 to 921600 baud dropdown
- **Data Bits** - 7 or 8 bit selection
- **Parity Setting** - None, Odd, or Even parity
- **Stop Bits** - 1 or 2 stop bits
- **GPIO Pin Selection** - Configurable RX/TX pins
- **✅ Apply Configuration** - Save and activate settings

**Real-time Status:**
- **Capture State** - READY/CAPTURING with color coding
- **Sample Rate** - Current sampling frequency (up to 10MHz)
- **Buffer Usage** - Memory utilization percentage (16,384 samples)
- **GPIO1 State** - Live signal level display (HIGH/LOW with colors)
- **UART Status** - Active/Disabled monitoring state
- **UART Configuration** - Current settings display (e.g., "115200 8N1")
- **UART Pins** - Active RX/TX GPIO pins (e.g., "RX:43 TX:44")
- **UART Statistics** - Real-time byte counters (RX/TX separately)

### 📱 Device Display

**Information shown:**
- **GPIO1 ANALYZER** - Main title with status indicators
- **READY/CAPTURING** - Current logic analyzer operation state
- **Rate: X.XMHz** - GPIO1 sampling frequency (up to 10MHz)
- **Buf: XX%** - Logic analyzer buffer usage with purple progress bar
- **WiFi Status** - Connection indicator and network information
- **GPIO1 State** - Live HIGH/LOW status with color coding
- **UART Status** - External UART monitoring state when configured

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
| **Logic Analyzer** | **Specification** |
| **Input Channels** | 1 (GPIO1 ONLY) |
| **Pin Configuration** | GPIO1 + GND (2-wire) |
| **Trigger Modes** | Rising, Falling, Both, Level |
| **UART Monitor** | **Specification** |
| **UART Channels** | External RX/TX (configurable pins) |
| **Default UART Pins** | RX: GPIO43, TX: GPIO44 |
| **Supported Baud Rates** | 9600 - 921600 baud |
| **UART Parameters** | 7/8 data bits, None/Odd/Even parity, 1/2 stop bits |
| **UART Buffer** | 200 entries circular buffer |
| **General** | **Specification** |
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

### JSON Format (GPIO1 Logic + UART Data)
```json
{
  "device": "M5Stack-AtomS3",
  "analyzer_version": "2.0.0",
  "logic_analyzer": {
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
  },
  "uart_monitor": {
    "config": {
      "baudrate": 115200,
      "data_bits": 8,
      "parity": "None",
      "stop_bits": 1,
      "rx_pin": 43,
      "tx_pin": 44
    },
    "statistics": {
      "bytes_received": 2048,
      "bytes_sent": 512,
      "monitoring_enabled": true,
      "last_activity": 1642534567890
    },
    "uart_logs": [
      "1642534567890: [UART RX] Hello World",
      "1642534567891: [UART TX] OK"
    ]
  }
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
