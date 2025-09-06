# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is a specialized M5Stack AtomS3 logic analyzer that captures digital signals on GPIO1 only and provides a web-based interface for control and data visualization. The system uses the ESP32-S3's high-speed GPIO capabilities with WiFi connectivity for wireless operation.

This implementation is specifically designed for:
- **M5Stack AtomS3 ONLY**: Compact device with 0.85" display optimized for dual-functionality
- **GPIO1 exclusive**: Single-channel logic analysis for maximum performance (up to 10MHz sampling)
- **External UART monitoring**: Configurable external device communication analysis
- **Gemini-style UI**: Modern dark interface with glass-morphism effects and professional configuration

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
- Implements dual functionality: GPIO1 logic analysis + external UART monitoring
- GPIO1: 16,384 sample buffer with configurable sample rates (1kHz - 10MHz)
- UART: 200-entry circular buffer with full parameter configuration
- Multiple trigger modes: rising/falling edge, both edges, high/low level
- External UART support with configurable pins, baud rates, and parameters
- Uses microsecond-precision timestamps for accurate timing analysis
- Optimized for maximum single-channel logic performance + professional UART monitoring

**Web Interface** (`src/main.cpp`)
- Embedded HTTP server using ESPAsyncWebServer library
- REST API endpoints for capture control (`/api/start`, `/api/stop`, `/api/data`, `/api/status`)
- Real-time status updates via JavaScript polling
- Single-page application served directly from ESP32

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

**External UART Monitor:**
- **Default pins: GPIO43 (RX), GPIO44 (TX)** - configurable via web interface
- **Supported baud rates:** 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
- **Data bits:** 7 or 8 bits configurable
- **Parity:** None, Odd, or Even
- **Stop bits:** 1 or 2 bits
- **Buffer size:** 200 UART communication entries
- **Pin configuration:** Fully customizable through web GUI

**General Hardware:**
- 0.85" TFT display (128x128 pixels) with Gemini-style UI
- Physical button for start/stop capture
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

## Important Implementation Details

**Timing Critical Code**: The `process()` method must be called frequently from main loop. Avoid long delays or blocking operations in main loop to maintain sample rate accuracy.

**Interrupt Handling**: Current implementation uses polling rather than interrupts for better timing precision and reduced jitter.

**Buffer Overflow**: System automatically stops capture when buffer reaches capacity. Monitor buffer usage via status API to prevent data loss.

**Trigger Logic**: Pre-trigger data not currently supported. Triggering starts capture from trigger point forward. Consider this when analyzing signal timing relationships.

## AtomS3 Display Usage

**Display Features:**
- Real-time status display (READY/CAPTURING) with Gemini-style UI
- Buffer usage indicator with purple progress bar for GPIO1 analysis
- Current sample rate display (up to 10MHz)
- Live GPIO1 state visualization (HIGH/LOW with color coding)
- UART monitoring status when external communication is active
- WiFi connection status with animated indicators
- Modern dark theme with glass-morphism effects
- Dual-mode operation indicator (Logic + UART)

**Physical Controls:**
- Button A: Toggle capture start/stop
- Status updates every 500ms
- Color-coded states: Green (active/high), Red (inactive/low), Orange (waiting)

## Library Dependencies

**Common Libraries:**
- **ESPAsyncWebServer**: Handles HTTP server and WebSocket functionality
- **AsyncTCP**: TCP connection management for web server
- **ArduinoJson**: JSON serialization/deserialization for API responses
- **WiFi**: ESP32 built-in WiFi connectivity

**AtomS3 Additional Libraries:**
- **M5AtomS3**: Core AtomS3 hardware support
- **M5GFX**: Graphics library for display functionality
- **M5Unified**: Unified M5Stack device support

Install missing dependencies:
```bash
pio lib install "me-no-dev/ESPAsyncWebServer"
pio lib install "me-no-dev/AsyncTCP" 
pio lib install "bblanchon/ArduinoJson"
```
