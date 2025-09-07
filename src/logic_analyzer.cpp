#include "logic_analyzer.h"
#include <WiFi.h>
#include <cmath>
#include <soc/gpio_reg.h>  // For direct GPIO register access
#ifdef ATOMS3_BUILD
    extern M5GFX& display;
// ===== DUAL-MODE MONITORING (UART + LOGIC ON SAME PIN) =====

void LogicAnalyzer::enableDualMode(bool enable) {
    if (enable && isDualModeCompatible()) {
        dualModeActive = true;
        addLogEntry("Dual-mode activated: UART + Logic on GPIO" + String(logicConfig.gpioPin));
        Serial.println("Dual-mode monitoring enabled: UART + Logic analysis simultaneously");
    } else if (enable && !isDualModeCompatible()) {
        dualModeActive = false;
        addLogEntry("Dual-mode failed: Pin conflict - UART on GPIO" + String(uartConfig.rxPin) + ", Logic on GPIO" + String(logicConfig.gpioPin));
        Serial.println("Dual-mode incompatible: Different pins configured");
    } else {
        dualModeActive = false;
        addLogEntry("Dual-mode deactivated");
        Serial.println("Dual-mode monitoring disabled");
    }
}

bool LogicAnalyzer::isDualModeActive() const {
    return dualModeActive;
}

bool LogicAnalyzer::isDualModeCompatible() const {
    // Check if UART RX pin matches Logic GPIO pin
    return (uartConfig.rxPin == logicConfig.gpioPin);
}

void LogicAnalyzer::processDualModeData(bool currentState) {
    // Process Logic Analyzer data
    if (triggerMode != TRIGGER_NONE && !triggerArmed) {
        if (checkTrigger(currentState)) {
            triggerArmed = true;
            addLogEntry("Dual-mode trigger activated on GPIO" + String(logicConfig.gpioPin));
            Serial.println("Dual-mode trigger activated!");
        }
        lastState = currentState;
        // Don't return - still process UART data
    }
    
    // Add Logic sample
    if (triggerArmed) {
        addSample(currentState);
    }
    
    // Process UART data simultaneously
    if (uartSerial && uartSerial->available()) {
        while (uartSerial->available()) {
            char c = uartSerial->read();
            uartBytesReceived++;
            lastUartActivity = millis();
            
            if (c == '\n' || c == '\r') {
                if (uartRxBuffer.length() > 0) {
                    addUartEntry(uartRxBuffer + " [DUAL]", true);  // Mark as dual-mode data
                    uartRxBuffer = "";
                }
            } else if (c >= 32 && c <= 126) {
                uartRxBuffer += c;
                if (uartRxBuffer.length() > UART_MSG_MAX_LENGTH) {
                    addUartEntry(uartRxBuffer + " [DUAL-TRUNC]", true);
                    uartRxBuffer = "";
                }
            } else {
                uartRxBuffer += "[0x" + String(c, HEX) + "]";
            }
        }
    }
    
    // Handle incomplete UART lines
    if (uartRxBuffer.length() > 0 && (millis() - lastUartActivity) > 1000) {
        addUartEntry(uartRxBuffer + " [DUAL-TIMEOUT]", true);
        uartRxBuffer = "";
    }
    
    lastState = currentState;
    
    // Stop Logic capture if buffer is full
    if (isBufferFull()) {
        addLogEntry("Dual-mode Logic buffer full - stopping capture");
        capturing = false;
        Serial.println("Dual-mode Logic buffer full, capture stopped");
    }
}

String LogicAnalyzer::getDualModeStatus() const {
    JsonDocument doc;
    doc["dual_mode_active"] = dualModeActive;
    doc["compatible"] = isDualModeCompatible();
    doc["uart_pin"] = uartConfig.rxPin;
    doc["logic_pin"] = logicConfig.gpioPin;
    doc["uart_monitoring"] = uartMonitoringEnabled;
    doc["logic_capturing"] = capturing;
    doc["logic_samples"] = getBufferUsage();
    doc["uart_entries"] = getUartLogCount();
    
    String result;
    serializeJson(doc, result);
    return result;
}

#endif

LogicAnalyzer::LogicAnalyzer() {
    writeIndex = 0;
    readIndex = 0;
    capturing = false;
    sampleRate = DEFAULT_SAMPLE_RATE;
    gpio1Pin = CHANNEL_0_PIN;  // GPIO1 pin
    triggerMode = TRIGGER_NONE;
    lastState = false;
    triggerArmed = false;
    lastSampleTime = 0;
    
    // UART monitoring initialization
    uartSerial = nullptr;
    uartMonitoringEnabled = false;
    uartRxBuffer = "";
    uartTxBuffer = "";
    lastUartActivity = 0;
    uartBytesReceived = 0;
    uartBytesSent = 0;
    preferences = nullptr;
    maxUartEntries = MAX_UART_ENTRIES;  // Initialize with default
    
    // Half-duplex initialization
    halfDuplexTxMode = false;
    halfDuplexTxTimeout = 0;
    halfDuplexTxQueue = "";
    halfDuplexBusy = false;
    
    // Dual-mode initialization
    dualModeActive = false;
    
    // Flash Storage initialization - Enable by default to use 8MB Flash
    useFlashStorage = true;
    uartLogFileName = "/uart_logs.txt";
    
    // Advanced Logic Analyzer initialization
    flashLogicFileName = "/logic_samples.bin";
    flashSamplesWritten = 0;
    flashWritePosition = 0;
    flashStorageActive = false;
    compressedBuffer = nullptr;
    compressedCount = 0;
    lastTimestamp = 0;
    lastData = false;
    runLength = 0;
    streamingActive = false;
    streamingCount = 0;
    flashWriteBuffer = nullptr;
    bufferPosition = 0;
    
    sampleInterval = 1000000 / sampleRate; // microseconds
}

LogicAnalyzer::~LogicAnalyzer() {
    stopCapture();
    
    // Cleanup advanced buffers
    if (compressedBuffer) {
        free(compressedBuffer);
        compressedBuffer = nullptr;
    }
    if (flashWriteBuffer) {
        free(flashWriteBuffer);
        flashWriteBuffer = nullptr;
    }
    if (flashDataFile) {
        flashDataFile.close();
    }
}

void LogicAnalyzer::begin() {
    Serial.println("Initializing M5Stack AtomProbe GPIO Monitor...");
    initializeGPIO1();
    clearBuffer();
    
    // Initialize LittleFS for potential flash storage
    initFlashStorage();
    
    // Initialize flash storage for Logic Analyzer (default mode)
    if (logicConfig.bufferMode == BUFFER_FLASH) {
        enableFlashBuffering(BUFFER_FLASH, logicConfig.maxFlashSamples);
        addLogEntry("Logic Analyzer Flash mode enabled: " + String(logicConfig.maxFlashSamples) + " samples");
    }
    
    Serial.printf("M5Stack AtomProbe GPIO Monitor initialized at %d Hz with %d sample buffer (Flash: %s)\\n", 
                  sampleRate, logicConfig.maxFlashSamples, 
                  logicConfig.bufferMode == BUFFER_FLASH ? "enabled" : "disabled");
}

void LogicAnalyzer::initializeGPIO1() {
    pinMode(gpio1Pin, INPUT);
    Serial.printf("GPIO1 Pin: %d configured as input\n", gpio1Pin);
}

void LogicAnalyzer::process() {
    // Dual-mode processing (UART + Logic on same pin)
    if (dualModeActive && uartMonitoringEnabled && capturing) {
        uint32_t currentTime = micros();
        if (currentTime - lastSampleTime >= sampleInterval) {
            bool currentState = readGPIO1();
            processDualModeData(currentState);
            lastSampleTime = currentTime;
        }
        return;
    }
    
    // Process UART data monitoring
    if (uartMonitoringEnabled) {
        processUartData();
    }
    
    if (!capturing) return;
    
    uint32_t currentTime = micros();
    
    // Check if it's time for next sample
    if (currentTime - lastSampleTime >= sampleInterval) {
        bool currentState = readGPIO1();
        
        // Check trigger condition
        if (triggerMode != TRIGGER_NONE && !triggerArmed) {
            if (checkTrigger(currentState)) {
                triggerArmed = true;
                addLogEntry("Trigger activated on GPIO1");
                Serial.println("Trigger activated!");
            }
            lastState = currentState;
            return; // Don't sample until triggered
        }
        
        addSample(currentState);
        lastSampleTime = currentTime;
        lastState = currentState;
        
        // Stop if buffer is full
        if (isBufferFull()) {
            addLogEntry("Buffer full - auto-stopping capture");
            stopCapture();
            Serial.println("Buffer full, capture stopped");
        }
    }
}

bool LogicAnalyzer::readGPIO1() {
    // Direct register access for maximum speed on ESP32-S3
    // GPIO1 is in GPIO_IN_REG (0x3ff4403c)
    return (REG_READ(GPIO_IN_REG) & (1 << gpio1Pin)) != 0;
}

