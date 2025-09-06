#include "logic_analyzer.h"

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
    
    // Flash Storage initialization
    useFlashStorage = false;
    uartLogFileName = "/uart_logs.txt";
    
    sampleInterval = 1000000 / sampleRate; // microseconds
}

LogicAnalyzer::~LogicAnalyzer() {
    stopCapture();
}

void LogicAnalyzer::begin() {
    Serial.println("Initializing GPIO1 Logic Analyzer...");
    initializeGPIO1();
    clearBuffer();
    
    // Initialize LittleFS for potential flash storage
    initFlashStorage();
    
    Serial.printf("GPIO1 Logic Analyzer initialized at %d Hz with %d sample buffer\n", sampleRate, BUFFER_SIZE);
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
    Serial.println("=== GPIO1 Logic Analyzer Status ===");
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
    // Gemini-style dark gradient background (deep navy to dark purple)
    display.fillScreen(0x0841);  // Very dark navy base
    drawGradientBackground();
    drawHeader();
    drawInitialStatus();
}

void LogicAnalyzer::updateDisplay() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // Update display every 300ms for smoother updates
    if (now - lastUpdate > 300) {
        drawStatusSection();
        drawChannelSection();
        drawBottomSection();
        lastUpdate = now;
    }
}

// Gemini-Style Display Functions with Dark Theme
void LogicAnalyzer::drawGradientBackground() {
    // Gemini dark gradient from top to bottom (dark navy to dark purple)
    for(int i = 0; i < 128; i++) {
        uint16_t gradColor = display.color565(8 + i/16, 4 + i/32, 16 + i/8);
        display.drawLine(0, i, 128, i, gradColor);
    }
}

void LogicAnalyzer::drawHeader() {
    // Dark header area with subtle gradient
    for(int i = 0; i < 22; i++) {
        uint16_t gradColor = display.color565(12 + i, 8 + i/2, 20 + i*2);
        display.drawLine(0, i, 128, i, gradColor);
    }
    
    // Center-aligned title
    display.setTextColor(0xDEFB);  // Light blue-white text
    display.setTextSize(1);
    display.setCursor(26, 5);  // Centered position
    display.print("GPIO1 ANALYZER");
    
    // Left status indicator
    display.fillCircle(8, 11, 4, capturing ? 0xF800 : 0x4CAF);  // Red when capturing, blue when ready
    display.fillCircle(8, 11, 2, 0xFFFF);  // White highlight
    
    // Right WiFi indicator (will be updated by WiFi status function)
    display.fillCircle(120, 11, 4, 0x2104);  // Will be updated with actual status
    display.fillCircle(120, 11, 2, 0xFFFF);
}

void LogicAnalyzer::drawInitialStatus() {
    // Main Status Card - Clean and centered
    drawGeminiCard(8, 26, 112, 26, 0x2104, 0x4208, "");
    
    // Large centered status
    display.setTextColor(0xF7BE);
    display.setTextSize(1);
    display.setCursor(52, 32);  // Centered
    display.print("READY");
    
    // Right-aligned buffer info
    display.setTextColor(0xDEFB);
    display.setCursor(12, 42);
    display.print("Buffer: 16K");
    
    // Right-aligned max rate
    display.setCursor(72, 42);
    display.print("Max: 10MHz");
    
    // WiFi Status - compact horizontal layout
    drawGeminiCard(8, 56, 54, 18, 0x10A2, 0x2945, "");
    display.setTextColor(0xDEFB);
    display.setCursor(20, 64);  // Centered
    display.print("WiFi");
    
    // GPIO Status - compact with Gemini colors
    drawGeminiCard(66, 56, 54, 18, 0x2104, 0x52AA, "");
    display.setTextColor(0xFFFF);
    display.setCursor(75, 64);  // Centered
    display.print("GPIO1");
    
    // Purple progress bar placeholder
    display.drawRoundRect(8, 82, 112, 8, 4, 0x52AA);  // Purple border
    display.fillRoundRect(10, 84, 108, 4, 2, 0x52AA);  // Purple fill
}

void LogicAnalyzer::drawStatusSection() {
    // Main Status Card - larger and centered
    uint16_t statusGrad1, statusGrad2;
    if (capturing) {
        // Pulsing red gradient when capturing
        static uint8_t pulse = 0;
        pulse += 10;
        uint8_t intensity = 80 + (sin(pulse * 0.1) * 30);
        statusGrad1 = display.color565(intensity, 15, 15);
        statusGrad2 = display.color565(intensity + 40, intensity/3, 10);
    } else {
        // Calm blue gradient when ready
        statusGrad1 = 0x2104;
        statusGrad2 = 0x4208;
    }
    
    drawGeminiCard(8, 26, 112, 26, statusGrad1, statusGrad2, "");
    
    // Large centered status text
    display.setTextColor(0xF7BE);
    display.setTextSize(1);
    const char* statusText = capturing ? "CAPTURING" : "READY";
    int textWidth = strlen(statusText) * 6;  // Approximate text width
    display.setCursor((128 - textWidth) / 2, 32);
    display.print(statusText);
    
    // Sample rate - centered on second line
    display.setTextColor(0xDEFB);
    display.setCursor(12, 42);
    display.print("Rate: ");
    if (sampleRate >= 1000000) {
        display.printf("%.1fMHz", sampleRate/1000000.0);
    } else if (sampleRate >= 1000) {
        display.printf("%.1fkHz", sampleRate/1000.0);
    } else {
        display.printf("%dHz", sampleRate);
    }
    
    // Buffer usage - right side
    uint16_t bufUsage = getBufferUsage();
    float percentage = (bufUsage * 100.0) / BUFFER_SIZE;
    display.setCursor(70, 42);
    display.printf("Buf: %.0f%%", percentage);
}

