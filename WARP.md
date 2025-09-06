# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is an ESP32-based logic analyzer that captures digital signals and provides a web-based interface for control and data visualization. The system uses the ESP32's high-speed GPIO capabilities with WiFi connectivity for wireless operation.

The project supports two hardware configurations:
- **Standard ESP32**: Original development board with 8 channels
- **M5Stack AtomS3**: Compact device with 0.85" display and 6 channels

## Development Commands

### Build and Upload

**For Standard ESP32:**
```bash
# Build the project
pio run --environment esp32dev

# Upload to ESP32 device
pio run --target upload --environment esp32dev

# Upload and monitor
pio run --target upload --environment esp32dev && pio device monitor
```

**For M5Stack AtomS3:**
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
- Handles all signal capture logic using ESP32 GPIO
- Implements circular buffer for sample storage (4096 samples)
- Supports up to 8 channels with configurable sample rates (1kHz - 10MHz)
- Provides multiple trigger modes: rising/falling edge, both edges, high/low level
- Uses microsecond-precision timestamps for accurate timing analysis

**Web Interface** (`src/main.cpp`)
- Embedded HTTP server using ESPAsyncWebServer library
- REST API endpoints for capture control (`/api/start`, `/api/stop`, `/api/data`, `/api/status`)
- Real-time status updates via JavaScript polling
- Single-page application served directly from ESP32

**Signal Processing Pipeline**
1. **Initialization**: Configure GPIO pins as inputs for specified channels
2. **Capture**: High-speed polling loop reads all channels simultaneously
3. **Triggering**: Optional trigger conditions arm/disarm capture based on signal patterns  
4. **Buffering**: Circular buffer stores timestamp + 8-bit channel data
5. **Export**: JSON serialization for web interface consumption

### Key Design Patterns

**Real-time Processing**: Main loop (`analyzer.process()`) runs continuously, checking timing intervals to maintain precise sample rates without blocking WiFi stack.

**Memory Management**: Fixed-size circular buffer avoids dynamic allocation. Buffer automatically stops capture when full to prevent overruns.

**Hardware Abstraction**: Channel pin mapping defined in header constants, easily configurable for different ESP32 board layouts.

## Configuration

### Hardware Setup

**Standard ESP32:**
- Channel pins: GPIO 2, 4, 5, 18, 19, 21, 22, 23 (channels 0-7)
- Input voltage: 3.3V logic levels
- Maximum sample rate: 10MHz
- 8 channels available

**M5Stack AtomS3:**
- Channel pins: GPIO 1, 2, 5, 6, 7, 8 (channels 0-5)
- Input voltage: 3.3V logic levels
- Maximum sample rate: 5MHz (reduced due to display overhead)
- 6 channels available
- 0.85" TFT display (128x128 pixels)
- Physical button for start/stop capture
- Compact form factor (24×24×10mm)

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

### Channel Configuration
- Modify `MAX_CHANNELS` in header to reduce memory usage if fewer channels needed
- Update pin definitions (`CHANNEL_X_PIN`) for custom hardware layouts
- Active channel count configurable at runtime via web interface

## Important Implementation Details

**Timing Critical Code**: The `process()` method must be called frequently from main loop. Avoid long delays or blocking operations in main loop to maintain sample rate accuracy.

**Interrupt Handling**: Current implementation uses polling rather than interrupts for better timing precision and reduced jitter.

**Buffer Overflow**: System automatically stops capture when buffer reaches capacity. Monitor buffer usage via status API to prevent data loss.

**Trigger Logic**: Pre-trigger data not currently supported. Triggering starts capture from trigger point forward. Consider this when analyzing signal timing relationships.

## AtomS3 Display Usage

**Display Features:**
- Real-time status display (READY/CAPTURING)
- Buffer usage indicator
- Current sample rate and channel count
- Live channel state visualization (colored indicators)
- Trigger status and configuration

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