bool LogicAnalyzer::checkTrigger(bool currentState) {
    switch (triggerMode) {
        case TRIGGER_RISING_EDGE:
            return !lastState && currentState;
        case TRIGGER_FALLING_EDGE:
            return lastState && !currentState;
        case TRIGGER_BOTH_EDGES:
            return lastState != currentState;
        case TRIGGER_HIGH_LEVEL:
            return currentState;
        case TRIGGER_LOW_LEVEL:
            return !currentState;
        default:
            return true;
    }
}

void LogicAnalyzer::addSample(bool data) {
    Sample sample;
    sample.timestamp = micros();
    sample.data = data;
    
    // Handle different buffer modes
    switch (logicConfig.bufferMode) {
        case BUFFER_RAM:
            // Standard RAM buffer
            buffer[writeIndex] = sample;
            writeIndex = (writeIndex + 1) % BUFFER_SIZE;
            break;
            
        case BUFFER_FLASH:
            // Write directly to flash
            writeToFlash(sample);
            break;
            
        case BUFFER_STREAMING:
            // Continuous streaming
            processStreamingSample(sample);
            break;
            
        case BUFFER_COMPRESSED:
            // Compressed storage
            compressSample(sample);
            break;
    }
}

void LogicAnalyzer::startCapture() {
    clearBuffer();
    triggerArmed = (triggerMode == TRIGGER_NONE);
    lastSampleTime = micros();
    capturing = true;
    addLogEntry("Capture started on GPIO1");
    Serial.println("Capture started");
}

void LogicAnalyzer::stopCapture() {
    capturing = false;
    
    uint32_t maxSize = (logicConfig.bufferMode == BUFFER_FLASH || logicConfig.bufferMode == BUFFER_STREAMING) 
                       ? logicConfig.maxFlashSamples : BUFFER_SIZE;
    
    addLogEntry(String("Capture stopped. Buffer: ") + getBufferUsage() + "/" + maxSize + " (" + 
                (logicConfig.bufferMode == BUFFER_FLASH ? "Flash" : "RAM") + ")");
    Serial.println("Capture stopped");
    
    // Flush any remaining flash data
    if (logicConfig.bufferMode == BUFFER_FLASH) {
        flushFlashBuffer();
    }
}

bool LogicAnalyzer::isCapturing() const {
    return capturing;
}

void LogicAnalyzer::setSampleRate(uint32_t rate) {
    if (rate < MIN_SAMPLE_RATE) rate = MIN_SAMPLE_RATE;
    if (rate > MAX_SAMPLE_RATE) rate = MAX_SAMPLE_RATE;
    
    sampleRate = rate;
    sampleInterval = 1000000 / sampleRate;
    Serial.printf("Sample rate set to %d Hz (interval: %d µs)\n", sampleRate, sampleInterval);
}

uint32_t LogicAnalyzer::getSampleRate() const {
    return sampleRate;
}

void LogicAnalyzer::setTrigger(TriggerMode mode) {
    triggerMode = mode;
    triggerArmed = false;
    Serial.printf("GPIO1 Trigger set: mode=%d\n", mode);
}

void LogicAnalyzer::disableTrigger() {
    triggerMode = TRIGGER_NONE;
    triggerArmed = true;
    Serial.println("GPIO1 Trigger disabled");
}

TriggerMode LogicAnalyzer::getTriggerMode() const {
    return triggerMode;
}

String LogicAnalyzer::getDataAsJSON() {
    JsonDocument doc;
    JsonArray samples = doc["samples"].to<JsonArray>();
    
    uint32_t count = getBufferUsage();
    uint32_t index = readIndex;
    
    for (uint32_t i = 0; i < count; i++) {
        JsonObject sample = samples.add<JsonObject>();
        sample["timestamp"] = buffer[index].timestamp;
        sample["gpio1"] = buffer[index].data;  // Single boolean for GPIO1
        sample["state"] = buffer[index].data ? "HIGH" : "LOW";
        
        index = (index + 1) % BUFFER_SIZE;
    }
    
    doc["sample_count"] = count;
    doc["sample_rate"] = sampleRate;
    doc["gpio_pin"] = gpio1Pin;
    doc["buffer_size"] = BUFFER_SIZE;
    doc["trigger_mode"] = (int)triggerMode;
    
    String result;
    serializeJson(doc, result);
    return result;
}

void LogicAnalyzer::clearBuffer() {
    writeIndex = 0;
    readIndex = 0;
    
    // Clear flash storage if in flash mode
    if (logicConfig.bufferMode == BUFFER_FLASH || logicConfig.bufferMode == BUFFER_STREAMING) {
        flashSamplesWritten = 0;
        flashWritePosition = 0;
        bufferPosition = 0;
        
        if (flashDataFile) {
            flashDataFile.close();
        }
        
        // Remove existing flash file
        if (LittleFS.exists(flashLogicFileName)) {
            LittleFS.remove(flashLogicFileName);
        }
    }
}

uint32_t LogicAnalyzer::getBufferUsage() const {
    if (logicConfig.bufferMode == BUFFER_FLASH || logicConfig.bufferMode == BUFFER_STREAMING) {
        return flashSamplesWritten;
    }
    
    if (writeIndex >= readIndex) {
        return writeIndex - readIndex;
    } else {
        return (BUFFER_SIZE - readIndex) + writeIndex;
    }
}

uint32_t LogicAnalyzer::getCurrentBufferSize() const {
    if (logicConfig.bufferMode == BUFFER_FLASH || logicConfig.bufferMode == BUFFER_STREAMING) {
        return logicConfig.maxFlashSamples;
    }
    return BUFFER_SIZE;
}

bool LogicAnalyzer::isBufferFull() const {
    if (logicConfig.bufferMode == BUFFER_FLASH || logicConfig.bufferMode == BUFFER_STREAMING) {
        return flashSamplesWritten >= logicConfig.maxFlashSamples;
    }
    
    return getBufferUsage() >= (BUFFER_SIZE - 1);
}

void LogicAnalyzer::printStatus() {
    Serial.println("=== M5Stack AtomProbe GPIO1 Monitor Status ===");
    Serial.printf("Capturing: %s\n", capturing ? "YES" : "NO");
    Serial.printf("Sample Rate: %d Hz\n", sampleRate);
    Serial.printf("GPIO Pin: %d\n", gpio1Pin);
    Serial.printf("Buffer Usage: %d/%d (%.1f%%)\n", getBufferUsage(), BUFFER_SIZE, 
                  (getBufferUsage() * 100.0) / BUFFER_SIZE);
    Serial.printf("Trigger Mode: %d\n", triggerMode);
    Serial.printf("Trigger Armed: %s\n", triggerArmed ? "YES" : "NO");
}

void LogicAnalyzer::printChannelStates() {
    bool state = readGPIO1();
    Serial.printf("GPIO1 State: %s (%d)\n", state ? "HIGH" : "LOW", state ? 1 : 0);
}

#ifdef ATOMS3_BUILD
void LogicAnalyzer::initDisplay() {
    // Initialize display settings
    current_page = 0;  // Start with WiFi page
    lastDisplayUpdate = 0;
    drawGradientBackground();
}

// Modern startup logo with blue-purple gradient and white diamond
void LogicAnalyzer::drawStartupLogo() {
    display.fillScreen(0x0841);  // Dark navy background
    
    // Create gradient background
    for(int y = 0; y < 128; y++) {
        uint16_t gradColor = display.color565(6 + y/10, 8 + y/8, 20 + y/4);
        display.drawLine(0, y, 128, y, gradColor);
    }
    
    // Main circular gradient background
    for (int r = 40; r > 5; r--) {
        float ratio = (40.0 - r) / 35.0;
        uint8_t red = 10 + (60 * ratio);     // Dark blue to purple
        uint8_t green = 15 + (40 * ratio);   // Blue component
        uint8_t blue = 80 + (100 * ratio);   // Strong blue to purple
        uint16_t color = display.color565(red, green, blue);
        display.fillCircle(64, 60, r, color);
        delay(20);  // Smooth animation
    }
    
    // White diamond/star shape in center
    display.fillTriangle(64, 35, 49, 60, 79, 60, 0xFFFF);  // Top triangle
    display.fillTriangle(64, 85, 49, 60, 79, 60, 0xFFFF);  // Bottom triangle
    display.fillTriangle(39, 60, 64, 45, 64, 75, 0xFFFF);  // Left triangle
    display.fillTriangle(89, 60, 64, 45, 64, 75, 0xFFFF);  // Right triangle
    
    // Add subtle glow to diamond
    display.drawCircle(64, 60, 20, 0x7BEF);
    display.drawCircle(64, 60, 22, 0x4CAF);
    
    // Product name with modern font
    display.setTextColor(0xFFFF);
    display.setTextSize(1);
    display.setCursor(27, 95);
    display.print("M5Stack");
    display.setCursor(23, 105);
    display.print("AtomProbe");
    
    // Version with accent color
    display.setTextColor(0x7BEF);
    display.setCursor(48, 118);
    display.print("v3.0.0");
    
    // Final glow effect
    for (int i = 0; i < 2; i++) {
        display.drawCircle(64, 60, 35, 0x52AA);
        delay(200);
        display.drawCircle(64, 60, 35, 0x0841);
        delay(200);
    }
}