void LogicAnalyzer::drawChannelSection() {
    bool gpio1State = readGPIO1();
    
    // GPIO1 Status with Gemini color scheme
    uint16_t gpioGrad1, gpioGrad2;
    if (gpio1State) {
        // Bright blue gradient for HIGH (active state)
        gpioGrad1 = 0x2945;  // Dark blue-purple
        gpioGrad2 = 0x4CAF;  // Bright Gemini blue
    } else {
        // Muted purple gradient for LOW (inactive state)
        gpioGrad1 = 0x2104;  // Very dark blue
        gpioGrad2 = 0x52AA;  // Muted purple
    }
    
    drawGeminiCard(6, 85, 58, 20, gpioGrad1, gpioGrad2, "");
    display.setTextColor(0xFFFF);
    display.setTextSize(1);
    display.setCursor(12, 92);
    display.print(gpio1State ? "HIGH" : "LOW");
    
    // Buffer status with modern Gemini design
    uint16_t bufUsage = getBufferUsage();
    float percentage = (bufUsage * 100.0) / BUFFER_SIZE;
    
    // Buffer percentage card without title
    uint16_t bufColor1 = 0x1E3A, bufColor2 = 0x4A6F;
    drawGeminiCard(68, 85, 56, 20, bufColor1, bufColor2, "");
    display.setTextColor(0xE7FF);
    display.setTextSize(1);
    display.setCursor(74, 92);
    display.printf("%.0f%%", percentage);
    
    // Purple progress bar
    display.fillRoundRect(6, 110, 116, 8, 4, 0x2104);  // Dark background
    if (bufUsage > 0) {
        int barWidth = (bufUsage * 112) / BUFFER_SIZE;
        
        // Simple purple bar
        display.fillRoundRect(8, 111, barWidth, 6, 3, 0x52AA);  // Purple color
        
        // Subtle glow border
        if (barWidth > 2) {
            display.drawRoundRect(7, 110, barWidth + 2, 8, 4, 0x52AA);
        }
    }
}

void LogicAnalyzer::drawBottomSection() {
    // Optional trigger info in the existing bottom area - only if space allows
    if (triggerMode != TRIGGER_NONE) {
        display.setTextColor(0xFD20, 0x0841);  // Dark background
        display.setTextSize(1);
        display.setCursor(85, 87);  // Right side of GPIO1 area
        display.printf("T:%s", triggerArmed ? "ARM" : "WAIT");
    }
    
    // Bottom border line with Gemini accent color
    display.drawLine(0, 127, 128, 127, 0x4A69);
}

void LogicAnalyzer::drawCard(int x, int y, int w, int h, uint16_t color, const char* title) {
    // Legacy card function - kept for compatibility
    display.fillRect(x, y, w, h, color);
    display.drawRect(x, y, w, h, 0x39E7);
    
    if (title) {
        display.setTextColor(0xFFFF, color);
        display.setTextSize(1);
        display.setCursor(x + 2, y + 2);
        display.print(title);
    }
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

// WiFi status integration functions
void LogicAnalyzer::updateWiFiStatus(bool isConnected, bool isAP, const String& networkName, const String& ipAddress) {
    static bool lastConnected = false;
    static bool lastAP = false;
    static String lastNetwork = "";
    static String lastIP = "";
    
    // Only update if something changed
    if (isConnected != lastConnected || isAP != lastAP || 
        networkName != lastNetwork || ipAddress != lastIP) {
        
        // Clear WiFi status area with dark background
        display.fillRect(4, 32, 120, 20, 0x0841);
        
        // WiFi status indicator with dark theme colors
        uint16_t statusColor;
        const char* statusText;
        
        if (isConnected) {
            statusColor = 0x4CAF;  // Blue (Gemini accent)
            statusText = "WiFi";
            display.fillCircle(10, 36, 3, 0x4CAF);  // Blue dot
        } else if (isAP) {
            statusColor = 0x52AA;  // Purple (Gemini secondary)  
            statusText = "AP";
            display.fillCircle(10, 36, 3, 0x52AA);  // Purple dot
        } else {
            statusColor = 0x8410;  // Dark red
            statusText = "No WiFi";
            display.fillCircle(10, 36, 3, 0x8410);  // Dark red dot
        }
        
        display.setTextColor(statusColor, 0x0841);
        display.setTextSize(1);
        display.setCursor(20, 33);
        display.print(statusText);
        
        // Network name (truncated if too long) with light text
        display.setTextColor(0xDEFB, 0x0841);
        display.setCursor(6, 42);
        String truncatedNetwork = networkName;
        if (truncatedNetwork.length() > 16) {
            truncatedNetwork = truncatedNetwork.substring(0, 13) + "...";
        }
        display.print(truncatedNetwork);
        
        // Update cached values
        lastConnected = isConnected;
        lastAP = isAP;
        lastNetwork = networkName;
        lastIP = ipAddress;
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
    String result = "# AtomS3 Logic Analyzer - Serial Logs\n";
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
    String result = "# AtomS3 Logic Analyzer - GPIO1 Capture Data (CSV Format)\n";
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
    String result = "# AtomS3 Logic Analyzer - UART Communication Logs\n";
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
