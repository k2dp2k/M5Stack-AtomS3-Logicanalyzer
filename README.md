# 🔬 ESP32 AtomS3 Logic Analyzer

> **Professional-grade dual-mode analyzer with Flash Storage and Gemini-style UI**

[![Version](https://img.shields.io/badge/Version-2.3.0-brightgreen.svg)](https://github.com/k2dp2k/M5Stack-AtomS3-Logicanalyzer)
[![Flash Storage](https://img.shields.io/badge/Flash%20Storage-Enhanced-blue.svg)](https://github.com/k2dp2k/M5Stack-AtomS3-Logicanalyzer)
[![UART Monitor](https://img.shields.io/badge/UART-Professional-yellow.svg)](https://github.com/k2dp2k/M5Stack-AtomS3-Logicanalyzer)
[![PlatformIO CI](https://img.shields.io/badge/PlatformIO-Ready-orange.svg)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Compatible-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![M5Stack](https://img.shields.io/badge/M5Stack-AtomS3-red.svg)](https://docs.m5stack.com/en/core/AtomS3)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A **professional-grade dual-mode analyzer** built exclusively for the M5Stack AtomS3 featuring:

🔋 **High-Speed Logic Analysis** - Up to 10MHz sampling on GPIO1 with 16,384-sample buffer  
💾 **Flash Storage System** - Store up to 200,000+ UART entries with enhanced LittleFS partition
📡 **Professional UART Monitor** - Full-duplex communication analysis with intelligent buffer management  
🌌 **Gemini-Style Interface** - Modern dark UI with glass-morphism effects and real-time controls  
⚡ **Wireless Operation** - Complete WiFi connectivity with web-based control and data export  
📱 **Dual Functionality** - Simultaneous logic analysis and UART monitoring in one compact device

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

### 💾 **Professional Flash Storage System**
- **LittleFS Integration** - Automatic formatting and management
- **100,000+ UART Entries** - Massive capacity for long-term monitoring
- **Smart Buffer Management** - Dynamic RAM/Flash selection based on size
- **Persistent Data** - Survives power cycles and device resets
- **Real-time Migration** - Seamless switching between storage types
- **Interactive Time Estimates** - Shows buffer duration at current baud rates

### 🌐 **Modern Web Interface**
- **Responsive design** with touch-friendly controls
- **Real-time status updates** and live data visualization
- **JSON/CSV export** for analysis in external tools
- **WiFi configuration** with AP fallback mode
- **Serial log viewing** for debugging
- **Advanced UART monitoring** with Flash storage controls

### 📱 **Specialized AtomS3 Hardware with Dual-Page Display**
- **M5Stack AtomS3 ONLY** - (24×24×10mm) ultra-compact form factor
- **ESP32-S3** - 240MHz dual-core with 320KB RAM + 8MB Flash
- **GPIO1 input** - single-channel optimized for maximum speed
- **0.85\" TFT display** (128x128 pixels) with **dual-page Gemini-style UI**
- **Animated startup logo** with AtomS3 branding and version info
- **Page 1: WiFi Information** - Connection status, SSID, IP address, signal strength
- **Page 2: System Information** - CPU usage, RAM status, flash size, uptime
- **Physical button** for **page switching** (replaces capture control)
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

### 📶 WiFi Setup with Auto-Fallback

**First-time setup:**
- Device creates AP: `AtomS3-LogicAnalyzer` (password: `logic123`)
- Connect and go to `http://192.168.4.1/config`
- Enter your WiFi credentials
- Device will restart and connect to your network

**Automatic WiFi Management:**
- **30-second fallback** - Automatically starts AP mode if WiFi is disconnected for 30+ seconds
- **Auto-reconnection** - Continuously attempts to reconnect to saved WiFi network
- **Dual-mode support** - Can run WiFi client and AP simultaneously for maximum accessibility
- **Real-time monitoring** - WiFi status displayed on device screen and web interface

**Web interface access:**
- **WiFi Client mode:** Check **WiFi page** on device display for IP address
- **Access Point mode:** Connect to device AP and go to `http://192.168.4.1`
- **Dual mode:** Access via either IP address

## 📱 Dual-Page Display System

The AtomS3 features a modern **dual-page display system** optimized for the compact 0.85" screen:

### 🎨 **Startup Experience**
- **Animated startup logo** with AtomS3 Logic Analyzer branding
- **Version information** display (v2.2.0)
- **Purple/blue gradient** color scheme with flash effects
- **3-second display** before switching to main interface

### 📶 **Page 1: WiFi Information**
- **Connection status** - Connected/Disconnected with color coding
- **Network name (SSID)** - Current WiFi network or AP name
- **IP address** - Device IP for web interface access
- **Signal strength** - RSSI in dBm with quality indication
- **AP mode indicator** - Shows when device is in fallback AP mode
- **Glass-morphism panel** with purple accent colors

### 💻 **Page 2: System Information**
- **CPU usage** - Estimated based on capture activity (15% idle, 85% capturing)
- **RAM status** - Free heap memory in KB with usage percentage
- **Flash size** - Total flash memory available in MB
- **Uptime** - System runtime in hours and minutes
- **Blue accent colors** to distinguish from WiFi page

### 🔄 **Page Navigation**
- **Physical button** - Press AtomS3 button to switch between pages
- **Page indicators** - "1/2" and "2/2" shown in corner of each page
- **Auto-refresh** - Pages update every second with fresh information
- **No capture control** - Logic analyzer start/stop now only via web interface

### 🎨 **Visual Design**
- **Gemini-style gradients** - Dark navy to purple background
- **Glass-morphism effects** - Semi-transparent panels with subtle highlights
- **Color-coded status** - Green (good), yellow (warning), red (error)
- **Optimized typography** - Clear, readable text at 128x128 resolution
- **Modern aesthetics** - Consistent with Gemini AI design language

## 🔌 Usage

### 📡 Professional UART Monitoring with Flash Storage

The AtomS3 Logic Analyzer features an enterprise-grade UART monitoring system with intelligent flash storage management, capable of analyzing communication between any two devices for hours with full persistence.

**Advanced Features:**
- **Dual Storage System** - Smart RAM/Flash selection based on buffer size
- **Massive Capacity** - Up to 100,000 UART entries with LittleFS persistence
- **Real-time Estimates** - Dynamic time calculations based on current baud rate
- **Professional Buffer Management** - Automatic compaction and memory optimization
- **Persistent Monitoring** - Data survives device resets and power cycles
- **Interactive Controls** - One-click storage type switching
- **Advanced Configuration** - Full UART parameter control via web interface

**How it works:**
1. Configure UART parameters via web interface
2. Connect external device TX → AtomS3 RX (GPIO7)
3. Connect external device RX → AtomS3 TX (disabled by default)
4. Share common ground between devices
5. Start monitoring to capture all communication
6. View real-time data with statistics and timestamps
7. Switch between RAM/Flash storage as needed
8. Export comprehensive logs with metadata

**Professional Use Cases:**
- **Protocol Analysis**: Debug I2C, SPI, Modbus over UART
- **Long-term Monitoring**: 3.5+ hour sessions with Flash storage
- **Device Communication**: Monitor Arduino, ESP32, STM32 serial communication
- **Sensor Networks**: Analyze sensor data streams and responses
- **Industrial Systems**: Monitor PLC and HMI communication with persistence
- **IoT Development**: Debug wireless module AT commands
- **Reverse Engineering**: Document unknown communication protocols

### 🔌 Hardware Setup

```
M5Stack AtomS3 - Logic + UART Communication Analyzer:
┌───────────────────────────────────────────┐
│ 📺 [0.85" Display] 🔘 [Button]          │
│                                        │
│ GPIO1  ◀── 🔌 LOGIC SIGNAL INPUT     │  ← Digital logic analysis (10MHz)
│ GPIO7  ◀── 📡 UART RX (configurable)  │  ← External device TX connection
│ GPIO44  ─▶ 📡 UART TX (configurable)  │  ← External device RX connection
│ GND    ◀── ⚡ COMMON GROUND           │  ← Shared ground for all signals
│                                        │
│ 💻 USB-C (Power + Programming)        │
└───────────────────────────────────────────┘

🔴 DUAL FUNCTIONALITY:
• GPIO1: High-speed logic analysis (single-channel, up to 10MHz)
• GPIO7: Configurable external UART monitoring with Flash storage
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

**UART Monitoring & Flash Storage:**
- **▶️ Start UART** - Begin external UART communication monitoring
- **⏹️ Stop UART** - End UART monitoring session
- **⚙️ Configure** - Open advanced UART parameter configuration
- **💾 Flash Toggle** - Switch between RAM and Flash storage modes
- **🗑️ Clear UART** - Clear UART logs (RAM or Flash)
- **📋 Compact Buffer** - Optimize Flash storage usage
- **📥 Download UART** - Export comprehensive logs with metadata

**Advanced UART Configuration:**
- **Baudrate Selection** - 9600 to 921600 baud with real-time estimates
- **Buffer Size Management** - 1K to 100K entries with time calculations
- **Storage Type Selection** - Automatic or manual RAM/Flash switching
- **Data Bits** - 7 or 8 bit selection
- **Parity Setting** - None, Odd, or Even parity
- **Stop Bits** - 1 or 2 stop bits
- **GPIO Pin Selection** - Configurable RX/TX pins
- **Interactive Time Estimates** - Shows buffer duration at current settings
- **✅ Apply Configuration** - Save and activate all settings
- **💾 Set Buffer Size** - Configure storage with automatic type selection

**Real-time Status:**
- **Capture State** - READY/CAPTURING with color coding
- **Sample Rate** - Current sampling frequency (up to 10MHz)
- **Buffer Usage** - Memory utilization percentage (16,384 samples)
- **GPIO1 State** - Live signal level display (HIGH/LOW with colors)
- **UART Status** - Active/Disabled monitoring state
- **UART Configuration** - Current settings display (e.g., "115200 8N1")
- **UART Pins** - Active RX/TX GPIO pins (e.g., "RX:7 TX:disabled")
- **UART Statistics** - Real-time byte counters (RX/TX separately)
- **Storage Type** - Current storage mode (RAM/Flash) with toggle button

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

## 🚀 **Flash Storage Performance**

### 📊 **Storage Capacity Comparison**

| Buffer Size | Storage Type | Duration @ 115200 | Memory Usage | Use Case |
|-------------|--------------|-------------------|--------------|----------|
| 1,000 entries | **RAM** | ~2 minutes | ~50KB | Quick tests, debugging |
| 5,000 entries | **RAM** | ~10 minutes | ~250KB | Development sessions |
| 10,000 entries | **Flash** | ~21 minutes | ~500KB | Extended monitoring |
| 25,000 entries | **Flash** | ~52 minutes | ~1.25MB | Professional analysis |
| 50,000 entries | **Flash** | **~1.7 hours** | ~2.5MB | Long-term monitoring |
| 100,000 entries | **Flash** | **~3.5 hours** | ~5MB | Enterprise logging |

### ⚡ **Performance Characteristics**

```
Operation          | RAM      | Flash (LittleFS)
-------------------|----------|------------------
Write Speed        | ~1μs     | ~1ms
Read Speed         | ~1μs     | ~100μs
Max Capacity       | ~5K      | 200K+ entries
Persistence        | ❌       | ✅ Survives reboot
Wear Leveling      | N/A      | ✅ Automatic
Data Migration     | N/A      | ✅ Seamless
```

### 🎯 **Intelligent Storage Selection**

- **< 5,000 entries**: Automatic **RAM** selection for speed
- **> 5,000 entries**: Automatic **Flash** selection for capacity
- **Manual override**: One-click toggle between storage types
- **Real-time migration**: Seamless data transfer between RAM/Flash
- **Interactive estimates**: Live calculation of buffer duration

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

### Flash Storage Configuration
```cpp
#define MAX_UART_ENTRIES 100000   // Maximum Flash entries
#define UART_MSG_MAX_LENGTH 500   // Max message size
// Automatic selection: <5K = RAM, >5K = Flash
```

### WiFi Settings
- **AP SSID Format:** `AtomS3-Logic-XXXXXX` (XXXXXX = MAC suffix)
- **AP Password:** `logicanalyzer` (default)
- **Web Server Port:** 80 (HTTP)
- **Connection Timeout:** 15 seconds

## 📊 Technical Specifications

| Specification | Value |
|---------------|-------|
| **Hardware** | **Specification** |
| **MCU** | ESP32-S3 (240MHz dual-core) |
| **RAM Usage** | ~179KB (54.6% of 320KB) |
|| **Flash Usage** | ~1.04MB (33.2% of 3MB app partition) |
| **Sample Buffer** | 16,384 samples |
| **Max Sample Rate** | 10MHz (GPIO1 optimized) |
| **Timing Precision** | 1μs resolution |
| **Flash Storage System** | **Specification** |
| **LittleFS Partition** | ~1.4MB available for logs |
| **Max UART Entries** | 100,000+ entries |
| **Storage Types** | RAM (fast) + Flash (persistent) |
| **Auto-Selection** | <5K entries=RAM, >5K=Flash |
| **Logic Analyzer** | **Specification** |
| **Input Channels** | 1 (GPIO1 ONLY) |
| **Pin Configuration** | GPIO1 + GND (2-wire) |
| **Trigger Modes** | Rising, Falling, Both, Level |
| **UART Monitor** | **Specification** |
| **UART Channels** | External RX/TX (configurable pins) |
| **Default UART Pins** | RX: GPIO7, TX: disabled (-1) |
| **Supported Baud Rates** | 9600 - 921600 baud |
| **UART Parameters** | 7/8 data bits, None/Odd/Even parity, 1/2 stop bits |
| **Buffer Capacity** | 1K - 100K entries (configurable) |
| **Session Duration** | Up to 3.5+ hours @ 115200 baud |
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

**Flash Storage Benefits:**
- 💾 **Massive capacity** - 20x more data than RAM-only systems
- 🔒 **Data persistence** - Survives power cycles and crashes
- ⚡ **Smart selection** - Automatic RAM/Flash switching
- 📊 **Professional monitoring** - Hours-long analysis sessions
- 🔄 **Seamless migration** - Real-time data transfer between storage types

**Gemini UI Inspiration:**
- **Dark gradients** for reduced eye strain
- **Purple accents** for modern, professional look
- **Glass-morphism** for depth and elegance
- **Minimal text** for clarity on small display
- **Smooth animations** for premium feel

## 📦 Data Export Formats

### JSON Format (Enhanced with Flash Storage)
```json
{
  "device": "M5Stack-AtomS3",
  "analyzer_version": "2.1.0",
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
      }
    ]
  },
  "uart_monitor": {
    "config": {
      "baudrate": 115200,
      "data_bits": 8,
      "parity": "None",
      "stop_bits": 1,
      "rx_pin": 7,
      "tx_pin": -1
    },
    "storage_info": {
      "storage_type": "Flash",
      "flash_file": "/uart_logs_12345.txt",
      "buffer_capacity": 100000,
      "estimated_duration": "3.5 hours"
    },
    "statistics": {
      "bytes_received": 2048,
      "bytes_sent": 512,
      "monitoring_enabled": true,
      "last_activity": 1642534567890,
      "memory_usage": 5242880
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

## 🔧 Development

### Project Structure
```
esp32-logicanalyzer/
├── include/
│   └── logic_analyzer.h      # Core analyzer class with Flash support
├── src/
│   ├── main.cpp              # Web server & WiFi with Flash API
│   └── logic_analyzer.cpp    # Signal capture + Flash storage logic
├── platformio.ini            # Build configuration with LittleFS
├── WARP.md                   # AI assistant guidance (updated)
├── FLASH_STORAGE_NOTES.md    # Flash implementation documentation
└── README.md                 # This file (updated)
```

### Build Environment
- `m5stack-atoms3` - M5Stack AtomS3 (ONLY supported platform)

### Key Libraries
- **M5AtomS3** - Hardware abstraction
- **M5GFX** - Display graphics
- **ESPAsyncWebServer** - HTTP server
- **ArduinoJson** - Data serialization
- **WiFi** - Network connectivity
- **LittleFS** - Flash file system for persistent storage

## 🕰️ Performance Tips

**For maximum sampling rate:**
1. Use **GPIO1 only** (this design)
2. Minimize **WiFi activity** during capture
3. Use **appropriate buffer size** for your signals
4. Consider **trigger modes** to capture events of interest
5. Monitor **buffer usage** to avoid overruns

**For optimal Flash storage:**
- Use **Flash storage** for sessions >10 minutes
- Enable **automatic storage selection** for best performance
- Use **compact buffer** function to optimize Flash usage
- Consider **RAM storage** for frequent, short captures

**For best WiFi performance:**
- Place device **close to router** during capture
- Use **5GHz WiFi** if available for less congestion
- Avoid **simultaneous web browsing** during capture
- Consider **offline capture** then connect for data download

## 🐛 Troubleshooting

**Flash Storage issues?**
- Check LittleFS formatting in serial monitor
- Look for "LittleFS formatted successfully" message
- Try power cycling the device
- Use "Clear Flash" button to reset storage

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

**UART monitoring problems?**
- Verify correct pin configuration (default: RX=GPIO7, TX=disabled)
- Check baud rate settings match your device
- Ensure proper ground connections
- Try different buffer sizes
- Check storage type selection

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
- Flash storage optimizations

## 👥 Acknowledgments

- **M5Stack** for the excellent AtomS3 hardware
- **Espressif** for the powerful ESP32-S3 chip and LittleFS
- **Google** for Gemini UI design inspiration
- **PlatformIO** for the amazing development environment
- **Open source community** for the fantastic libraries

---

**Made with ❤️ for the maker community** 🚀

**🆕 Version 2.3.0 Features:**
- 🎨 **Modern Animated Logo** - Blue-purple gradient with glass-morphism effects
- 🔧 **Fixed Display Flickering** - Optimized refresh rates and redraw logic  
- 💾 **Enhanced Flash Storage** - Improved storage reliability and performance
- 🖱️ **Better UI Controls** - Renamed "Compact Buffer" to "Copy Data" with clipboard function
- ⚙️ **UART Button Fixes** - Toggle button appearance now updates correctly
- 📊 **Stable Storage System** - Up to 200,000+ UART entries with reliable persistence