// Gemini-style gradient background
void LogicAnalyzer::drawGradientBackground() {
    // Gemini dark gradient from top to bottom (dark navy to dark purple)
    for(int i = 0; i < 128; i++) {
        uint16_t gradColor = display.color565(8 + i/16, 4 + i/32, 16 + i/8);
        display.drawLine(0, i, 128, i, gradColor);
    }
}

// Glass morphism panel effect
void LogicAnalyzer::drawGlassPanel(int x, int y, int w, int h) {
    // Semi-transparent dark background with purple tint
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            uint16_t bgColor = display.color565(16, 12, 28);
            display.drawPixel(px, py, bgColor);
        }
    }
    
    // Glass reflection highlight (top edge)
    for (int px = x; px < x + w; px++) {
        display.drawPixel(px, y, 0x52AA);
    }
    
    // Subtle border
    display.drawRect(x, y, w, h, 0x4CAF);
}

// Page 1: WiFi Information Display
void LogicAnalyzer::drawWiFiPage() {
    drawGradientBackground();
    
    // Page title with purple accent
    display.setTextColor(0x52AA);
    display.setTextSize(2);
    display.setCursor(30, 10);
    display.print("WiFi");
    
    // Glass panel for main content
    drawGlassPanel(8, 35, 112, 80);
    
    display.setTextSize(1);
    display.setTextColor(0xFFFF);
    
    // WiFi Connection Status
    display.setCursor(15, 45);
    display.print("Status:");
    display.setTextColor(WiFi.status() == WL_CONNECTED ? 0x4CAF : 0xF800);
    display.setCursor(55, 45);
    display.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    if (WiFi.status() == WL_CONNECTED) {
        // Network Name (SSID)
        display.setTextColor(0xFFFF);
        display.setCursor(15, 60);
        display.print("SSID:");
        display.setTextColor(0xDEFB);
        display.setCursor(15, 70);
        String ssid = WiFi.SSID();
        if (ssid.length() > 15) {
            ssid = ssid.substring(0, 12) + "...";
        }
        display.print(ssid);
        
        // IP Address
        display.setTextColor(0xFFFF);
        display.setCursor(15, 85);
        display.print("IP:");
        display.setTextColor(0x4CAF);
        display.setCursor(15, 95);
        display.print(WiFi.localIP().toString());
        
        // Signal Strength
        display.setTextColor(0xFFFF);
        display.setCursor(15, 105);
        display.print("Signal: ");
        int rssi = WiFi.RSSI();
        display.setTextColor(rssi > -50 ? 0x4CAF : rssi > -70 ? 0xFFEB : 0xF800);
        display.printf("%ddBm", rssi);
    } else if (ap_mode) {
        // Access Point Information
        display.setTextColor(0xFFEB);
        display.setCursor(15, 60);
        display.print("AP Mode Active");
        display.setTextColor(0x4CAF);
        display.setCursor(15, 75);
        display.print("192.168.4.1");
        display.setTextColor(0xDEFB);
        display.setCursor(15, 90);
        display.print("M5Stack-AtomProbe");
    }
    
    // Page indicator
    display.setTextColor(0x52AA);
    display.setCursor(110, 120);
    display.print("1/2");
}

// Page 2: System Information Display
void LogicAnalyzer::drawSystemPage() {
    drawGradientBackground();
    
    // Page title with blue accent
    display.setTextColor(0x4CAF);
    display.setTextSize(2);
    display.setCursor(20, 10);
    display.print("System");
    
    // Glass panel for main content
    drawGlassPanel(8, 35, 112, 80);
    
    display.setTextSize(1);
    
    // CPU Usage (estimated based on capture activity)
    display.setTextColor(0xFFFF);
    display.setCursor(15, 45);
    display.print("CPU:");
    int cpu_usage = capturing ? 85 : 15; // Rough estimation
    display.setTextColor(cpu_usage > 80 ? 0xF800 : cpu_usage > 50 ? 0xFFEB : 0x4CAF);
    display.setCursor(50, 45);
    display.printf("%d%%", cpu_usage);
    
    // Free Heap
    display.setTextColor(0xFFFF);
    display.setCursor(15, 60);
    display.print("RAM:");
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t total_heap = ESP.getHeapSize();
    int heap_percent = ((total_heap - free_heap) * 100) / total_heap;
    display.setTextColor(heap_percent > 80 ? 0xF800 : heap_percent > 60 ? 0xFFEB : 0x4CAF);
    display.setCursor(50, 60);
    display.printf("%dKB", free_heap / 1024);
    
    // Flash Usage
    display.setTextColor(0xFFFF);
    display.setCursor(15, 75);
    display.print("Flash:");
    uint32_t flash_size = ESP.getFlashChipSize();
    display.setTextColor(0x4CAF);
    display.setCursor(50, 75);
    display.printf("%dMB", flash_size / (1024 * 1024));
    
    // Uptime
    display.setTextColor(0xFFFF);
    display.setCursor(15, 90);
    display.print("Uptime:");
    display.setTextColor(0xDEFB);
    display.setCursor(15, 100);
    unsigned long uptime_sec = millis() / 1000;
    unsigned long hours = uptime_sec / 3600;
    unsigned long minutes = (uptime_sec % 3600) / 60;
    display.printf("%luh %lum", hours, minutes);
    
    // Page indicator
    display.setTextColor(0x4CAF);
    display.setCursor(110, 120);
    display.print("2/2");
}

// Switch between pages
void LogicAnalyzer::switchPage() {
    current_page = (current_page + 1) % 2;
}

// Set AP mode status
void LogicAnalyzer::setAPMode(bool isAPMode) {
    ap_mode = isAPMode;
}

// Update the current page display with reduced flicker
void LogicAnalyzer::updateDisplay() {
    static uint8_t last_page = 255; // Force initial draw
    static bool wifi_status_changed = false;
    unsigned long now = millis();
    
    // Check if page changed (immediate update)
    if (current_page != last_page) {
        if (current_page == 0) {
            drawWiFiPage();
        } else {
            drawSystemPage();
        }
        last_page = current_page;
        lastDisplayUpdate = now;
        return;
    }
    
    // Regular timed updates (reduced frequency to prevent blinking)
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        if (current_page == 0) {
            drawWiFiPage();
        } else {
            drawSystemPage();
        }
        lastDisplayUpdate = now;
    }
}

// Legacy compatibility methods - these now redirect to the new dual-page system
void LogicAnalyzer::drawNetworkPage() {
    drawWiFiPage();
}

void LogicAnalyzer::drawSystemStatsPage() {
    drawSystemPage();
}


void LogicAnalyzer::drawGeminiCard(int x, int y, int w, int h, uint16_t grad1, uint16_t grad2, const char* title) {
    // Gemini-style card with gradient and shadow
    
    // Soft shadow effect
    display.fillRoundRect(x + 2, y + 2, w, h, 8, 0x1082);  // Dark shadow
    
    // Main card with vertical gradient
    for(int i = 0; i < h; i++) {
        float ratio = (float)i / h;
        uint16_t r1 = (grad1 >> 11) & 0x1F;
        uint16_t g1 = (grad1 >> 5) & 0x3F;
        uint16_t b1 = grad1 & 0x1F;
        uint16_t r2 = (grad2 >> 11) & 0x1F;
        uint16_t g2 = (grad2 >> 5) & 0x3F;
        uint16_t b2 = grad2 & 0x1F;
        
        uint16_t r = r1 + (r2 - r1) * ratio;
        uint16_t g = g1 + (g2 - g1) * ratio;
        uint16_t b = b1 + (b2 - b1) * ratio;
        
        uint16_t gradColor = (r << 11) | (g << 5) | b;
        display.drawLine(x, y + i, x + w - 1, y + i, gradColor);
    }
    
    // Rounded corners overlay
    display.drawRoundRect(x, y, w, h, 8, 0x6B6D);  // Subtle border
    
    // Modern title with glow effect (optional)
    if (title) {
        display.setTextColor(0xE7FF);
        display.setTextSize(1);
        display.setCursor(x + 4, y + 3);
        // Subtle glow by drawing text slightly offset
        display.setTextColor(0x4208);
        display.setCursor(x + 5, y + 4);
        display.print(title);
        // Main text
        display.setTextColor(0xFFFF);
        display.setCursor(x + 4, y + 3);
        display.print(title);
    }
}


void LogicAnalyzer::addLogEntry(const String& message) {
    String timestamp = String(millis());
    String logEntry = timestamp + ": " + message;
    
    serialLogBuffer.push_back(logEntry);
    
    // Keep only the last MAX_LOG_ENTRIES
    if (serialLogBuffer.size() > MAX_LOG_ENTRIES) {
        serialLogBuffer.erase(serialLogBuffer.begin());
    }
}

String LogicAnalyzer::getLogsAsJSON() {
    JsonDocument doc;
    JsonArray logs = doc["logs"].to<JsonArray>();
    
    for (const auto& logEntry : serialLogBuffer) {
        logs.add(logEntry);
    }
    
    doc["count"] = serialLogBuffer.size();
    doc["max_entries"] = MAX_LOG_ENTRIES;
    
    String result;
    serializeJson(doc, result);
    return result;
}

void LogicAnalyzer::clearLogs() {
    serialLogBuffer.clear();
}

