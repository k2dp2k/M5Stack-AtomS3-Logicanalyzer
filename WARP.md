# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is a specialized M5Stack AtomS3 logic analyzer that captures digital signals on GPIO1 only and provides a web-based interface for control and data visualization. The system uses the ESP32-S3's high-speed GPIO capabilities with WiFi connectivity for wireless operation.

This implementation is specifically designed for:
- **M5Stack AtomS3 ONLY**: Compact device with 0.85" display optimized for dual-functionality
- **GPIO1 exclusive**: Single-channel logic analysis for maximum performance (up to 10MHz sampling)
- **Professional UART monitoring**: LittleFS Flash storage with 100K+ entry capacity
- **Intelligent Storage Management**: Dynamic RAM/Flash selection with real-time migration
- **Gemini-style UI**: Modern dark interface with glass-morphism effects and Flash storage controls

## Development Commands

### Build and Upload

**For M5Stack AtomS3 (ONLY supported platform):**
```bash
# Build for AtomS3
pio run --environment m5stack-atoms3

# Upload to AtomS3 device
pio run --target upload --environment m5stack-atoms3

# Upload and monitor
pio run --target upload --environment m5stack-atoms3 && pio device monitor
```

### Development Workflow
```bash
# Clean build (useful after major changes)
pio run --target clean

# Check code without building
pio check

# List available devices/ports
pio device list

# Format code (if formatter is configured)
pio run --target format
```

### Testing and Analysis
```bash
# Run unit tests (if test/ directory exists)
pio test

# Build with verbose output for debugging
pio run --verbose

# Check library dependencies
pio lib list
```

## Core Architecture

### Main Components

**LogicAnalyzer Class** (`src/logic_analyzer.cpp`, `include/logic_analyzer.h`)
- Handles all signal capture logic using ESP32-S3 GPIO1 exclusively
- Implements dual functionality: GPIO1 logic analysis + external UART monitoring with Flash storage
- GPIO1: 16,384 sample buffer with flash storage default and configurable sample rates (1kHz - 10MHz)
- UART: Flash storage by default with intelligent dual-storage system (RAM: fast, Flash: persistent)
- Flash Storage: LittleFS integration with up to 100,000 UART entries
- Automatic storage selection: <5K entries=RAM, >5K entries=Flash
- Real-time storage migration with seamless data transfer
- Interactive buffer time estimates based on current baud rate
- Multiple trigger modes: rising/falling edge, both edges, high/low level
- External UART support with configurable pins, baud rates, and parameters
- Uses microsecond-precision timestamps for accurate timing analysis
- Optimized for maximum single-channel logic performance + professional UART monitoring

**Web Interface** (`src/main.cpp`)
- Embedded HTTP server using ESPAsyncWebServer library
- REST API endpoints for capture control (`/api/start`, `/api/stop`, `/api/data`, `/api/status`)
- Real-time status updates via JavaScript polling
- Single-page application served directly from ESP32 with 'Logic Data' section for captured samples

**Signal Processing Pipeline**
1. **Initialization**: Configure GPIO1 as input with pull-up resistor
2. **Capture**: High-speed polling loop reads GPIO1 state at precise intervals
3. **Triggering**: Optional trigger conditions arm/disarm capture based on GPIO1 signal patterns  
4. **Buffering**: Circular buffer stores timestamp + GPIO1 state data
5. **Export**: JSON serialization for web interface consumption with GPIO1-specific formatting

### Key Design Patterns

**Real-time Processing**: Main loop (`analyzer.process()`) runs continuously, checking timing intervals to maintain precise sample rates without blocking WiFi stack.

**Memory Management**: Fixed-size circular buffer avoids dynamic allocation. Buffer automatically stops capture when full to prevent overruns.

**Hardware Abstraction**: Channel pin mapping defined in header constants, easily configurable for different ESP32 board layouts.

## Configuration

### Hardware Setup

**M5Stack AtomS3 (ONLY supported platform):**

**Logic Analyzer (GPIO1):**
- **Signal input pin: GPIO1 ONLY** - optimized for maximum performance
- Input voltage: 3.3V logic levels (0V = LOW, 3.3V = HIGH)
- Maximum sample rate: 10MHz (single-channel optimization)
- Buffer size: 16,384 samples (4x larger than multi-channel designs)
- Internal pull-up resistor on GPIO1

**External UART Monitor with Flash Storage:**
- **Default pins: GPIO7 (RX), TX disabled (-1)** - configurable via web interface
- **Supported baud rates:** 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
- **Data bits:** 7 or 8 bits configurable
- **Parity:** None, Odd, or Even
- **Stop bits:** 1 or 2 bits
- **Duplex modes:** Full-Duplex (RX+TX) and Half-Duplex (single bidirectional pin)
- **Buffer capacity:** 1,000 - 100,000 entries (configurable)
- **Storage types:** RAM (fast, <5K entries) + Flash (persistent, >5K entries)
- **Session duration:** Up to 3.5+ hours with Flash storage
- **Real-time estimates:** Dynamic time calculations based on baud rate
- **Pin configuration:** Fully customizable through web GUI with storage controls
- **Half-Duplex Commands:** Interactive command interface for bidirectional communication

