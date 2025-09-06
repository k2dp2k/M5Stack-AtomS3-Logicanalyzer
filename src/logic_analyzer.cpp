#include "logic_analyzer.h"
#include <WiFi.h>
#include <cmath>
#ifdef ATOMS3_BUILD
    extern M5GFX& display;
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
    
    // Flash Storage initialization - Enable by default to use 8MB Flash
    useFlashStorage = true;
    uartLogFileName = "/uart_logs.txt";
    
    sampleInterval = 1000000 / sampleRate; // microseconds
}

LogicAnalyzer::~LogicAnalyzer() {
    stopCapture();
}

void LogicAnalyzer::begin() {
    Serial.println("Initializing AtomProbe GPIO1 Monitor...");
    initializeGPIO1();
    clearBuffer();
    
    // Initialize LittleFS for potential flash storage
    initFlashStorage();
    
    Serial.printf("AtomProbe GPIO1 Monitor initialized at %d Hz with %d sample buffer\\n", sampleRate, BUFFER_SIZE);
}

void LogicAnalyzer::initializeGPIO1() {
    pinMode(gpio1Pin, INPUT);
    Serial.printf("GPIO1 Pin: %d configured as input\n", gpio1Pin);
}

void LogicAnalyzer::process() {
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
    return digitalRead(gpio1Pin);
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
    buffer[writeIndex].timestamp = micros();
    buffer[writeIndex].data = data;
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;
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
    addLogEntry(String("Capture stopped. Buffer: ") + getBufferUsage() + "/" + BUFFER_SIZE);
    Serial.println("Capture stopped");
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
    
    uint16_t count = getBufferUsage();
    uint16_t index = readIndex;
    
    for (uint16_t i = 0; i < count; i++) {
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
}

uint16_t LogicAnalyzer::getBufferUsage() const {
    if (writeIndex >= readIndex) {
        return writeIndex - readIndex;
    } else {
        return (BUFFER_SIZE - readIndex) + writeIndex;
    }
}

bool LogicAnalyzer::isBufferFull() const {
    return getBufferUsage() >= (BUFFER_SIZE - 1);
}

void LogicAnalyzer::printStatus() {
    Serial.println("=== AtomProbe GPIO1 Monitor Status ===");
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
    display.setCursor(31, 95);
    display.print("AtomS3");
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
        display.print("AtomS3_AtomProbe");
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
    String result = "# AtomS3 AtomProbe - Serial Logs\\n";
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
    String result = "# AtomS3 AtomProbe - GPIO1 Capture Data (CSV Format)\\n";
    result += "# Generated: " + String(millis()) + "ms\n";
    result += "# Sample Rate: " + String(sampleRate) + " Hz\n";
    result += "# GPIO Pin: " + String(gpio1Pin) + "\n";
    result += "# Buffer Size: " + String(BUFFER_SIZE) + " samples\n";
    result += "# Buffer Usage: " + String(getBufferUsage()) + "/" + String(BUFFER_SIZE) + 
                " (" + String((getBufferUsage() * 100.0) / BUFFER_SIZE, 1) + "%)\n";
    result += "# Trigger Mode: " + String((int)triggerMode) + "\n\n";
    
    // CSV Header - optimized for single GPIO1 channel
    result += "Sample,Timestamp_us,GPIO1_Digital,GPIO1_State\n";
    
    uint16_t count = getBufferUsage();
    uint16_t index = readIndex;
    
    for (uint16_t i = 0; i < count; i++) {
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

// UART Monitoring Functions
void LogicAnalyzer::configureUart(uint32_t baudrate, uint8_t dataBits, uint8_t parity, uint8_t stopBits, uint8_t rxPin, uint8_t txPin) {
    uartConfig.baudrate = baudrate;
    uartConfig.dataBits = dataBits;
    uartConfig.parity = parity;
    uartConfig.stopBits = stopBits;
    uartConfig.rxPin = rxPin;
    uartConfig.txPin = txPin;
    
    saveUartConfig();
    
    String configMsg = "UART configured: " + String(baudrate) + " baud, " + String(dataBits) + 
                       String(parity == 0 ? "N" : (parity == 1 ? "O" : "E")) + String(stopBits) +
                       ", RX:" + String(rxPin) + ", TX:" + String(txPin);
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
    
    uartSerial->begin(uartConfig.baudrate, config, uartConfig.rxPin, uartConfig.txPin);
    uartMonitoringEnabled = true;
    uartConfig.enabled = true;
    uartRxBuffer = "";
    uartTxBuffer = "";
    lastUartActivity = millis();
    uartBytesReceived = 0;
    uartBytesSent = 0;
    
    String enableMsg = "UART monitoring enabled on RX:" + String(uartConfig.rxPin) + ", TX:" + String(uartConfig.txPin) + 
                       " @ " + String(uartConfig.baudrate) + " baud";
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
        preferences->putUChar("uart_tx_pin", uartConfig.txPin);
        preferences->putBool("uart_enabled", uartConfig.enabled);
        
        String configMsg = "UART config saved: " + String(uartConfig.baudrate) + " baud, " + 
                           String(uartConfig.dataBits) + 
                           String(uartConfig.parity == 0 ? "N" : (uartConfig.parity == 1 ? "O" : "E")) + 
                           String(uartConfig.stopBits) + ", RX:" + String(uartConfig.rxPin) + ", TX:" + String(uartConfig.txPin);
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
        uartConfig.enabled = preferences->getBool("uart_enabled", false);
        
        String configMsg = "UART config loaded: " + String(uartConfig.baudrate) + " baud, " + 
                           String(uartConfig.dataBits) + 
                           String(uartConfig.parity == 0 ? "N" : (uartConfig.parity == 1 ? "O" : "E")) + 
                           String(uartConfig.stopBits) + ", RX:" + String(uartConfig.rxPin) + ", TX:" + String(uartConfig.txPin);
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

#endif