String LogicAnalyzer::getLogsAsPlainText() {
    String result = "# M5Stack AtomProbe - Serial Logs\\n";
    result += "# Generated: " + String(millis()) + "ms\n";
    result += "# Total entries: " + String(serialLogBuffer.size()) + "\n\n";
    
    for (const auto& logEntry : serialLogBuffer) {
        result += logEntry + "\n";
    }
    
    if (serialLogBuffer.empty()) {
        result += "No log entries available.\n";
    }
    
    return result;
}

String LogicAnalyzer::getDataAsCSV() {
    String result = "# M5Stack AtomProbe - GPIO1 Capture Data (CSV Format)\\n";
    result += "# Generated: " + String(millis()) + "ms\n";
    result += "# Sample Rate: " + String(sampleRate) + " Hz\n";
    result += "# GPIO Pin: " + String(gpio1Pin) + "\n";
    result += "# Buffer Size: " + String(BUFFER_SIZE) + " samples\n";
    result += "# Buffer Usage: " + String(getBufferUsage()) + "/" + String(BUFFER_SIZE) + 
                " (" + String((getBufferUsage() * 100.0) / BUFFER_SIZE, 1) + "%)\n";
    result += "# Trigger Mode: " + String((int)triggerMode) + "\n\n";
    
    // CSV Header - optimized for single GPIO1 channel
    result += "Sample,Timestamp_us,GPIO1_Digital,GPIO1_State\n";
    
    uint32_t count = getBufferUsage();
    uint32_t index = readIndex;
    
    for (uint32_t i = 0; i < count; i++) {
        result += String(i + 1) + ",";  // Sample number
        result += String(buffer[index].timestamp) + ",";  // Timestamp
        result += String(buffer[index].data ? 1 : 0) + ",";  // Digital value (0/1)
        result += String(buffer[index].data ? "HIGH" : "LOW");  // State string
        result += "\n";
        
        index = (index + 1) % BUFFER_SIZE;
    }
    
    if (count == 0) {
        result += "# No capture data available\n";
        result += "# Connect a signal to GPIO" + String(gpio1Pin) + " and start capture\n";
    }
    
    return result;
}

// Logic Analyzer Configuration Functions
void LogicAnalyzer::configureLogic(uint32_t sampleRate, uint8_t gpioPin, TriggerMode triggerMode, uint32_t bufferSize, uint8_t preTriggerPercent) {
    // Input validation
    if (sampleRate < MIN_SAMPLE_RATE) sampleRate = MIN_SAMPLE_RATE;
    if (sampleRate > MAX_SAMPLE_RATE) sampleRate = MAX_SAMPLE_RATE;
    if (gpioPin > 48) gpioPin = CHANNEL_0_PIN;  // ESP32 has max 48 GPIO pins
    if (bufferSize > MAX_BUFFER_SIZE) bufferSize = MAX_BUFFER_SIZE;
    if (bufferSize < 1024) bufferSize = 1024;  // Minimum sensible buffer size
    if (preTriggerPercent > 90) preTriggerPercent = 90;
    if ((int)triggerMode < 0 || (int)triggerMode > 5) triggerMode = TRIGGER_NONE;
    
    logicConfig.sampleRate = sampleRate;
    logicConfig.gpioPin = gpioPin;
    logicConfig.triggerMode = triggerMode;
    logicConfig.bufferSize = bufferSize;
    logicConfig.preTriggerPercent = preTriggerPercent;
    
    // Apply configuration to analyzer
    setSampleRate(sampleRate);
    setTrigger(triggerMode);
    gpio1Pin = gpioPin;
    
    saveLogicConfig();
    
    String configMsg = "Logic Analyzer configured: " + String(sampleRate) + "Hz, GPIO" + String(gpioPin) + 
                       ", Trigger:" + String((int)triggerMode) + ", Buffer:" + String(bufferSize) +
                       ", PreTrig:" + String(preTriggerPercent) + "%";
    addLogEntry(configMsg);
    Serial.println(configMsg);
}

String LogicAnalyzer::getLogicConfigAsJSON() {
    JsonDocument doc;
    doc["sample_rate"] = logicConfig.sampleRate;
    doc["gpio_pin"] = logicConfig.gpioPin;
    doc["trigger_mode"] = (int)logicConfig.triggerMode;
    doc["trigger_mode_string"] = (logicConfig.triggerMode == TRIGGER_NONE ? "None" : 
                                  logicConfig.triggerMode == TRIGGER_RISING_EDGE ? "Rising Edge" :
                                  logicConfig.triggerMode == TRIGGER_FALLING_EDGE ? "Falling Edge" :
                                  logicConfig.triggerMode == TRIGGER_BOTH_EDGES ? "Both Edges" :
                                  logicConfig.triggerMode == TRIGGER_HIGH_LEVEL ? "High Level" :
                                  logicConfig.triggerMode == TRIGGER_LOW_LEVEL ? "Low Level" : "Unknown");
    doc["buffer_size"] = logicConfig.bufferSize;
    doc["pre_trigger_percent"] = logicConfig.preTriggerPercent;
    doc["enabled"] = logicConfig.enabled;
    doc["buffer_duration_seconds"] = calculateBufferDuration();
    doc["min_sample_rate"] = MIN_SAMPLE_RATE;
    doc["max_sample_rate"] = MAX_SAMPLE_RATE;
    
    String result;
    serializeJson(doc, result);
    return result;
}

void LogicAnalyzer::saveLogicConfig() {
    if (preferences) {
        preferences->putUInt("logic_rate", logicConfig.sampleRate);
        preferences->putUChar("logic_gpio", logicConfig.gpioPin);
        preferences->putUChar("logic_trig", (uint8_t)logicConfig.triggerMode);
        preferences->putUInt("logic_buffer", logicConfig.bufferSize);
        preferences->putUChar("logic_pretrig", logicConfig.preTriggerPercent);
        preferences->putBool("logic_enabled", logicConfig.enabled);
        
        String configMsg = "Logic config saved: " + String(logicConfig.sampleRate) + "Hz, GPIO" + 
                           String(logicConfig.gpioPin) + ", Trigger:" + String((int)logicConfig.triggerMode);
        addLogEntry(configMsg);
        Serial.println(configMsg);
    } else {
        addLogEntry("Logic config save failed - no preferences available");
        Serial.println("Logic config save failed - no preferences available");
    }
}

void LogicAnalyzer::loadLogicConfig() {
    if (preferences) {
        logicConfig.sampleRate = preferences->getUInt("logic_rate", DEFAULT_SAMPLE_RATE);
        logicConfig.gpioPin = preferences->getUChar("logic_gpio", CHANNEL_0_PIN);
        logicConfig.triggerMode = (TriggerMode)preferences->getUChar("logic_trig", TRIGGER_NONE);
        logicConfig.bufferSize = preferences->getUInt("logic_buffer", BUFFER_SIZE);
        logicConfig.preTriggerPercent = preferences->getUChar("logic_pretrig", 10);
        logicConfig.enabled = preferences->getBool("logic_enabled", true);
        
        // Apply loaded configuration
        setSampleRate(logicConfig.sampleRate);
        setTrigger(logicConfig.triggerMode);
        gpio1Pin = logicConfig.gpioPin;
        
        String configMsg = "Logic config loaded: " + String(logicConfig.sampleRate) + "Hz, GPIO" + 
                           String(logicConfig.gpioPin) + ", Trigger:" + String((int)logicConfig.triggerMode);
        addLogEntry(configMsg);
        Serial.println(configMsg);
    } else {
        // Use default values
        logicConfig.sampleRate = DEFAULT_SAMPLE_RATE;
        logicConfig.gpioPin = CHANNEL_0_PIN;
        logicConfig.triggerMode = TRIGGER_NONE;
        logicConfig.bufferSize = BUFFER_SIZE;
        logicConfig.preTriggerPercent = 10;
        logicConfig.enabled = true;
        addLogEntry("Logic config loaded (defaults - no preferences available)");
    }
}

float LogicAnalyzer::calculateBufferDuration() const {
    if (logicConfig.sampleRate == 0) return 0.0;
    return (float)logicConfig.bufferSize / (float)logicConfig.sampleRate;
}