**General Hardware:**
- 0.85" TFT display (128x128 pixels) with dual-page Gemini-style UI
- Physical button for page switching (WiFi info ↔ System info)
- Compact form factor (24×24×10mm)
- USB-C for power and programming

### WiFi Configuration
Update credentials in `src/main.cpp`:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### Sample Rate Tuning
- Default: 1MHz (1,000,000 Hz)
- Range: 1kHz to 10MHz
- Higher rates may require shorter capture windows due to buffer size limits
- Consider ESP32 processing overhead when setting rates above 5MHz

### GPIO1 Configuration
- Fixed to GPIO1 only - no multi-channel support in this version
- Internal pull-up resistor automatically enabled
- High-impedance input suitable for most 3.3V logic signals
- Optimized pin selection for minimum jitter on AtomS3 hardware

### Half-Duplex UART Communication

**Half-Duplex Mode Features:**
- **Single bidirectional pin**: Uses RX pin for both receiving and transmitting
- **Automatic pin switching**: Hardware automatically switches between RX and TX modes
- **Command interface**: Interactive web-based command sending with automatic line endings
- **Real-time monitoring**: Seamless switching between command transmission and response monitoring
- **Queue system**: Commands are queued and sent sequentially to prevent collisions
- **Timeout handling**: Automatic switch back to RX mode after transmission timeout (100ms)
- **Status indication**: Real-time status shows current mode (RX/TX) and queue status

**Usage Scenarios:**
- **Single-wire protocols**: RS-485, ModBus RTU, proprietary single-wire communication
- **Sensor interrogation**: Send commands to sensors and log responses
- **Device configuration**: Configure remote devices through single-pin interfaces
- **Protocol debugging**: Interactive testing of bidirectional protocols

**Web Interface Controls:**
- **Duplex Mode Selection**: Choose between Full-Duplex and Half-Duplex in UART configuration
- **Command Interface**: Text input field appears automatically when Half-Duplex mode is selected
- **Send Commands**: Commands are sent with automatic \r\n line endings
- **Status Display**: Shows transmission status, queue length, and current mode
- **Response Logging**: All responses are automatically captured and timestamped

**API Endpoints:**
- `POST /api/uart/send`: Send half-duplex commands programmatically
- `GET /api/uart/half-duplex-status`: Get current half-duplex status and queue information

## Important Implementation Details

**Timing Critical Code**: The `process()` method must be called frequently from main loop. Avoid long delays or blocking operations in main loop to maintain sample rate accuracy.

**Interrupt Handling**: Current implementation uses polling rather than interrupts for better timing precision and reduced jitter.

**Buffer Overflow**: System automatically stops capture when buffer reaches capacity. Monitor buffer usage via status API to prevent data loss.

**Trigger Logic**: Pre-trigger data not currently supported. Triggering starts capture from trigger point forward. Consider this when analyzing signal timing relationships.

**Half-Duplex Communication**: The system automatically manages bidirectional communication on a single pin. TX mode is time-limited (100ms timeout) to ensure the system returns to RX mode for response monitoring. Commands are queued and processed sequentially to prevent transmission collisions.

## AtomS3 Display Usage

**Display Features:**
- **Dual-page system** with animated startup logo
- **Page 1 (WiFi)**: Connection status, SSID, IP address, signal strength, AP mode indicator
- **Page 2 (System)**: CPU usage estimate, RAM status, flash size, uptime
- **Glass-morphism panels** with Gemini-style gradients (navy to purple)
- **Page switching** via physical button with indicators (1/2, 2/2)
- **Auto-refresh** every second with smooth transitions
- **Color-coded status**: Connection quality, system health, resource usage
- **Modern dark theme** optimized for 0.85" display

**Physical Controls:**
- Button A: Switch between display pages (WiFi ↔ System information)
- Display updates every 1000ms for optimal performance
- Color-coded states: Green (active/high), Blue (info), Purple (accent), Red (inactive/error)
- Page indicators: "1/2" (WiFi page) and "2/2" (System page)

## Library Dependencies

**Common Libraries:**
- **ESPAsyncWebServer**: Handles HTTP server and WebSocket functionality
- **AsyncTCP**: TCP connection management for web server
- **ArduinoJson**: JSON serialization/deserialization for API responses
- **WiFi**: ESP32 built-in WiFi connectivity
- **LittleFS**: Flash file system for persistent UART log storage

**AtomS3 Additional Libraries:**
- **M5AtomS3**: Core AtomS3 hardware support
- **M5GFX**: Graphics library for display functionality
- **M5Unified**: Unified M5Stack device support
- **FastLED**: LED control library for status indicators

Install missing dependencies:
```bash
pio lib install "me-no-dev/ESPAsyncWebServer"
pio lib install "me-no-dev/AsyncTCP" 
pio lib install "bblanchon/ArduinoJson"
```