// UART Monitoring Functions
void LogicAnalyzer::configureUart(uint32_t baudrate, uint8_t dataBits, uint8_t parity, uint8_t stopBits, uint8_t rxPin, int8_t txPin, UartDuplexMode duplexMode) {
    uartConfig.baudrate = baudrate;
    uartConfig.dataBits = dataBits;
    uartConfig.parity = parity;
    uartConfig.stopBits = stopBits;
    uartConfig.rxPin = rxPin;
    uartConfig.txPin = txPin;
    uartConfig.duplexMode = duplexMode;
    
    // Initialize half-duplex state variables
    halfDuplexTxMode = false;
    halfDuplexTxTimeout = 0;
    halfDuplexTxQueue = "";
    halfDuplexBusy = false;
    
    saveUartConfig();
    
    String modeStr = (duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
    String configMsg = "UART configured: " + String(baudrate) + " baud, " + String(dataBits) + 
                       String(parity == 0 ? "N" : (parity == 1 ? "O" : "E")) + String(stopBits) +
                       ", RX:" + String(rxPin) + ", TX:" + String(txPin) + ", " + modeStr + "-Duplex";
    addLogEntry(configMsg);
    Serial.println(configMsg);
}

void LogicAnalyzer::enableUartMonitoring() {
    if (uartSerial) {
        uartSerial->end();
    }
    
    // Use Serial2 for external UART monitoring
    uartSerial = &Serial2;
    
    // Configure UART parameters
    uint32_t config = SERIAL_8N1;
    if (uartConfig.dataBits == 7) {
        if (uartConfig.parity == 0) config = SERIAL_7N1;
        else if (uartConfig.parity == 1) config = SERIAL_7O1;
        else if (uartConfig.parity == 2) config = SERIAL_7E1;
        if (uartConfig.stopBits == 2) {
            if (uartConfig.parity == 0) config = SERIAL_7N2;
            else if (uartConfig.parity == 1) config = SERIAL_7O2;
            else if (uartConfig.parity == 2) config = SERIAL_7E2;
        }
    } else if (uartConfig.dataBits == 8) {
        if (uartConfig.parity == 0) config = SERIAL_8N1;
        else if (uartConfig.parity == 1) config = SERIAL_8O1;
        else if (uartConfig.parity == 2) config = SERIAL_8E1;
        if (uartConfig.stopBits == 2) {
            if (uartConfig.parity == 0) config = SERIAL_8N2;
            else if (uartConfig.parity == 1) config = SERIAL_8O2;
            else if (uartConfig.parity == 2) config = SERIAL_8E2;
        }
    }
    
    // Initialize UART based on duplex mode
    if (uartConfig.duplexMode == UART_HALF_DUPLEX) {
        // Half-duplex: Start in RX mode, TX pin not used for hardware UART
        uartSerial->begin(uartConfig.baudrate, config, uartConfig.rxPin, -1);
        setupHalfDuplexPin(false); // Start in RX mode
        halfDuplexTxMode = false;
        halfDuplexBusy = false;
        addLogEntry("Half-duplex mode: RX pin " + String(uartConfig.rxPin) + " (bidirectional)");
    } else {
        // Full-duplex: Standard RX/TX configuration
        uartSerial->begin(uartConfig.baudrate, config, uartConfig.rxPin, uartConfig.txPin);
    }
    
    uartMonitoringEnabled = true;
    uartConfig.enabled = true;
    uartRxBuffer = "";
    uartTxBuffer = "";
    lastUartActivity = millis();
    uartBytesReceived = 0;
    uartBytesSent = 0;
    
    String modeStr = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
    String enableMsg = "UART monitoring enabled (" + modeStr + "-duplex) on RX:" + String(uartConfig.rxPin);
    if (uartConfig.duplexMode == UART_FULL_DUPLEX && uartConfig.txPin != -1) {
        enableMsg += ", TX:" + String(uartConfig.txPin);
    }
    enableMsg += " @ " + String(uartConfig.baudrate) + " baud";
    addLogEntry(enableMsg);
    Serial.println(enableMsg);
}

void LogicAnalyzer::disableUartMonitoring() {
    uartMonitoringEnabled = false;
    uartConfig.enabled = false;
    
    if (uartSerial) {
        uartSerial->end();
        uartSerial = nullptr;
    }
    
    addLogEntry("UART monitoring disabled");
    Serial.println("UART monitoring disabled");
}

void LogicAnalyzer::processUartData() {
    if (!uartSerial || !uartMonitoringEnabled) return;
    
    // Handle half-duplex mode
    if (uartConfig.duplexMode == UART_HALF_DUPLEX) {
        processHalfDuplexQueue();
        
        // Check for TX timeout (switch back to RX mode)
        if (halfDuplexTxMode && (millis() - halfDuplexTxTimeout > 100)) {
            switchToRxMode();
        }
    }
    
    // Process incoming data (only if in RX mode for half-duplex)
    if (uartConfig.duplexMode == UART_FULL_DUPLEX || !halfDuplexTxMode) {
        while (uartSerial->available()) {
            char c = uartSerial->read();
            uartBytesReceived++;
            lastUartActivity = millis();
            
            if (c == '\n' || c == '\r') {
                if (uartRxBuffer.length() > 0) {
                    addUartEntry(uartRxBuffer, true);  // true = RX data
                    uartRxBuffer = "";
                }
            } else if (c >= 32 && c <= 126) {  // Printable characters only
                uartRxBuffer += c;
                
                // Prevent buffer overflow
                if (uartRxBuffer.length() > UART_MSG_MAX_LENGTH) {
                    addUartEntry(uartRxBuffer + " [TRUNCATED]", true);
                    uartRxBuffer = "";
                }
            } else {
                // Handle non-printable characters as hex
                uartRxBuffer += "[0x" + String(c, HEX) + "]";
            }
        }
        
        // Handle incomplete lines that timeout
        if (uartRxBuffer.length() > 0 && (millis() - lastUartActivity) > 1000) {
            addUartEntry(uartRxBuffer + " [TIMEOUT]", true);
            uartRxBuffer = "";
        }
    }
}

void LogicAnalyzer::addUartEntry(const String& data, bool isRx) {
    uint32_t timestamp = millis();
    String direction = isRx ? "RX" : "TX";
    String uartEntry = String(timestamp) + ": [UART " + direction + "] " + data;
    
    if (useFlashStorage) {
        // Flash-based storage
        File logFile = LittleFS.open(uartLogFileName, "a");
        if (logFile) {
            logFile.println(uartEntry);
            logFile.close();
        } else {
            // Fallback to RAM if Flash write fails
            uartLogBuffer.push_back(uartEntry);
            Serial.println("Flash write failed, fallback to RAM");
        }
    } else {
        // RAM-based storage
        uartLogBuffer.push_back(uartEntry);
        
        // Use smart buffer management instead of simple deletion
        if (uartLogBuffer.size() > maxUartEntries) {
            compactUartLogs();  // Remove oldest 20% when full
        }
    }
    
    // Also add to regular serial log for debugging
    addLogEntry("UART " + direction + ": " + data);
}

String LogicAnalyzer::getUartLogsAsJSON() {
    JsonDocument doc;
    JsonArray logs = doc["uart_logs"].to<JsonArray>();
    
    size_t logCount = 0;
    size_t memoryUsage = 0;
    
    if (useFlashStorage && LittleFS.exists(uartLogFileName)) {
        // Read from Flash storage
        File logFile = LittleFS.open(uartLogFileName, "r");
        if (logFile) {
            while (logFile.available()) {
                String line = logFile.readStringUntil('\n');
                if (line.length() > 0) {
                    logs.add(line);
                    logCount++;
                    memoryUsage += line.length();
                }
            }
            logFile.close();
        }
    } else {
        // Read from RAM storage
        for (const auto& uartEntry : uartLogBuffer) {
            logs.add(uartEntry);
        }
        logCount = uartLogBuffer.size();
        memoryUsage = getUartMemoryUsage();
    }
    
    doc["count"] = logCount;
    doc["max_entries"] = maxUartEntries;
    doc["monitoring_enabled"] = uartMonitoringEnabled;
    doc["last_activity"] = lastUartActivity;
    doc["bytes_received"] = uartBytesReceived;
    doc["bytes_sent"] = uartBytesSent;
    doc["memory_usage"] = memoryUsage;
    doc["buffer_full"] = (logCount >= maxUartEntries);
    doc["storage_type"] = useFlashStorage ? "Flash" : "RAM";
    doc["flash_file"] = useFlashStorage ? uartLogFileName : "";
    
    // Embed config as JSON object, not string
    JsonDocument configDoc;
    configDoc["baudrate"] = uartConfig.baudrate;
    configDoc["data_bits"] = uartConfig.dataBits;
    configDoc["parity"] = uartConfig.parity;
    configDoc["parity_string"] = (uartConfig.parity == 0 ? "None" : (uartConfig.parity == 1 ? "Odd" : "Even"));
    configDoc["stop_bits"] = uartConfig.stopBits;
    configDoc["rx_pin"] = uartConfig.rxPin;
    configDoc["tx_pin"] = uartConfig.txPin;
    configDoc["duplex_mode"] = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? 0 : 1;  // 0=Full, 1=Half
    configDoc["duplex_string"] = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
    configDoc["enabled"] = uartConfig.enabled;
    
    doc["config"] = configDoc;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String LogicAnalyzer::getUartConfigAsJSON() {
    JsonDocument doc;
    doc["baudrate"] = uartConfig.baudrate;
    doc["data_bits"] = uartConfig.dataBits;
    doc["parity"] = uartConfig.parity;  // 0=None, 1=Odd, 2=Even
    doc["parity_string"] = (uartConfig.parity == 0 ? "None" : (uartConfig.parity == 1 ? "Odd" : "Even"));
    doc["stop_bits"] = uartConfig.stopBits;
    doc["rx_pin"] = uartConfig.rxPin;
    doc["tx_pin"] = uartConfig.txPin;
    doc["duplex_mode"] = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? 0 : 1;  // 0=Full, 1=Half
    doc["duplex_string"] = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
    doc["enabled"] = uartConfig.enabled;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String LogicAnalyzer::getUartLogsAsPlainText() {
    String result = "# AtomS3 AtomProbe - UART Communication Logs\\n";
    result += "# Generated: " + String(millis()) + "ms\n";
    result += "# Monitoring Enabled: " + String(uartMonitoringEnabled ? "YES" : "NO") + "\n";
    result += "# Last Activity: " + String(lastUartActivity) + "ms\n";
    result += "# Storage Type: " + String(useFlashStorage ? "Flash" : "RAM") + "\n";
    
    size_t logCount = 0;
    
    if (useFlashStorage && LittleFS.exists(uartLogFileName)) {
        // Read from Flash storage
        result += "# Flash File: " + uartLogFileName + "\n";
        File logFile = LittleFS.open(uartLogFileName, "r");
        if (logFile) {
            while (logFile.available()) {
                String line = logFile.readStringUntil('\n');
                if (line.length() > 0) {
                    result += line + "\n";
                    logCount++;
                }
            }
            logFile.close();
        }
    } else {
        // Read from RAM storage
        logCount = uartLogBuffer.size();
        for (const auto& uartEntry : uartLogBuffer) {
            result += uartEntry + "\n";
        }
    }
    
    result = result.substring(0, result.lastIndexOf('\n'));
    result += "\n# Total entries: " + String(logCount) + "\n\n";
    
    if (logCount == 0) {
        result += "No UART communication logged.\n";
        if (!uartMonitoringEnabled) {
            result += "Note: UART monitoring is currently disabled.\n";
        }
    }
    
    return result;
}

void LogicAnalyzer::clearUartLogs() {
    if (useFlashStorage) {
        clearFlashUartLogs();  // Clear Flash logs
    } else {
        uartLogBuffer.clear();  // Clear RAM logs
    }
    addLogEntry("UART logs cleared");
}

void LogicAnalyzer::saveUartConfig() {
    if (preferences) {
        preferences->putUInt("uart_baud", uartConfig.baudrate);
        preferences->putUChar("uart_data", uartConfig.dataBits);
        preferences->putUChar("uart_parity", uartConfig.parity);
        preferences->putUChar("uart_stop", uartConfig.stopBits);
        preferences->putUChar("uart_rx_pin", uartConfig.rxPin);
        preferences->putChar("uart_tx_pin", uartConfig.txPin);
        preferences->putUChar("uart_duplex", (uint8_t)uartConfig.duplexMode);
        preferences->putBool("uart_enabled", uartConfig.enabled);
        
        String modeStr = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
        String configMsg = "UART config saved: " + String(uartConfig.baudrate) + " baud, " + 
                           String(uartConfig.dataBits) + 
                           String(uartConfig.parity == 0 ? "N" : (uartConfig.parity == 1 ? "O" : "E")) + 
                           String(uartConfig.stopBits) + ", RX:" + String(uartConfig.rxPin) + ", TX:" + String(uartConfig.txPin) + 
                           ", " + modeStr + "-Duplex";
        addLogEntry(configMsg);
        Serial.println(configMsg);
    } else {
        addLogEntry("UART config save failed - no preferences available");
        Serial.println("UART config save failed - no preferences available");
    }
}

void LogicAnalyzer::loadUartConfig() {
    if (preferences) {
        uartConfig.baudrate = preferences->getUInt("uart_baud", 115200);
        uartConfig.dataBits = preferences->getUChar("uart_data", 8);
        uartConfig.parity = preferences->getUChar("uart_parity", 0);
        uartConfig.stopBits = preferences->getUChar("uart_stop", 1);
        uartConfig.rxPin = preferences->getUChar("uart_rx_pin", 7);
        uartConfig.txPin = preferences->getChar("uart_tx_pin", -1);
        uartConfig.duplexMode = (UartDuplexMode)preferences->getUChar("uart_duplex", UART_FULL_DUPLEX);
        uartConfig.enabled = preferences->getBool("uart_enabled", false);
        
        String modeStr = (uartConfig.duplexMode == UART_FULL_DUPLEX) ? "Full" : "Half";
        String configMsg = "UART config loaded: " + String(uartConfig.baudrate) + " baud, " + 
                           String(uartConfig.dataBits) + 
                           String(uartConfig.parity == 0 ? "N" : (uartConfig.parity == 1 ? "O" : "E")) + 
                           String(uartConfig.stopBits) + ", RX:" + String(uartConfig.rxPin) + ", TX:" + String(uartConfig.txPin) + 
                           ", " + modeStr + "-Duplex";
        addLogEntry(configMsg);
        Serial.println(configMsg);
    } else {
        // Use default values
        uartConfig.baudrate = 115200;
        uartConfig.dataBits = 8;
        uartConfig.parity = 0;
        uartConfig.stopBits = 1;
        uartConfig.rxPin = 7;
        uartConfig.txPin = -1;
        uartConfig.duplexMode = UART_FULL_DUPLEX;
        uartConfig.enabled = false;
        addLogEntry("UART config loaded (defaults - no preferences available)");
        Serial.println("UART config loaded (defaults)");
    }
}

void LogicAnalyzer::setPreferences(Preferences* prefs) {
    preferences = prefs;
}

// UART Buffer Management Functions
size_t LogicAnalyzer::getUartLogCount() const {
    return uartLogBuffer.size();
}

size_t LogicAnalyzer::getUartMemoryUsage() const {
    size_t totalSize = 0;
    for (const auto& entry : uartLogBuffer) {
        totalSize += entry.length();
    }
    return totalSize;
}

bool LogicAnalyzer::isUartBufferFull() const {
    return uartLogBuffer.size() >= maxUartEntries;
}

void LogicAnalyzer::compactUartLogs() {
    // Remove oldest 20% of entries when buffer is getting full
    if (uartLogBuffer.size() >= maxUartEntries * 0.9) {
        size_t removeCount = maxUartEntries * 0.2;
        uartLogBuffer.erase(uartLogBuffer.begin(), uartLogBuffer.begin() + removeCount);
        
        String compactMsg = "UART buffer compacted: removed " + String(removeCount) + " oldest entries (" + String(uartLogBuffer.size()) + "/" + String(maxUartEntries) + " remaining)";
        addLogEntry(compactMsg);
        Serial.println(compactMsg);
    }
}

void LogicAnalyzer::setUartBufferSize(size_t maxEntries) {
    maxUartEntries = maxEntries;
    
    // If current buffer is larger than new limit, compact it
    while (uartLogBuffer.size() > maxUartEntries) {
        uartLogBuffer.erase(uartLogBuffer.begin());
    }
    
    String msg = "UART buffer size set to " + String(maxUartEntries) + " entries";
    addLogEntry(msg);
    Serial.println(msg);
}

size_t LogicAnalyzer::getMaxUartEntries() const {
    return maxUartEntries;
}

float LogicAnalyzer::getUartBufferUsagePercent() const {
    return (uartLogBuffer.size() * 100.0) / maxUartEntries;
}

// Flash Storage Methods
void LogicAnalyzer::enableFlashStorage(bool enable) {
    if (enable && !LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed! Attempting to format...");
        addLogEntry("Flash storage mount failed - formatting...");
        
        // Try to format LittleFS partition
        if (!LittleFS.begin(true)) {  // true = format if mount fails
            Serial.println("LittleFS format failed! Using RAM storage instead.");
            addLogEntry("Flash storage format failed - using RAM");
            useFlashStorage = false;
            return;
        } else {
            Serial.println("LittleFS formatted and mounted successfully!");
            addLogEntry("Flash storage formatted and ready");
        }
    }
    
    if (enable != useFlashStorage) {
        bool wasEnabled = useFlashStorage;
        useFlashStorage = enable;
        
        if (enable) {
            // Switching to Flash: generate unique filename with timestamp
            uartLogFileName = "/uart_logs_" + String(millis()) + ".txt";
            addLogEntry("Switched to Flash storage: " + uartLogFileName);
            Serial.println("UART logging switched to Flash storage: " + uartLogFileName);
            
            // Copy existing RAM logs to Flash if any
            if (!uartLogBuffer.empty()) {
                File logFile = LittleFS.open(uartLogFileName, "w");
                if (logFile) {
                    for (const auto& entry : uartLogBuffer) {
                        logFile.println(entry);
                    }
                    logFile.close();
                    addLogEntry("Migrated " + String(uartLogBuffer.size()) + " entries to Flash");
                    uartLogBuffer.clear();  // Clear RAM buffer
                }
            }
        } else {
            // Switching to RAM: load Flash logs to RAM if file exists
            if (LittleFS.exists(uartLogFileName)) {
                File logFile = LittleFS.open(uartLogFileName, "r");
                if (logFile) {
                    uartLogBuffer.clear();
                    while (logFile.available() && uartLogBuffer.size() < maxUartEntries) {
                        String line = logFile.readStringUntil('\n');
                        if (line.length() > 0) {
                            uartLogBuffer.push_back(line);
                        }
                    }
                    logFile.close();
                    addLogEntry("Migrated " + String(uartLogBuffer.size()) + " entries from Flash to RAM");
                }
            }
            addLogEntry("Switched to RAM storage");
            Serial.println("UART logging switched to RAM storage");
        }
    }
}

bool LogicAnalyzer::isFlashStorageEnabled() const {
    return useFlashStorage;
}

void LogicAnalyzer::initFlashStorage() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed! Attempting to format...");
        addLogEntry("Flash storage mount failed - formatting...");
        
        // Try to format LittleFS partition
        if (!LittleFS.begin(true)) {  // true = format if mount fails
            Serial.println("LittleFS format failed! Flash storage not available.");
            addLogEntry("Flash storage initialization failed - format error");
            useFlashStorage = false;
            return;
        } else {
            Serial.println("LittleFS formatted and mounted successfully!");
            addLogEntry("Flash storage formatted and initialized");
        }
    } else {
        Serial.println("LittleFS initialized successfully");
        addLogEntry("Flash storage available (LittleFS)");
    }
    
    // List total and used flash space
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("Flash Storage: %d KB total, %d KB used, %d KB free\n", 
                 totalBytes/1024, usedBytes/1024, (totalBytes-usedBytes)/1024);
    
    String flashInfo = "Flash: " + String(totalBytes/1024) + "KB total, " + 
                      String(usedBytes/1024) + "KB used, " + 
                      String((totalBytes-usedBytes)/1024) + "KB free";
    addLogEntry(flashInfo);
}

void LogicAnalyzer::clearFlashUartLogs() {
    if (useFlashStorage && LittleFS.exists(uartLogFileName)) {
        if (LittleFS.remove(uartLogFileName)) {
            addLogEntry("Flash UART logs cleared: " + uartLogFileName);
            Serial.println("Flash UART logs cleared");
        } else {
            addLogEntry("Failed to clear Flash UART logs");
            Serial.println("Failed to clear Flash UART logs");
        }
    }
}

// ===== ADVANCED LOGIC ANALYZER FEATURES =====

// Flash Storage for Logic Analyzer
void LogicAnalyzer::initFlashLogicStorage() {
    if (!LittleFS.begin()) {
        Serial.println("Flash storage not available for logic analyzer");
        addLogEntry("Logic flash storage init failed");
        return;
    }
    
    // Initialize flash write buffer
    if (!flashWriteBuffer) {
        flashWriteBuffer = (uint8_t*)malloc(FLASH_CHUNK_SIZE);
        if (!flashWriteBuffer) {
            Serial.println("Failed to allocate flash write buffer");
            return;
        }
    }
    
    // Initialize compression buffer
    if (!compressedBuffer) {
        compressedBuffer = (CompressedSample*)malloc(sizeof(CompressedSample) * 1000);
        if (!compressedBuffer) {
            Serial.println("Failed to allocate compression buffer");
        }
    }
    
    flashStorageActive = true;
    bufferPosition = 0;
    compressedCount = 0;
    
    addLogEntry("Logic flash storage initialized");
    Serial.println("Logic Analyzer Flash Storage ready");
}

void LogicAnalyzer::enableFlashBuffering(BufferMode mode, uint32_t maxSamples) {
    logicConfig.bufferMode = mode;
    
    // Limit flash samples to realistic values (5.6MB total shared with UART)
    if (maxSamples > MAX_FLASH_BUFFER_SIZE) {
        maxSamples = MAX_FLASH_BUFFER_SIZE;
    }
    logicConfig.maxFlashSamples = maxSamples;
    
    if (mode == BUFFER_FLASH || mode == BUFFER_STREAMING) {
        initFlashLogicStorage();
        
        // Initialize flash header
        flashHeader.magic = 0x4C4F4749;  // "LOGI"
        flashHeader.version = 1;
        flashHeader.sample_count = 0;
        flashHeader.buffer_size = maxSamples;
        flashHeader.sample_rate = sampleRate;
        flashHeader.compression = (uint32_t)logicConfig.compression;
        
        String msg = "Flash buffering enabled: " + String(maxSamples) + " max samples (shared 5.6MB flash)";
        addLogEntry(msg);
        addLogEntry("WARNING: Flash storage shared with UART logs");
    }
    
    if (mode == BUFFER_COMPRESSED) {
        if (!compressedBuffer) {
            initFlashLogicStorage();
        }
        addLogEntry("Compression enabled: " + String(logicConfig.compression));
    }
}

void LogicAnalyzer::writeToFlash(const Sample& sample) {
    if (!flashWriteBuffer) return;
    
    // Write sample to buffer
    memcpy(flashWriteBuffer + bufferPosition, &sample, sizeof(Sample));
    bufferPosition += sizeof(Sample);
    flashSamplesWritten++;
    
    // Flush if buffer is full
    if (bufferPosition >= FLASH_CHUNK_SIZE - sizeof(Sample)) {
        flushFlashBuffer();
    }
}

void LogicAnalyzer::flushFlashBuffer() {
    if (!flashWriteBuffer || bufferPosition == 0) return;
    
    if (!flashDataFile) {
        flashDataFile = LittleFS.open(flashLogicFileName, "a");
    }
    
    if (flashDataFile) {
        size_t written = flashDataFile.write(flashWriteBuffer, bufferPosition);
        flashWritePosition += written;
        bufferPosition = 0;
    }
}

// Compression Methods
void LogicAnalyzer::enableCompression(CompressionType type) {
    logicConfig.compression = type;
    runLength = 0;
    lastTimestamp = 0;
    lastData = false;
    
    String compressionName = (type == COMPRESS_RLE ? "RLE" :
                             type == COMPRESS_DELTA ? "Delta" :
                             type == COMPRESS_HYBRID ? "Hybrid" : "None");
    
    addLogEntry("Compression enabled: " + compressionName);
}

void LogicAnalyzer::compressSample(const Sample& sample) {
    if (!compressedBuffer) return;
    
    switch (logicConfig.compression) {
        case COMPRESS_RLE:
            compressRunLength(sample.data, sample.timestamp, 1);
            break;
        case COMPRESS_DELTA:
            compressDelta(sample.timestamp, sample.data);
            break;
        case COMPRESS_HYBRID:
            // Use RLE for consecutive same values, delta for changes
            if (sample.data == lastData && runLength < 65535) {
                runLength++;
            } else {
                if (runLength > 0) {
                    compressRunLength(lastData, lastTimestamp, runLength);
                }
                compressDelta(sample.timestamp, sample.data);
                runLength = 1;
            }
            break;
        default:
            break;
    }
    
    lastTimestamp = sample.timestamp;
    lastData = sample.data;
}

void LogicAnalyzer::compressRunLength(bool data, uint32_t timestamp, uint16_t count) {
    if (compressedCount < 1000) {
        CompressedSample& compressed = compressedBuffer[compressedCount];
        compressed.timestamp = timestamp;
        compressed.count = count;
        compressed.data = data;
        compressed.type = COMPRESS_RLE;
        compressedCount++;
    }
}

void LogicAnalyzer::compressDelta(uint32_t timestamp, bool data) {
    if (compressedCount < 1000) {
        CompressedSample& compressed = compressedBuffer[compressedCount];
        compressed.timestamp = timestamp - lastTimestamp;
        compressed.count = 1;
        compressed.data = data;
        compressed.type = COMPRESS_DELTA;
        compressedCount++;
    }
}

// Streaming Methods
void LogicAnalyzer::enableStreamingMode(bool enable) {
    logicConfig.streamingMode = enable;
    streamingActive = enable;
    streamingCount = 0;
    
    if (enable) {
        initFlashLogicStorage();
        addLogEntry("Streaming mode enabled - continuous capture to flash");
    } else {
        flushFlashBuffer();
        if (flashDataFile) {
            flashDataFile.close();
        }
        addLogEntry("Streaming mode disabled");
    }
}

void LogicAnalyzer::processStreamingSample(const Sample& sample) {
    if (!streamingActive) return;
    
    streamingCount++;
    
    if (logicConfig.compression != COMPRESS_NONE) {
        compressSample(sample);
        
        // Flush compressed data periodically
        if (compressedCount >= 500) {
            // Write compressed samples to flash
            for (uint32_t i = 0; i < compressedCount; i++) {
                writeToFlash(*(Sample*)&compressedBuffer[i]);
            }
            compressedCount = 0;
        }
    } else {
        writeToFlash(sample);
    }
    
    // Flush to flash every 1000 samples
    if (streamingCount % 1000 == 0) {
        flushFlashBuffer();
    }
}

String LogicAnalyzer::getFlashDataAsJSON(uint32_t offset, uint32_t count) {
    JsonDocument doc;
    doc["flash_samples"] = flashSamplesWritten;
    doc["flash_position"] = flashWritePosition;
    doc["storage_mb"] = getFlashStorageUsedMB();
    doc["buffer_mode"] = getBufferModeString();
    doc["compression_ratio"] = getCompressionRatio();
    
    String result;
    serializeJson(doc, result);
    return result;
}

uint32_t LogicAnalyzer::getCompressionRatio() const {
    if (flashSamplesWritten == 0) return 0;
    
    uint32_t originalSize = flashSamplesWritten * sizeof(Sample);
    uint32_t compressedSize = compressedCount * sizeof(CompressedSample);
    
    if (compressedSize == 0) return 0;
    return (originalSize - compressedSize) * 100 / originalSize;
}

String LogicAnalyzer::getBufferModeString() const {
    switch (logicConfig.bufferMode) {
        case BUFFER_RAM: return "RAM";
        case BUFFER_FLASH: return "Flash";
        case BUFFER_STREAMING: return "Streaming";
        case BUFFER_COMPRESSED: return "Compressed";
        default: return "Unknown";
    }
}

float LogicAnalyzer::getFlashStorageUsedMB() const {
    return flashWritePosition / (1024.0 * 1024.0);
}

uint32_t LogicAnalyzer::getFlashSampleCount() const {
    return flashSamplesWritten;
}

void LogicAnalyzer::clearFlashLogicData() {
    if (flashDataFile) {
        flashDataFile.close();
    }
    
    if (LittleFS.exists(flashLogicFileName)) {
        LittleFS.remove(flashLogicFileName);
    }
    
    flashSamplesWritten = 0;
    flashWritePosition = 0;
    bufferPosition = 0;
    compressedCount = 0;
    
    addLogEntry("Flash logic data cleared");
}

void LogicAnalyzer::setBufferMode(BufferMode mode) {
    logicConfig.bufferMode = mode;
    
    switch (mode) {
        case BUFFER_FLASH:
            enableFlashBuffering(mode, FLASH_BUFFER_SIZE);
            break;
        case BUFFER_STREAMING:
            enableStreamingMode(true);
            break;
        case BUFFER_COMPRESSED:
            enableCompression(COMPRESS_HYBRID);
            break;
        default:
            break;
    }
}

String LogicAnalyzer::getAdvancedStatusJSON() const {
    JsonDocument doc;
    doc["buffer_mode"] = getBufferModeString();
    doc["compression_type"] = (int)logicConfig.compression;
    doc["flash_samples"] = flashSamplesWritten;
    doc["flash_storage_mb"] = getFlashStorageUsedMB();
    doc["streaming_active"] = streamingActive;
    doc["streaming_count"] = streamingCount;
    doc["compression_ratio"] = getCompressionRatio();
    doc["compressed_samples"] = compressedCount;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool LogicAnalyzer::isStreamingActive() const {
    return streamingActive;
}

uint32_t LogicAnalyzer::getStreamingSampleCount() const {
    return streamingCount;
}

void LogicAnalyzer::stopStreaming() {
    if (streamingActive) {
        flushFlashBuffer();
        if (flashDataFile) {
            flashDataFile.close();
        }
        streamingActive = false;
        addLogEntry("Streaming capture stopped - " + String(streamingCount) + " samples captured");
    }
}

BufferMode LogicAnalyzer::getBufferMode() const {
    return logicConfig.bufferMode;
}

String LogicAnalyzer::getCompressedDataAsJSON() {
    JsonDocument doc;
    JsonArray samples = doc["compressed_samples"].to<JsonArray>();
    
    for (uint32_t i = 0; i < compressedCount && i < 100; i++) {
        JsonObject sample = samples.add<JsonObject>();
        sample["timestamp"] = compressedBuffer[i].timestamp;
        sample["count"] = compressedBuffer[i].count;
        sample["data"] = compressedBuffer[i].data;
        sample["type"] = compressedBuffer[i].type;
    }
    
    doc["total_compressed"] = compressedCount;
    doc["compression_ratio"] = getCompressionRatio();
    doc["original_samples"] = flashSamplesWritten;
    
    String result;
    serializeJson(doc, result);
    return result;
}

void LogicAnalyzer::clearCompressedBuffer() {
    compressedCount = 0;
    runLength = 0;
    lastTimestamp = 0;
    lastData = false;
}

// Half-Duplex UART Communication Methods
void LogicAnalyzer::setupHalfDuplexPin(bool txMode) {
    if (txMode) {
        // Configure pin as output for TX mode
        pinMode(uartConfig.rxPin, OUTPUT);
        digitalWrite(uartConfig.rxPin, HIGH); // Idle high for UART
    } else {
        // Configure pin as input for RX mode
        pinMode(uartConfig.rxPin, INPUT);
    }
}

void LogicAnalyzer::processHalfDuplexQueue() {
    if (!halfDuplexTxQueue.isEmpty() && !halfDuplexBusy) {
        switchToTxMode();
        
        // Send the queued command
        if (uartSerial) {
            uartSerial->print(halfDuplexTxQueue);
            uartSerial->flush(); // Ensure all data is sent
        }
        
        // Log the transmitted data
        addUartEntry(halfDuplexTxQueue, false); // false = TX data
        uartBytesSent += halfDuplexTxQueue.length();
        
        halfDuplexTxQueue = "";
        halfDuplexTxTimeout = millis();
        halfDuplexBusy = true;
        
        addLogEntry("Half-duplex: Command sent, waiting for response");
    }
}

void LogicAnalyzer::switchToRxMode() {
    if (halfDuplexTxMode) {
        halfDuplexTxMode = false;
        halfDuplexBusy = false;
        
        // Reconfigure UART for RX mode
        if (uartSerial) {
            uartSerial->end();
            
            // Configure UART parameters for RX
            uint32_t config = SERIAL_8N1;
            if (uartConfig.dataBits == 7) {
                if (uartConfig.parity == 0) config = SERIAL_7N1;
                else if (uartConfig.parity == 1) config = SERIAL_7O1;
                else if (uartConfig.parity == 2) config = SERIAL_7E1;
                if (uartConfig.stopBits == 2) {
                    if (uartConfig.parity == 0) config = SERIAL_7N2;
                    else if (uartConfig.parity == 1) config = SERIAL_7O2;
                    else if (uartConfig.parity == 2) config = SERIAL_7E2;
                }
            } else if (uartConfig.dataBits == 8) {
                if (uartConfig.parity == 0) config = SERIAL_8N1;
                else if (uartConfig.parity == 1) config = SERIAL_8O1;
                else if (uartConfig.parity == 2) config = SERIAL_8E1;
                if (uartConfig.stopBits == 2) {
                    if (uartConfig.parity == 0) config = SERIAL_8N2;
                    else if (uartConfig.parity == 1) config = SERIAL_8O2;
                    else if (uartConfig.parity == 2) config = SERIAL_8E2;
                }
            }
            
            uartSerial->begin(uartConfig.baudrate, config, uartConfig.rxPin, -1);
        }
        
        setupHalfDuplexPin(false); // Configure as input
        addLogEntry("Half-duplex: Switched to RX mode");
    }
}

void LogicAnalyzer::switchToTxMode() {
    if (!halfDuplexTxMode) {
        halfDuplexTxMode = true;
        
        // Reconfigure UART for TX mode
        if (uartSerial) {
            uartSerial->end();
            
            // Configure UART parameters for TX
            uint32_t config = SERIAL_8N1;
            if (uartConfig.dataBits == 7) {
                if (uartConfig.parity == 0) config = SERIAL_7N1;
                else if (uartConfig.parity == 1) config = SERIAL_7O1;
                else if (uartConfig.parity == 2) config = SERIAL_7E1;
                if (uartConfig.stopBits == 2) {
                    if (uartConfig.parity == 0) config = SERIAL_7N2;
                    else if (uartConfig.parity == 1) config = SERIAL_7O2;
                    else if (uartConfig.parity == 2) config = SERIAL_7E2;
                }
            } else if (uartConfig.dataBits == 8) {
                if (uartConfig.parity == 0) config = SERIAL_8N1;
                else if (uartConfig.parity == 1) config = SERIAL_8O1;
                else if (uartConfig.parity == 2) config = SERIAL_8E1;
                if (uartConfig.stopBits == 2) {
                    if (uartConfig.parity == 0) config = SERIAL_8N2;
                    else if (uartConfig.parity == 1) config = SERIAL_8O2;
                    else if (uartConfig.parity == 2) config = SERIAL_8E2;
                }
            }
            
            uartSerial->begin(uartConfig.baudrate, config, -1, uartConfig.rxPin);
        }
        
        setupHalfDuplexPin(true); // Configure as output
        addLogEntry("Half-duplex: Switched to TX mode");
    }
}

bool LogicAnalyzer::sendHalfDuplexCommand(const String& command) {
    if (uartConfig.duplexMode != UART_HALF_DUPLEX) {
        addLogEntry("Error: Half-duplex command sent but not in half-duplex mode");
        return false;
    }
    
    if (halfDuplexBusy) {
        addLogEntry("Error: Half-duplex busy, command queued: " + command);
        halfDuplexTxQueue = command + "\r\n";  // Add line ending
        return false;
    }
    
    halfDuplexTxQueue = command + "\r\n";  // Add line ending
    addLogEntry("Half-duplex: Command queued - " + command);
    return true;
}

bool LogicAnalyzer::isHalfDuplexMode() const {
    return uartConfig.duplexMode == UART_HALF_DUPLEX;
}

bool LogicAnalyzer::isHalfDuplexBusy() const {
    return halfDuplexBusy;
}

String LogicAnalyzer::getHalfDuplexStatus() const {
    JsonDocument doc;
    doc["mode"] = (uartConfig.duplexMode == UART_HALF_DUPLEX) ? "Half" : "Full";
    doc["busy"] = halfDuplexBusy;
    doc["tx_mode"] = halfDuplexTxMode;
    doc["queue_length"] = halfDuplexTxQueue.length();
    doc["timeout"] = halfDuplexTxTimeout;
    
    String result;
    serializeJson(doc, result);
    return result;
}

#endif
