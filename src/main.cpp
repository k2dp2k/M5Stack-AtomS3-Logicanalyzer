#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "logic_analyzer.h"

#ifdef ATOMS3_BUILD
    #include <M5AtomS3.h>
    M5GFX& display = AtomS3.Display;
#endif

// Access Point Configuration
const char* ap_ssid = "M5Stack-AtomProbe";
const char* ap_password = "probe123";  // At least 8 characters

// WiFi Configuration
Preferences preferences;
String saved_ssid = "";
String saved_password = "";
bool wifi_connected = false;
bool ap_mode = false;
const int WIFI_CONNECT_TIMEOUT = 15000;  // 15 seconds
const unsigned long WIFI_DISCONNECT_TIMEOUT = 30000;  // 30 seconds
unsigned long last_wifi_connected_time = 0;
bool wifi_monitoring_active = false;

// Web server on port 80
AsyncWebServer server(80);

// Logic analyzer instance
LogicAnalyzer analyzer;

// Function declarations
void setupWebServer();
String getIndexHTML();
String getConfigHTML();
void loadWiFiCredentials();
void saveWiFiCredentials(const String& ssid, const String& password);
bool connectToWiFi();
void startAPMode();
void checkWiFiConnection();
void handleWiFiReconnection();
String getWiFiStatus();

void setup() {
#ifdef ATOMS3_BUILD
    // Initialize M5AtomS3
    auto cfg = M5.config();
    AtomS3.begin(cfg);
    display = AtomS3.Display;
    
    Serial.println("M5Stack AtomProbe Starting...");
    
    // Initialize logic analyzer
    analyzer.begin();
    analyzer.initDisplay();
    
    // Show startup logo
    analyzer.drawStartupLogo();
    delay(3000);  // Show logo for 3 seconds
#else
    Serial.begin(115200);
    Serial.println("ESP32 AtomProbe Starting...");
    
    // Initialize logic analyzer
    analyzer.begin();
#endif
    
    // Initialize preferences
    preferences.begin("atomprobe", false);
    
    // Pass preferences instance to analyzer
    analyzer.setPreferences(&preferences);
    
    // Load saved WiFi credentials
    loadWiFiCredentials();
    
    // Load configurations
    analyzer.loadLogicConfig();
    analyzer.loadUartConfig();
    
    // Ensure proper defaults are set
    analyzer.addLogEntry("Logic Analyzer initialized with defaults");
    
    // Try to connect to saved WiFi first
    bool connected = false;
    if (saved_ssid.length() > 0) {
        analyzer.addLogEntry("Trying to connect to saved WiFi: " + saved_ssid);
        connected = connectToWiFi();
    }
    
    // If WiFi connection failed or no saved credentials, start AP mode
    if (!connected) {
        analyzer.addLogEntry("Starting Access Point mode...");
        startAPMode();
        analyzer.setAPMode(true);
    } else {
        analyzer.setAPMode(false);
        wifi_connected = true;
        last_wifi_connected_time = millis();
        wifi_monitoring_active = true;
    }
    
    // Setup web server routes
    setupWebServer();
    
    // Start server
    server.begin();
    analyzer.addLogEntry("Web server started");
    Serial.println("Web server started");
    Serial.println(getWiFiStatus());
}

void loop() {
#ifdef ATOMS3_BUILD
    // Update M5AtomS3
    AtomS3.update();
    
    // Handle button press (switch display pages)
    if (AtomS3.BtnA.wasPressed()) {
        analyzer.switchPage();
    }
    
    // Main logic analyzer processing
    analyzer.process();
    
    // Update display
    analyzer.updateDisplay();
    
    // WiFi connection monitoring
    checkWiFiConnection();
#else
    // Main logic analyzer processing
    analyzer.process();
    
    // WiFi connection monitoring (non-AtomS3)
    checkWiFiConnection();
#endif
    
    // Small delay to prevent watchdog issues
    delay(1);
}

void setupWebServer() {
    // Serve static files
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", getIndexHTML());
    });
    
    // WiFi configuration page (only available in AP mode)
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", getConfigHTML());
    });
    
    // API endpoints
    server.on("/api/start", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.startCapture();
        request->send(200, "application/json", "{\"status\":\"started\"}");
    });
    
    server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.stopCapture();
        request->send(200, "application/json", "{\"status\":\"stopped\"}");
    });
    
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String data = analyzer.getDataAsJSON();
        request->send(200, "application/json", data);
    });
    
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["capturing"] = analyzer.isCapturing();
        doc["sample_rate"] = analyzer.getSampleRate();
        doc["gpio_pin"] = 1;  // GPIO1 only
        doc["buffer_usage"] = analyzer.getBufferUsage();
        doc["buffer_size"] = analyzer.getCurrentBufferSize();
        doc["wifi_connected"] = wifi_connected;
        doc["ap_mode"] = ap_mode;
        doc["wifi_ssid"] = wifi_connected ? WiFi.SSID() : (ap_mode ? String(ap_ssid) : "");
        doc["ip_address"] = wifi_connected ? WiFi.localIP().toString() : (ap_mode ? WiFi.softAPIP().toString() : "");
#ifdef ATOMS3_BUILD
        doc["device"] = "AtomS3";
        doc["display"] = "enabled";
#else
        doc["device"] = "ESP32";
        doc["display"] = "none";
#endif
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Serial logs endpoint
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        String logs = analyzer.getLogsAsJSON();
        request->send(200, "application/json", logs);
    });
    
    // Clear logs endpoint
    server.on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.clearLogs();
        request->send(200, "application/json", "{\"status\":\"cleared\"}");
    });
    
    // UART monitoring endpoints
    server.on("/api/uart/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        String uartLogs = analyzer.getUartLogsAsJSON();
        request->send(200, "application/json", uartLogs);
    });
    
    server.on("/api/uart/enable", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.enableUartMonitoring();
        request->send(200, "application/json", "{\"status\":\"enabled\",\"message\":\"UART monitoring started\"}");
    });
    
    // Logic Analyzer configuration endpoints
    server.on("/api/logic/config", HTTP_GET, [](AsyncWebServerRequest *request){
        String config = analyzer.getLogicConfigAsJSON();
        request->send(200, "application/json", config);
    });
    
    server.on("/api/logic/config", HTTP_POST, [](AsyncWebServerRequest *request){
        uint32_t sampleRate = DEFAULT_SAMPLE_RATE;
        uint8_t gpioPin = CHANNEL_0_PIN;
        uint8_t triggerMode = TRIGGER_NONE;
        uint32_t bufferSize = BUFFER_SIZE;
        uint8_t preTriggerPercent = 10;
        
        if (request->hasParam("sample_rate", true)) {
            sampleRate = request->getParam("sample_rate", true)->value().toInt();
        }
        if (request->hasParam("gpio_pin", true)) {
            gpioPin = request->getParam("gpio_pin", true)->value().toInt();
        }
        if (request->hasParam("trigger_mode", true)) {
            triggerMode = request->getParam("trigger_mode", true)->value().toInt();
        }
        if (request->hasParam("buffer_size", true)) {
            bufferSize = request->getParam("buffer_size", true)->value().toInt();
        }
        if (request->hasParam("pre_trigger_percent", true)) {
            preTriggerPercent = request->getParam("pre_trigger_percent", true)->value().toInt();
        }
        
        // Handle new parameters for advanced modes
        uint8_t bufferMode = 1; // Default to Flash (BUFFER_FLASH)
        uint8_t compression = 0; // No compression
        uint32_t flashSamples = FLASH_BUFFER_SIZE;
        
        if (request->hasParam("buffer_mode", true)) {
            bufferMode = request->getParam("buffer_mode", true)->value().toInt();
        }
        if (request->hasParam("compression", true)) {
            compression = request->getParam("compression", true)->value().toInt();
        }
        if (request->hasParam("flash_samples", true)) {
            flashSamples = request->getParam("flash_samples", true)->value().toInt();
        }
        
        analyzer.configureLogic(sampleRate, gpioPin, (TriggerMode)triggerMode, bufferSize, preTriggerPercent);
        
        // Configure advanced modes
        analyzer.setBufferMode((BufferMode)bufferMode);
        if (compression > 0) {
            analyzer.enableCompression((CompressionType)compression);
        }
        if (bufferMode == BUFFER_FLASH || bufferMode == BUFFER_STREAMING) {
            analyzer.enableFlashBuffering((BufferMode)bufferMode, flashSamples);
        }
        
        request->send(200, "application/json", "{\"status\":\"configured\"}");
    });
    
    // UART configuration endpoint (POST)
    server.on("/api/uart/config", HTTP_POST, [](AsyncWebServerRequest *request){
        uint32_t baudrate = 115200;
        uint8_t dataBits = 8;
        uint8_t parity = 0;
        uint8_t stopBits = 1;
        uint8_t rxPin = 7;
        int8_t txPin = -1;
        UartDuplexMode duplexMode = UART_FULL_DUPLEX;  // Default to full duplex
        
        if (request->hasParam("baudrate", true)) {
            baudrate = request->getParam("baudrate", true)->value().toInt();
        }
        if (request->hasParam("data_bits", true)) {
            dataBits = request->getParam("data_bits", true)->value().toInt();
        }
        if (request->hasParam("parity", true)) {
            parity = request->getParam("parity", true)->value().toInt();
        }
        if (request->hasParam("stop_bits", true)) {
            stopBits = request->getParam("stop_bits", true)->value().toInt();
        }
        if (request->hasParam("rx_pin", true)) {
            rxPin = request->getParam("rx_pin", true)->value().toInt();
        }
        if (request->hasParam("tx_pin", true)) {
            txPin = request->getParam("tx_pin", true)->value().toInt();
        }
        if (request->hasParam("duplex_mode", true)) {
            uint8_t duplexValue = request->getParam("duplex_mode", true)->value().toInt();
            duplexMode = (duplexValue == 1) ? UART_HALF_DUPLEX : UART_FULL_DUPLEX;
        }
        
        analyzer.configureUart(baudrate, dataBits, parity, stopBits, rxPin, txPin, duplexMode);
        request->send(200, "application/json", "{\"status\":\"configured\",\"message\":\"UART settings updated\"}");
    });
    
    // Get UART configuration
    server.on("/api/uart/config", HTTP_GET, [](AsyncWebServerRequest *request){
        String config = analyzer.getUartConfigAsJSON();
        request->send(200, "application/json", config);
    });
    
    server.on("/api/uart/disable", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.disableUartMonitoring();
        request->send(200, "application/json", "{\"status\":\"disabled\",\"message\":\"UART monitoring stopped\"}");
    });
    
    server.on("/api/uart/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.clearUartLogs();
        request->send(200, "application/json", "{\"status\":\"cleared\",\"message\":\"UART logs cleared\"}");
    });
    
    // UART buffer management
    server.on("/api/uart/compact", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.compactUartLogs();
        request->send(200, "application/json", "{\"status\":\"compacted\",\"message\":\"UART buffer compacted\"}");
    });
    
    server.on("/api/uart/stats", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["count"] = analyzer.getUartLogCount();
        doc["memory_usage"] = analyzer.getUartMemoryUsage();
        doc["buffer_full"] = analyzer.isUartBufferFull();
        doc["max_entries"] = analyzer.getMaxUartEntries();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Set UART buffer size with auto Flash/RAM selection
    server.on("/api/uart/buffersize", HTTP_POST, [](AsyncWebServerRequest *request){
        size_t newSize = 10000;  // Default
        if (request->hasParam("size", true)) {
            newSize = request->getParam("size", true)->value().toInt();
        }
        
        // Limit buffer size to prevent memory issues
        if (newSize < 100) newSize = 100;
        if (newSize > 1000000) newSize = 1000000;  // Allow up to 1M with Flash - utilize full 8MB
        
        // Auto-select storage type based on buffer size
        bool shouldUseFlash = (newSize > 5000);
        if (shouldUseFlash != analyzer.isFlashStorageEnabled()) {
            analyzer.enableFlashStorage(shouldUseFlash);
        }
        
        analyzer.setUartBufferSize(newSize);
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["new_size"] = newSize;
        doc["storage_type"] = analyzer.isFlashStorageEnabled() ? "Flash" : "RAM";
        doc["auto_switched"] = (shouldUseFlash != analyzer.isFlashStorageEnabled()) ? false : true;
        doc["message"] = "UART buffer size updated to " + String(newSize) + " entries (" + 
                        String(analyzer.isFlashStorageEnabled() ? "Flash" : "RAM") + " storage)";
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Flash Storage control endpoints
    server.on("/api/uart/storage", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["storage_type"] = analyzer.isFlashStorageEnabled() ? "Flash" : "RAM";
        doc["flash_enabled"] = analyzer.isFlashStorageEnabled();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/api/uart/storage/flash", HTTP_POST, [](AsyncWebServerRequest *request){
        bool enable = true;
        if (request->hasParam("enable", true)) {
            enable = (request->getParam("enable", true)->value() == "true");
        }
        
        analyzer.enableFlashStorage(enable);
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["storage_type"] = analyzer.isFlashStorageEnabled() ? "Flash" : "RAM";
        doc["message"] = "Storage switched to " + String(analyzer.isFlashStorageEnabled() ? "Flash" : "RAM");
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Clear buffer data endpoint
    server.on("/api/data/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.clearBuffer();
        analyzer.clearFlashLogicData(); // Also clear flash data
        analyzer.addLogEntry("Capture buffer cleared by user");
        request->send(200, "application/json", "{\"status\":\"cleared\",\"message\":\"Buffer cleared\"}");
    });
    
    // === ADVANCED LOGIC ANALYZER ENDPOINTS ===
    
    // Advanced status endpoint
    server.on("/api/logic/advanced-status", HTTP_GET, [](AsyncWebServerRequest *request){
        String status = analyzer.getAdvancedStatusJSON();
        request->send(200, "application/json", status);
    });
    
    // Flash data endpoint
    server.on("/api/logic/flash-data", HTTP_GET, [](AsyncWebServerRequest *request){
        uint32_t offset = 0;
        uint32_t count = 1000;
        
        if (request->hasParam("offset")) {
            offset = request->getParam("offset")->value().toInt();
        }
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
        }
        
        String flashData = analyzer.getFlashDataAsJSON(offset, count);
        request->send(200, "application/json", flashData);
    });
    
    // Set buffer mode endpoint
    server.on("/api/logic/buffer-mode", HTTP_POST, [](AsyncWebServerRequest *request){
        uint8_t mode = 0; // Default RAM
        uint32_t flashSamples = FLASH_BUFFER_SIZE;
        
        if (request->hasParam("mode", true)) {
            mode = request->getParam("mode", true)->value().toInt();
        }
        if (request->hasParam("flash_samples", true)) {
            flashSamples = request->getParam("flash_samples", true)->value().toInt();
        }
        
        analyzer.setBufferMode((BufferMode)mode);
        if (mode == BUFFER_FLASH || mode == BUFFER_STREAMING) {
            analyzer.enableFlashBuffering((BufferMode)mode, flashSamples);
        }
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["buffer_mode"] = analyzer.getBufferModeString();
        doc["flash_samples"] = flashSamples;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Enable compression endpoint
    server.on("/api/logic/compression", HTTP_POST, [](AsyncWebServerRequest *request){
        uint8_t compressionType = 0; // No compression
        
        if (request->hasParam("type", true)) {
            compressionType = request->getParam("type", true)->value().toInt();
        }
        
        analyzer.enableCompression((CompressionType)compressionType);
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["compression_type"] = compressionType;
        doc["compression_name"] = (compressionType == 1 ? "RLE" : 
                                   compressionType == 2 ? "Delta" : 
                                   compressionType == 3 ? "Hybrid" : "None");
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Streaming control endpoint
    server.on("/api/logic/streaming", HTTP_POST, [](AsyncWebServerRequest *request){
        bool enable = false;
        
        if (request->hasParam("enable", true)) {
            enable = (request->getParam("enable", true)->value() == "true");
        }
        
        analyzer.enableStreamingMode(enable);
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["streaming_active"] = enable;
        doc["streaming_count"] = analyzer.getStreamingSampleCount();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Flash storage stats endpoint
    server.on("/api/logic/flash-stats", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["flash_samples"] = analyzer.getFlashSampleCount();
        doc["flash_storage_mb"] = analyzer.getFlashStorageUsedMB();
        doc["compression_ratio"] = analyzer.getCompressionRatio();
        doc["buffer_mode"] = analyzer.getBufferModeString();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // === HALF-DUPLEX UART ENDPOINTS ===
    
    // Send half-duplex command endpoint
    server.on("/api/uart/send", HTTP_POST, [](AsyncWebServerRequest *request){
        String command = "";
        
        if (request->hasParam("command", true)) {
            command = request->getParam("command", true)->value();
        }
        
        if (command.length() == 0) {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Command is required\"}");
            return;
        }
        
        bool success = analyzer.sendHalfDuplexCommand(command);
        
        JsonDocument doc;
        doc["status"] = success ? "queued" : "error";
        doc["command"] = command;
        doc["message"] = success ? "Command queued for transmission" : "Failed to queue command (half-duplex busy)";
        doc["is_half_duplex"] = analyzer.isHalfDuplexMode();
        doc["busy"] = analyzer.isHalfDuplexBusy();
        
        String response;
        serializeJson(doc, response);
        request->send(success ? 200 : 409, "application/json", response);  // 409 = Conflict if busy
    });
    
    // Get half-duplex status endpoint
    server.on("/api/uart/half-duplex-status", HTTP_GET, [](AsyncWebServerRequest *request){
        String status = analyzer.getHalfDuplexStatus();
        request->send(200, "application/json", status);
    });
    
    // === DUAL-MODE MONITORING ENDPOINTS ===
    
    // Enable/disable dual-mode monitoring
    server.on("/api/dual-mode", HTTP_POST, [](AsyncWebServerRequest *request){
        bool enable = false;
        
        if (request->hasParam("enable", true)) {
            enable = (request->getParam("enable", true)->value() == "true");
        }
        
        analyzer.enableDualMode(enable);
        
        JsonDocument doc;
        doc["status"] = "updated";
        doc["dual_mode_active"] = analyzer.isDualModeActive();
        doc["compatible"] = enable ? "pins match" : "disabled";
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Get dual-mode status
    server.on("/api/dual-mode/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String status = analyzer.getDualModeStatus();
        request->send(200, "application/json", status);
    });
    
    // WiFi configuration endpoint
    server.on("/api/wifi/config", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid = "";
        String password = "";
        
        if (request->hasParam("ssid", true)) {
            ssid = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("password", true)) {
            password = request->getParam("password", true)->value();
        }
        
        if (ssid.length() > 0) {
            saveWiFiCredentials(ssid, password);
            analyzer.addLogEntry("WiFi credentials saved. Restarting...");
            request->send(200, "application/json", "{\"status\":\"saved\",\"message\":\"Restarting to connect to WiFi...\"}");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID is required\"}");
        }
    });
    
    // Switch to AP mode endpoint
    server.on("/api/wifi/ap", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.addLogEntry("Switching to AP mode. Restarting...");
        // Clear saved credentials to force AP mode
        preferences.remove("wifi_ssid");
        preferences.remove("wifi_password");
        request->send(200, "application/json", "{\"status\":\"switching\",\"message\":\"Switching to AP mode...\"}");
        delay(1000);
        ESP.restart();
    });
    
    // Download endpoints
    server.on("/download/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        String logs = analyzer.getLogsAsPlainText();
        String timestamp = String(millis());
        String filename = "m5stack-atomprobe_logs_" + timestamp + ".txt";
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", logs);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        response->addHeader("Content-Type", "text/plain; charset=utf-8");
        request->send(response);
        
        analyzer.addLogEntry("Serial logs downloaded as " + filename);
    });
    
    server.on("/download/uart", HTTP_GET, [](AsyncWebServerRequest *request){
        String uartLogs = analyzer.getUartLogsAsPlainText();
        String timestamp = String(millis());
        String filename = "m5stack-atomprobe_uart_" + timestamp + ".txt";
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", uartLogs);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        response->addHeader("Content-Type", "text/plain; charset=utf-8");
        request->send(response);
        
        analyzer.addLogEntry("UART logs downloaded as " + filename);
    });
    
    server.on("/download/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String format = "json";  // Default format
        if (request->hasParam("format")) {
            format = request->getParam("format")->value();
        }
        
        String timestamp = String(millis());
        String data;
        String filename;
        String contentType;
        
        if (format == "csv") {
            data = analyzer.getDataAsCSV();
            filename = "m5stack-atomprobe_capture_" + timestamp + ".csv";
            contentType = "text/csv";
        } else {
            data = analyzer.getDataAsJSON();
            filename = "m5stack-atomprobe_capture_" + timestamp + ".json";
            contentType = "application/json";
        }
        
        AsyncWebServerResponse *response = request->beginResponse(200, contentType, data);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        response->addHeader("Content-Type", contentType + "; charset=utf-8");
        request->send(response);
        
        String upperFormat = format;
        upperFormat.toUpperCase();
        analyzer.addLogEntry("Capture data downloaded as " + filename + " (" + upperFormat + " format)");
    });
}

String getIndexHTML() {
    return "<!DOCTYPE html><html><head><title>M5Stack AtomProbe</title><meta charset='UTF-8'>"
           "<style>" 
           "*{margin:0;padding:0;box-sizing:border-box;}" 
           "body{font-family:'Google Sans',Inter,-apple-system,BlinkMacSystemFont,sans-serif;background:radial-gradient(ellipse at top,#1a1a2e 0%,#16213e 50%,#0f0f1a 100%);color:#e8eaed;min-height:100vh;line-height:1.6;}" 
           ".container{max-width:1400px;margin:0 auto;padding:24px;}" 
           "h1{text-align:center;margin-bottom:40px;font-size:2.8em;font-weight:300;background:linear-gradient(135deg,#4fc3f7 0%,#9c27b0 50%,#e91e63 100%);-webkit-background-clip:text;-webkit-text-fill-color:transparent;filter:drop-shadow(0 0 20px rgba(156,39,176,0.3));}" 
           ".gemini-card{background:linear-gradient(135deg,rgba(255,255,255,0.06) 0%,rgba(255,255,255,0.02) 100%);backdrop-filter:blur(20px);border:1px solid rgba(255,255,255,0.08);border-radius:24px;padding:32px;margin:24px 0;box-shadow:0 16px 40px rgba(0,0,0,0.12),0 8px 16px rgba(0,0,0,0.08);transition:all 0.3s cubic-bezier(0.4,0,0.2,1);}" 
           ".gemini-card:hover{transform:translateY(-4px);box-shadow:0 24px 48px rgba(0,0,0,0.16),0 12px 24px rgba(0,0,0,0.12);border-color:rgba(79,195,247,0.2);}" 
           ".gemini-card h2,.gemini-card h3{color:#4fc3f7;margin-bottom:20px;font-size:1.4em;font-weight:500;}" 
           ".gemini-btn{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;padding:14px 28px;margin:8px;border-radius:32px;cursor:pointer;font-weight:500;font-size:14px;transition:all 0.3s cubic-bezier(0.4,0,0.2,1);box-shadow:0 8px 24px rgba(102,126,234,0.3);}" 
           ".gemini-btn:hover{transform:translateY(-2px) scale(1.02);box-shadow:0 12px 32px rgba(102,126,234,0.4);filter:brightness(1.1);}" 
           ".gemini-btn.danger{background:linear-gradient(135deg,#ff6b6b 0%,#ee5a52 100%);box-shadow:0 8px 24px rgba(255,107,107,0.3);}" 
           ".gemini-btn.success{background:linear-gradient(135deg,#4ecdc4 0%,#44a08d 100%);box-shadow:0 8px 24px rgba(78,205,196,0.3);}" 
           ".gemini-btn.secondary{background:linear-gradient(135deg,#a8a8a8 0%,#7f7f7f 100%);box-shadow:0 8px 24px rgba(168,168,168,0.2);}"
           ".gemini-status{font-size:18px;font-weight:500;background:linear-gradient(135deg,rgba(78,205,196,0.15) 0%,rgba(78,205,196,0.05) 100%);padding:20px;border-radius:16px;margin:20px 0;border:1px solid rgba(78,205,196,0.2);position:relative;overflow:hidden;}" 
           ".gemini-status::before{content:'';position:absolute;top:0;left:-100%;width:100%;height:100%;background:linear-gradient(90deg,transparent,rgba(255,255,255,0.1),transparent);animation:shimmer 2s infinite;}" 
           ".gpio-status{font-size:20px;font-weight:500;margin:20px 0;padding:20px;background:linear-gradient(135deg,rgba(156,39,176,0.15) 0%,rgba(156,39,176,0.05) 100%);border-radius:16px;border:1px solid rgba(156,39,176,0.2);}" 
           ".info-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:20px;margin:20px 0;}" 
           ".info-item{background:linear-gradient(135deg,rgba(255,255,255,0.08) 0%,rgba(255,255,255,0.02) 100%);padding:20px;border-radius:16px;border:1px solid rgba(79,195,247,0.15);transition:all 0.3s ease;}" 
           ".info-item:hover{transform:translateY(-2px);border-color:rgba(79,195,247,0.3);}" 
           ".info-item strong{color:#4fc3f7;font-weight:500;}" 
           "#logs,#data,.gemini-mono{height:360px;overflow-y:auto;background:linear-gradient(135deg,rgba(255,255,255,0.03) 0%,rgba(255,255,255,0.01) 100%);padding:20px;border:1px solid rgba(255,255,255,0.06);border-radius:16px;font-family:'JetBrains Mono','Fira Code',monospace;font-size:13px;line-height:1.6;color:#e8eaed;}" 
           "#logs::-webkit-scrollbar,#data::-webkit-scrollbar,.gemini-mono::-webkit-scrollbar{width:6px;}" 
           "#logs::-webkit-scrollbar-track,#data::-webkit-scrollbar-track,.gemini-mono::-webkit-scrollbar-track{background:rgba(255,255,255,0.05);border-radius:8px;}" 
           "#logs::-webkit-scrollbar-thumb,#data::-webkit-scrollbar-thumb,.gemini-mono::-webkit-scrollbar-thumb{background:rgba(79,195,247,0.4);border-radius:8px;}"
           ".grid{display:grid;grid-template-columns:1fr 1fr;gap:32px;}" 
           ".controls{display:flex;flex-wrap:wrap;gap:12px;margin:24px 0;}" 
           ".gemini-indicator{display:inline-flex;align-items:center;justify-content:center;width:16px;height:16px;border-radius:50%;margin-right:12px;position:relative;}" 
           ".gemini-indicator.ready{background:linear-gradient(135deg,#4ecdc4,#44a08d);box-shadow:0 0 16px rgba(78,205,196,0.4);animation:pulse-ready 2s infinite;}" 
           ".gemini-indicator.capturing{background:linear-gradient(135deg,#ff6b6b,#ee5a52);box-shadow:0 0 16px rgba(255,107,107,0.5);animation:pulse-capture 1s infinite;}" 
           "@keyframes pulse-ready{0%,100%{transform:scale(1);opacity:1;}50%{transform:scale(1.1);opacity:0.8;}}" 
           "@keyframes pulse-capture{0%,100%{transform:scale(1);opacity:1;}50%{transform:scale(1.2);opacity:0.7;}}" 
           "@keyframes shimmer{0%{left:-100%;}100%{left:100%;}}" 
           "@media (max-width:768px){.grid{grid-template-columns:1fr;}.controls{justify-content:center;}}"
           "</style></head><body>" 
           "<div class='container'>" 
           "<h1>M5Stack AtomProbe</h1>"
           "<div class='gemini-card'>" 
           "<h2>🌐 Network Connection</h2>" 
           "<div class='info-grid'>" 
           "<div class='info-item'><strong>Mode:</strong> <span id='network-mode'>Loading...</span></div>" 
           "<div class='info-item'><strong>Network:</strong> <span id='network-name'>Loading...</span></div>" 
           "<div class='info-item'><strong>IP Address:</strong> <span id='network-ip'>Loading...</span></div>" 
           "</div>" 
           "<div class='controls'>" 
           "<button class='gemini-btn secondary' onclick='window.location.href=\"/config\"'>⚙️ WiFi Configuration</button>" 
           "</div></div>"
           "<div class='gemini-card'>" 
           "<h2>📡 UART Monitor</h2>" 
           "<div class='controls'>" 
           "<button class='gemini-btn success' onclick='toggleUart()' id='uart-toggle'>▶️ Start UART</button>" 
           "<button class='gemini-btn secondary' onclick='toggleUartConfig()'>⚙️ Configure</button>" 
           "<button class='gemini-btn secondary' onclick='clearUartLogs()'>🗑️ Clear UART</button>" 
           "<button class='gemini-btn secondary' onclick='copyUartData()'>📋 Copy Data</button>"
           "<button class='gemini-btn' onclick='downloadUartLogs()'>📥 Download UART</button>" 
           "</div>" 
           "<div id='uart-config' style='display:none;margin:15px 0;padding:20px;background:rgba(156,39,176,0.1);border-radius:12px;'>" 
           "<h4 style='margin-bottom:15px;color:#9c27b0;'>⚙️ UART Configuration</h4>" 
           "<div class='info-grid'>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Baudrate:</label>" 
           "<select id='uart-baudrate' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;' onchange='updateBufferTimeEstimates()'>"
           "<option value='9600'>9600</option><option value='19200'>19200</option><option value='38400'>38400</option>" 
           "<option value='57600'>57600</option><option value='115200' selected>115200</option><option value='230400'>230400</option>" 
           "<option value='460800'>460800</option><option value='921600'>921600</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Data Bits:</label>" 
           "<select id='uart-databits' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='7'>7</option><option value='8' selected>8</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Parity:</label>" 
           "<select id='uart-parity' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='0' selected>None</option><option value='1'>Odd</option><option value='2'>Even</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Stop Bits:</label>" 
           "<select id='uart-stopbits' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='1' selected>1</option><option value='2'>2</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>RX Pin:</label>" 
           "<input id='uart-rxpin' type='number' value='7' min='0' max='48' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>TX Pin (-1=disabled):</label>" 
           "<input id='uart-txpin' type='number' value='-1' min='-1' max='48' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Duplex Mode:</label>" 
           "<select id='uart-duplex' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='0' selected>Full Duplex (RX + TX)</option><option value='1'>Half Duplex (Single Wire)</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Buffer Size:</label>"
           "<select id='uart-buffersize' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;' onchange='updateBufferTimeEstimates()'>" 
           "<option value='1000'>1,000 entries</option>" 
           "<option value='5000'>5,000 entries</option>" 
           "<option value='10000' selected>10,000 entries</option>" 
           "<option value='25000'>25,000 entries</option>" 
           "<option value='50000'>50,000 entries</option>" 
           "<option value='100000'>100,000 entries</option>" 
           "<option value='200000'>200,000 entries (Shared Flash)</option>" 
           "<option value='300000'>300,000 entries</option>" 
           "<option value='400000'>400,000 entries (Max Flash)</option></select></div>"
           "<div style='margin:10px 5px;padding:8px;background:rgba(79,195,247,0.1);border-radius:4px;font-size:12px;color:#4fc3f7;'>" 
           "<div id='uart-time-estimate'>📊 Estimated duration...</div></div>" 
           "<div style='margin-top:15px;'>" 
           "<button class='gemini-btn' onclick='saveUartConfig()'>✅ Apply Configuration</button></div>"
           "</div>" 
           "<div id='uart-status' class='info-grid' style='margin:10px 0;'>" 
           "<div class='info-item'><strong>Status:</strong> <span id='uart-monitoring-status'>Disabled</span></div>" 
           "<div class='info-item'><strong>Config:</strong> <span id='uart-current-config'>115200 8N1</span></div>" 
           "<div class='info-item'><strong>Pins:</strong> <span id='uart-pins'>RX:7 TX:disabled</span></div>" 
           "<div class='info-item'><strong>Bytes:</strong> <span id='uart-bytes'>RX:0 TX:0</span></div>" 
           "<div class='info-item'><strong>Buffer:</strong> <span id='uart-buffer-info'>0/10000 (0KB)</span></div>" 
           "<div class='info-item'><strong>Storage:</strong> <span id='uart-storage-type'>Flash</span> <button class='gemini-btn secondary' onclick='toggleFlashStorage()' id='flash-toggle' style='margin-left:8px;padding:4px 8px;font-size:11px;background:linear-gradient(135deg,#4ecdc4 0%,#44a08d 100%);'>💽 RAM</button></div>"
           "</div>"
           "<div id='half-duplex-panel' style='display:none;margin:15px 0;padding:20px;background:rgba(255,152,0,0.1);border-radius:12px;border:1px solid rgba(255,152,0,0.2);'>" 
           "<h4 style='margin-bottom:15px;color:#ff9800;'>📡 Half-Duplex Command Interface</h4>" 
           "<div style='display:flex;align-items:center;gap:12px;'>" 
           "<input id='half-duplex-command' type='text' placeholder='Enter command to send...' style='flex:1;padding:12px;border-radius:8px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;font-family:monospace;font-size:14px;'>" 
           "<button class='gemini-btn success' onclick='sendHalfDuplexCommand()' id='send-command-btn'>📤 Send</button>" 
           "</div>" 
           "<div style='margin-top:12px;font-size:12px;color:#ff9800;'>" 
           "<div>💡 Commands are sent with automatic line endings (\\r\\n)</div>" 
           "<div id='half-duplex-status' style='margin-top:8px;font-weight:bold;'>Status: Ready</div>" 
           "</div></div>"
           "<div id='uart-logs' class='gemini-mono'>UART monitoring disabled...</div>" 
           "</div>"
           "<div class='gemini-card'>" 
           "<h2>⚡ GPIO High-Performance Analysis</h2>"
           "<div class='controls'>" 
           "<button class='gemini-btn success' onclick='toggleCapture()' id='capture-toggle'>▶️ Start Capture</button>" 
           "<button class='gemini-btn secondary' onclick='toggleLogicConfig()'>⚙️ Configure</button>" 
           "<button class='gemini-btn' onclick='getData()'>📊 Get Data</button>" 
           "<button class='gemini-btn' onclick='toggleDualMode()' id='dual-mode-toggle' style='background:linear-gradient(135deg,#ff9800 0%,#f57c00 100%);'>🔗 Dual Mode</button>" 
           "</div>"
           "<div id='logic-config' style='display:none;margin:15px 0;padding:20px;background:rgba(79,195,247,0.1);border-radius:12px;'>" 
           "<h4 style='margin-bottom:15px;color:#4fc3f7;'>⚙️ Logic Analyzer Configuration</h4>" 
           "<div class='info-grid'>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Sample Rate (Hz):</label>" 
           "<select id='logic-samplerate' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;' onchange='updateLogicTimeEstimates()'>" 
           "<option value='10'>10Hz (Ultra-Low)</option><option value='20'>20Hz</option><option value='40'>40Hz</option>" 
           "<option value='80'>80Hz</option><option value='100'>100Hz</option><option value='200'>200Hz</option>" 
           "<option value='400'>400Hz</option><option value='1000'>1kHz</option><option value='10000'>10kHz</option>" 
           "<option value='100000'>100kHz</option><option value='1000000' selected>1MHz</option>" 
           "<option value='2000000'>2MHz</option><option value='5000000'>5MHz</option><option value='10000000'>10MHz</option>" 
           "<option value='20000000'>20MHz</option><option value='40000000'>40MHz (Max)</option></select></div>"
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>GPIO Pin:</label>" 
           "<select id='logic-gpiopin' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='1' selected>GPIO1 (AtomS3 Optimized)</option><option value='2'>GPIO2</option><option value='4'>GPIO4</option></select></div>"
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Trigger Mode:</label>" 
           "<select id='logic-trigger' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
           "<option value='0' selected>None</option><option value='1'>Rising Edge</option><option value='2'>Falling Edge</option>" 
           "<option value='3'>Both Edges</option><option value='4'>High Level</option><option value='5'>Low Level</option></select></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Buffer Size:</label>" 
           "<select id='logic-buffersize' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;' onchange='updateLogicTimeEstimates()'>" 
           "<option value='4096'>4,096 samples (20KB - RAM)</option><option value='8192'>8,192 samples (40KB - RAM)</option>" 
           "<option value='16384'>16,384 samples (80KB - RAM)</option><option value='100000'>100K samples (500KB - Flash)</option>" 
           "<option value='200000'>200K samples (1MB - Flash)</option><option value='400000' selected>400K samples (2MB - Flash, Shared)</option>" 
           "<option value='600000'>600K samples (3MB - Flash)</option><option value='800000'>800K samples (4MB - Flash, Max)</option></select></div>"
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Pre-Trigger (%):</label>" 
           "<input id='logic-pretrigger' type='number' value='10' min='0' max='90' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'></div>"
           "</div>" 
           "<div style='margin:10px 5px;padding:8px;background:rgba(79,195,247,0.1);border-radius:4px;font-size:12px;color:#4fc3f7;'>" 
           "<div id='logic-time-estimate'>📊 Estimated buffer duration...</div>" 
           "<div style='font-size:11px;color:#ff9800;margin-top:4px;'>⚠️ Flash shared with UART (6MB total: 4MB Logic + 2MB UART)</div></div>" 
           "<div style='margin-top:15px;'>" 
           "<button class='gemini-btn' onclick='saveLogicConfig()'>✅ Apply Configuration</button></div>" 
           "</div>"
           "<div id='logic-status' class='info-grid' style='margin:10px 0;'>" 
           "<div class='info-item'><strong>Channel:</strong> <span id='logic-current-channel'>GPIO1</span></div>" 
           "<div class='info-item'><strong>Sample Rate:</strong> <span id='logic-current-rate'>1MHz</span></div>" 
           "<div class='info-item'><strong>Trigger:</strong> <span id='logic-current-trigger'>None</span></div>" 
           "<div class='info-item'><strong>Buffer:</strong> <span id='logic-buffer-info'>1,000,000 samples</span></div>" 
           "<div class='info-item'><strong>Duration:</strong> <span id='logic-duration'>1.0s</span></div>" 
           "<div class='info-item'><strong>Usage:</strong> <span id='logic-buffer-usage'>0/1,000,000 (0%)</span></div>" 
           "<div class='info-item'><strong>Storage:</strong> <span id='logic-storage-type'>Flash</span> <span id='logic-storage-size'>(2MB)</span></div>" 
           "<div class='info-item'><strong>Dual Mode:</strong> <span id='dual-mode-status'>Disabled</span> <span id='dual-mode-info'></span></div>" 
           "</div>"
           "<div id='status' class='gemini-status'><span class='gemini-indicator ready'></span>Ready</div>" 
           "<div id='gpio-status' class='gpio-status'>🔌 GPIO: High-Performance Mode</div>"
           "</div>" 
           "<div class='grid'>" 
           "<div class='gemini-card'>" 
           "<h3>📊 Logic Data</h3>"
           "<div class='controls'>" 
           "<button class='gemini-btn secondary' onclick='clearData()'>🗑️ Clear Data</button>" 
           "<button class='gemini-btn' onclick='downloadData(\"json\")'>📥 Download JSON</button>" 
           "<button class='gemini-btn' onclick='downloadData(\"csv\")'>📥 Download CSV</button>" 
           "</div>" 
           "<div id='data' class='gemini-mono'>No data captured yet...</div>" 
           "</div>"
           "<div class='gemini-card'>" 
           "<h3>📝 Serial Logs</h3>" 
           "<div class='controls'>" 
           "<button class='gemini-btn secondary' onclick='clearLogs()'>🗑️ Clear Logs</button>" 
           "<button class='gemini-btn' onclick='downloadLogs()'>📥 Download Logs</button>" 
           "</div>" 
           "<div id='logs' class='gemini-mono'>Loading logs...</div>" 
           "</div>"
           "</div>" 
           "</div>" 
           "<script>"
           "function toggleCapture(){fetch('/api/status').then(r=>r.json()).then(d=>{if(d.capturing){stopCapture();}else{startCapture();}});}" 
           "function startCapture(){fetch('/api/start',{method:'POST'}).then(()=>updateAll());}" 
           "function stopCapture(){fetch('/api/stop',{method:'POST'}).then(()=>updateAll());}" 
           "function getData(){fetch('/api/data').then(r=>r.json()).then(d=>document.getElementById('data').innerHTML='<pre>'+JSON.stringify(d,null,2)+'</pre>');}"
           "function clearData(){fetch('/api/data/clear',{method:'POST'}).then(()=>{document.getElementById('data').innerHTML='No data captured yet...';updateAll();});}"
           "function clearLogs(){fetch('/api/logs/clear',{method:'POST'}).then(()=>loadLogs());}" 
           "function loadLogs(){fetch('/api/logs').then(r=>r.json()).then(d=>{const logs=d.logs.map(log=>'<div style=\"margin-bottom:5px;padding:5px;background:rgba(0,212,255,0.1);border-radius:4px;\">' + log + '</div>').join('');document.getElementById('logs').innerHTML=logs||'No logs available';});}" 
           "function downloadLogs(){window.open('/download/logs','_blank');}" 
           "function downloadData(format){window.open('/download/data?format='+format,'_blank');}" 
           "function toggleUart(){fetch('/api/uart/logs').then(r=>r.json()).then(d=>{if(d.monitoring_enabled){disableUartMonitoring();}else{enableUartMonitoring();}});}" 
           "function enableUartMonitoring(){fetch('/api/uart/enable',{method:'POST'}).then(()=>{setTimeout(loadUartLogs,500);updateAll();});}" 
           "function disableUartMonitoring(){fetch('/api/uart/disable',{method:'POST'}).then(()=>{setTimeout(loadUartLogs,500);updateAll();});}"
           "function clearUartLogs(){fetch('/api/uart/clear',{method:'POST'}).then(()=>loadUartLogs());}" 
           "function loadUartLogs(){fetch('/api/uart/logs').then(r=>r.json()).then(d=>{document.getElementById('uart-monitoring-status').textContent=d.monitoring_enabled?'Active':'Disabled';const config=d.config;document.getElementById('uart-current-config').textContent=config.baudrate+' '+config.data_bits+config.parity_string.charAt(0)+config.stop_bits+' ('+(config.duplex_string||'Full')+')';document.getElementById('uart-pins').textContent='RX:'+config.rx_pin+' TX:'+config.tx_pin;document.getElementById('uart-bytes').textContent='RX:'+d.bytes_received+' TX:'+d.bytes_sent;document.getElementById('uart-buffer-info').textContent=d.count+'/'+d.max_entries+' ('+(d.memory_usage/1024).toFixed(1)+'KB)';document.getElementById('uart-memory-usage').textContent=(d.memory_usage/1024).toFixed(1)+'KB used';document.getElementById('uart-storage-type').textContent=d.storage_type||'Flash';const logs=d.uart_logs.map(log=>'<div style=\"margin-bottom:5px;padding:5px;background:rgba(156,39,176,0.1);border-radius:4px;\">' + log + '</div>').join('');document.getElementById('uart-logs').innerHTML=logs||'No UART data logged';const toggleBtn=document.getElementById('uart-toggle');if(d.monitoring_enabled){toggleBtn.textContent='⏹️ Stop UART';toggleBtn.className='gemini-btn danger';}else{toggleBtn.textContent='▶️ Start UART';toggleBtn.className='gemini-btn success';}updateStorageDisplay();const panel=document.getElementById('half-duplex-panel');if(config.duplex_mode===1){panel.style.display='block';}else{panel.style.display='none';}}).catch(e=>console.error('UART logs error:',e));}"
           "function copyUartData(){navigator.clipboard.writeText(document.getElementById('uart-logs').innerText).then(()=>alert('UART data copied to clipboard!')).catch(e=>alert('Copy failed: '+e.message));}" 
           "function downloadUartLogs(){window.open('/download/uart','_blank');}" 
           "function toggleUartConfig(){const config=document.getElementById('uart-config');config.style.display=config.style.display==='none'?'block':'none';if(config.style.display==='block'){loadUartConfig();};}"
           "function checkAndUpdateHalfDuplexPanel(){fetch('/api/uart/config').then(r=>r.json()).then(d=>{const panel=document.getElementById('half-duplex-panel');if(d.duplex_mode===1){panel.style.display='block';}else{panel.style.display='none';}}).catch(e=>console.error('Half-duplex panel check error:',e));}"
           "function loadUartConfig(){fetch('/api/uart/config').then(r=>r.json()).then(d=>{document.getElementById('uart-baudrate').value=d.baudrate;document.getElementById('uart-databits').value=d.data_bits;document.getElementById('uart-parity').value=d.parity;document.getElementById('uart-stopbits').value=d.stop_bits;document.getElementById('uart-rxpin').value=d.rx_pin;document.getElementById('uart-txpin').value=d.tx_pin;document.getElementById('uart-duplex').value=d.duplex_mode||0;}).catch(e=>console.error('UART config load error:',e));}"
           "function saveUartConfig(){const formData=new FormData();formData.append('baudrate',document.getElementById('uart-baudrate').value);formData.append('data_bits',document.getElementById('uart-databits').value);formData.append('parity',document.getElementById('uart-parity').value);formData.append('stop_bits',document.getElementById('uart-stopbits').value);formData.append('rx_pin',document.getElementById('uart-rxpin').value);formData.append('tx_pin',document.getElementById('uart-txpin').value);formData.append('duplex_mode',document.getElementById('uart-duplex').value);fetch('/api/uart/config',{method:'POST',body:formData}).then(()=>{loadUartLogs();document.getElementById('uart-config').style.display='none';setTimeout(checkAndUpdateHalfDuplexPanel,500);});}"
           "function saveBufferSize(){const formData=new FormData();formData.append('size',document.getElementById('uart-buffersize').value);fetch('/api/uart/buffersize',{method:'POST',body:formData}).then(r=>r.json()).then(d=>{alert('Buffer size updated to '+d.new_size+' entries');loadUartLogs();updateBufferTimeEstimates();});}"
           "function updateBufferTimeEstimates(){" 
           "const baudrate=parseInt(document.getElementById('uart-baudrate').value);" 
           "const bufferSize=parseInt(document.getElementById('uart-buffersize').value);" 
           "const avgBytesPerEntry=50;" 
           "const totalBytes=bufferSize*avgBytesPerEntry;" 
           "const bytesPerSecond=baudrate/10;" 
           "const timeInSeconds=totalBytes/bytesPerSecond;" 
           "let timeStr='';" 
           "if(timeInSeconds<60){timeStr=Math.round(timeInSeconds)+'s';}" 
           "else if(timeInSeconds<3600){timeStr=Math.round(timeInSeconds/60)+'min';}" 
           "else{timeStr=Math.round(timeInSeconds/3600*10)/10+'h';}" 
           "const storageUsage=Math.round(bufferSize*avgBytesPerEntry/1024);" 
           "document.getElementById('buffer-time-estimate').innerHTML='📊 '+bufferSize.toLocaleString()+' entries \\u2248 '+timeStr+' @ '+baudrate+' baud (\\u2248'+storageUsage+'KB)';"
           "}" 
           "function loadUartConfig(){fetch('/api/uart/config').then(r=>r.json()).then(d=>{document.getElementById('uart-baudrate').value=d.baudrate;document.getElementById('uart-databits').value=d.data_bits;document.getElementById('uart-parity').value=d.parity;document.getElementById('uart-stopbits').value=d.stop_bits;document.getElementById('uart-rxpin').value=d.rx_pin;document.getElementById('uart-txpin').value=d.tx_pin;document.getElementById('uart-duplex').value=d.duplex_mode||0;updateBufferTimeEstimates();updateHalfDuplexPanel();}).catch(e=>console.error('UART config load error:',e));}"
           "function toggleLogicConfig(){const config=document.getElementById('logic-config');config.style.display=config.style.display==='none'?'block':'none';if(config.style.display==='block'){loadLogicConfig();};}" 
           "function loadLogicConfig(){fetch('/api/logic/config').then(r=>r.json()).then(d=>{document.getElementById('logic-samplerate').value=d.sample_rate||1000000;document.getElementById('logic-gpiopin').value=d.gpio_pin||1;document.getElementById('logic-trigger').value=d.trigger_mode||0;document.getElementById('logic-buffersize').value=d.buffer_size||16384;document.getElementById('logic-pretrigger').value=d.pre_trigger_percent||10;updateLogicTimeEstimates();}).catch(e=>console.error('Logic config load error:',e));}"
           "function saveLogicConfig(){const formData=new FormData();formData.append('sample_rate',document.getElementById('logic-samplerate').value);formData.append('gpio_pin',document.getElementById('logic-gpiopin').value);formData.append('trigger_mode',document.getElementById('logic-trigger').value);formData.append('buffer_size',document.getElementById('logic-buffersize').value);formData.append('pre_trigger_percent',document.getElementById('logic-pretrigger').value);fetch('/api/logic/config',{method:'POST',body:formData}).then(()=>{loadLogicConfig();updateLogicStatus();document.getElementById('logic-config').style.display='none';alert('Logic Analyzer configuration saved!');});}"
           "function updateLogicTimeEstimates(){const sampleRate=parseInt(document.getElementById('logic-samplerate').value);const bufferSize=parseInt(document.getElementById('logic-buffersize').value);const durationSeconds=bufferSize/sampleRate;let timeStr='';if(durationSeconds<0.001){timeStr=Math.round(durationSeconds*1000000)+'μs';}else if(durationSeconds<1){timeStr=Math.round(durationSeconds*1000*10)/10+'ms';}else if(durationSeconds<60){timeStr=Math.round(durationSeconds*10)/10+'s';}else if(durationSeconds<3600){timeStr=Math.round(durationSeconds/60*10)/10+'min';}else if(durationSeconds<86400){timeStr=Math.round(durationSeconds/3600*10)/10+'h';}else{timeStr=Math.round(durationSeconds/86400*10)/10+'d';}document.getElementById('logic-time-estimate').innerHTML='📊 '+bufferSize.toLocaleString()+' samples ≈ '+timeStr+' @ '+(sampleRate>=1000000?Math.round(sampleRate/1000000*10)/10+'MHz':sampleRate>=1000?Math.round(sampleRate/1000)+'kHz':sampleRate+'Hz');}"
           "function updateLogicStatus(){fetch('/api/logic/config').then(r=>r.json()).then(d=>{document.getElementById('logic-current-channel').textContent='GPIO'+d.gpio_pin;document.getElementById('logic-current-rate').textContent=d.sample_rate>=1000000?Math.round(d.sample_rate/1000000*10)/10+'MHz':d.sample_rate>=1000?Math.round(d.sample_rate/1000)+'kHz':d.sample_rate+'Hz';document.getElementById('logic-current-trigger').textContent=d.trigger_mode_string;document.getElementById('logic-buffer-info').textContent=d.buffer_size.toLocaleString()+' samples';const duration=d.buffer_duration_seconds;let durationStr='';if(duration<0.001){durationStr=Math.round(duration*1000000)+'μs';}else if(duration<1){durationStr=Math.round(duration*1000*10)/10+'ms';}else if(duration<60){durationStr=Math.round(duration*10)/10+'s';}else if(duration<3600){durationStr=Math.round(duration/60*10)/10+'min';}else if(duration<86400){durationStr=Math.round(duration/3600*10)/10+'h';}else{durationStr=Math.round(duration/86400*10)/10+'d';}document.getElementById('logic-duration').textContent=durationStr;});fetch('/api/status').then(r=>r.json()).then(d=>{const usage=d.buffer_usage||0;const total=d.buffer_size||1000000;const percent=Math.round((usage/total)*100);document.getElementById('logic-buffer-usage').textContent=usage.toLocaleString()+'/'+total.toLocaleString()+' ('+percent+'%)';const storageType=total>50000?'Flash':'RAM';const storageMB=(total*5/1024/1024).toFixed(1);document.getElementById('logic-storage-type').textContent=storageType;document.getElementById('logic-storage-size').textContent='('+storageMB+'MB)';}).catch(e=>console.error('Logic status update error:',e));}"
           "function toggleFlashStorage(){"
           "fetch('/api/uart/storage').then(r=>r.json()).then(d=>{" 
           "const newState = !d.flash_enabled;" 
           "const formData=new FormData();formData.append('enable',newState.toString());" 
           "fetch('/api/uart/storage/flash',{method:'POST',body:formData}).then(r=>r.json()).then(result=>{" 
           "updateStorageDisplay();" 
           "loadUartLogs();" 
           "alert('Storage switched to: '+result.storage_type);" 
           "}).catch(e=>console.error('Flash toggle error:',e));" 
           "}).catch(e=>console.error('Storage status error:',e));}" 
           "function updateStorageDisplay(){" 
           "fetch('/api/uart/storage').then(r=>r.json()).then(d=>{" 
           "document.getElementById('uart-storage-type').textContent=d.storage_type;" 
           "const flashBtn=document.getElementById('flash-toggle');" 
           "flashBtn.textContent=d.flash_enabled?'💽 RAM':'💾 Flash';" 
           "flashBtn.style.background=d.flash_enabled?'linear-gradient(135deg,#4ecdc4 0%,#44a08d 100%)':'linear-gradient(135deg,#a8a8a8 0%,#7f7f7f 100%)';" 
           "}).catch(e=>console.error('Storage display error:',e));}" 
           "function updateAll(){" 
           "fetch('/api/status').then(r=>r.json()).then(d=>{" 
           "const indicator=d.capturing?'<span class=\"gemini-indicator capturing\"></span>':'<span class=\"gemini-indicator ready\"></span>';" 
           "document.getElementById('status').innerHTML=indicator+(d.capturing?'Capturing':'Ready')+' | Rate: '+d.sample_rate+'Hz | Buffer: '+d.buffer_usage+'/'+d.buffer_size+' | GPIO Only';"
           "document.getElementById('network-mode').textContent = d.ap_mode ? 'Access Point' : (d.wifi_connected ? 'WiFi Client' : 'Disconnected');" 
           "document.getElementById('network-name').textContent = d.wifi_ssid || 'None';" 
           "document.getElementById('network-ip').textContent = d.ip_address || 'None';" 
           "const captureBtn=document.getElementById('capture-toggle');if(d.capturing){captureBtn.textContent='⏹️ Stop Capture';captureBtn.className='gemini-btn danger';}else{captureBtn.textContent='▶️ Start Capture';captureBtn.className='gemini-btn success';}" 
           "});" 
           "fetch('/api/uart/logs').then(r=>r.json()).then(d=>{" 
           "const uartToggleBtn=document.getElementById('uart-toggle');" 
           "if(d.monitoring_enabled){" 
           "uartToggleBtn.textContent='⏹️ Stop UART';" 
           "uartToggleBtn.className='gemini-btn danger';" 
           "}else{" 
           "uartToggleBtn.textContent='▶️ Start UART';" 
           "uartToggleBtn.className='gemini-btn success';" 
           "}}).catch(e=>console.error('UART toggle update error:',e));" 
           "loadLogs();loadUartLogs();updateLogicStatus();}"
           "function sendHalfDuplexCommand(){" 
           "const command=document.getElementById('half-duplex-command').value.trim();" 
           "if(!command){alert('Please enter a command to send');return;}" 
           "const statusDiv=document.getElementById('half-duplex-status');" 
           "const sendBtn=document.getElementById('send-command-btn');" 
           "statusDiv.textContent='Sending command...';" 
           "sendBtn.disabled=true;" 
           "const formData=new FormData();" 
           "formData.append('command',command);" 
           "fetch('/api/uart/send',{method:'POST',body:formData})" 
           ".then(r=>r.json())" 
           ".then(d=>{" 
           "if(d.status==='queued'){" 
           "statusDiv.textContent='Command sent: '+command;" 
           "statusDiv.style.color='#4caf50';" 
           "document.getElementById('half-duplex-command').value='';" 
           "setTimeout(()=>{statusDiv.textContent='Status: Ready';statusDiv.style.color='#ff9800';},3000);" 
           "}else{" 
           "statusDiv.textContent='Error: '+d.message;" 
           "statusDiv.style.color='#f44336';" 
           "setTimeout(()=>{statusDiv.textContent='Status: Ready';statusDiv.style.color='#ff9800';},3000);" 
           "}" 
           "sendBtn.disabled=false;" 
           "}).catch(e=>{" 
           "statusDiv.textContent='Error: '+e.message;" 
           "statusDiv.style.color='#f44336';" 
           "sendBtn.disabled=false;" 
           "setTimeout(()=>{statusDiv.textContent='Status: Ready';statusDiv.style.color='#ff9800';},3000);" 
           "});}" 
           "function updateHalfDuplexPanel(){" 
           "const duplexMode=document.getElementById('uart-duplex').value;" 
           "const panel=document.getElementById('half-duplex-panel');" 
           "if(duplexMode==='1'){" 
           "panel.style.display='block';" 
           "}else{" 
           "panel.style.display='none';" 
           "}}" 
           "document.getElementById('uart-duplex').addEventListener('change',updateHalfDuplexPanel);" 
           "document.getElementById('half-duplex-command').addEventListener('keypress',function(e){" 
           "if(e.key==='Enter'){sendHalfDuplexCommand();}" 
           "});" 
           "function toggleDualMode(){" 
           "fetch('/api/dual-mode/status').then(r=>r.json()).then(d=>{" 
           "const newState = !d.dual_mode_active;" 
           "const formData=new FormData();formData.append('enable',newState.toString());" 
           "fetch('/api/dual-mode',{method:'POST',body:formData}).then(r=>r.json()).then(result=>{" 
           "updateDualModeDisplay();" 
           "alert('Dual Mode '+(result.dual_mode_active?'Enabled':'Disabled')+': UART + Logic on same pin');" 
           "}).catch(e=>console.error('Dual mode toggle error:',e));" 
           "}).catch(e=>console.error('Dual mode status error:',e));}" 
           "function updateDualModeDisplay(){" 
           "fetch('/api/dual-mode/status').then(r=>r.json()).then(d=>{" 
           "document.getElementById('dual-mode-status').textContent=d.dual_mode_active?'Active':'Disabled';" 
           "document.getElementById('dual-mode-info').textContent=d.dual_mode_active?'(GPIO'+d.logic_pin+')':'';" 
           "const dualBtn=document.getElementById('dual-mode-toggle');" 
           "dualBtn.textContent=d.dual_mode_active?'🔗 Dual ON':'🔗 Dual Mode';" 
           "dualBtn.style.background=d.dual_mode_active?'linear-gradient(135deg,#4caf50 0%,#388e3c 100%)':'linear-gradient(135deg,#ff9800 0%,#f57c00 100%)';" 
           "}).catch(e=>console.error('Dual mode display error:',e));}" 
           "setInterval(updateAll,2000);updateAll();setInterval(loadUartLogs,3000);setInterval(updateDualModeDisplay,2000);" 
           "setTimeout(updateBufferTimeEstimates,1000);" 
           "setTimeout(checkAndUpdateHalfDuplexPanel,1500);"
           "</script></body></html>";
}

// WiFi management functions
void loadWiFiCredentials() {
    saved_ssid = preferences.getString("wifi_ssid", "");
    saved_password = preferences.getString("wifi_password", "");
    Serial.println("Loaded WiFi credentials: " + saved_ssid);
}

void saveWiFiCredentials(const String& ssid, const String& password) {
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_password", password);
    saved_ssid = ssid;
    saved_password = password;
    Serial.println("Saved WiFi credentials: " + ssid);
}

bool connectToWiFi() {
    if (saved_ssid.length() == 0) return false;
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    
    Serial.print("Connecting to WiFi: ");
    Serial.print(saved_ssid);
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        ap_mode = false;
        analyzer.addLogEntry("Connected to WiFi: " + saved_ssid);
        analyzer.addLogEntry("IP address: " + WiFi.localIP().toString());
        Serial.println();
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        analyzer.addLogEntry("Failed to connect to WiFi: " + saved_ssid);
        Serial.println();
        Serial.println("WiFi connection failed!");
        return false;
    }
}

void startAPMode() {
    WiFi.mode(WIFI_AP);
    bool apResult = WiFi.softAP(ap_ssid, ap_password);
    
    if (apResult) {
        wifi_connected = false;
        ap_mode = true;
        analyzer.addLogEntry("Access Point created successfully");
        analyzer.addLogEntry("AP SSID: " + String(ap_ssid));
        analyzer.addLogEntry("AP IP address: " + WiFi.softAPIP().toString());
        Serial.println("Access Point created!");
        Serial.print("AP SSID: ");
        Serial.println(ap_ssid);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        analyzer.addLogEntry("Failed to create Access Point");
        Serial.println("Failed to create Access Point!");
    }
}

String getWiFiStatus() {
    if (wifi_connected) {
        return "WiFi Mode: Connected to " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")";
    } else if (ap_mode) {
        return "WiFi Mode: Access Point " + String(ap_ssid) + " (" + WiFi.softAPIP().toString() + ")";
    } else {
        return "WiFi Mode: Disconnected";
    }
}

String getConfigHTML() {
    return "<!DOCTYPE html><html><head><title>AtomS3 Logic Analyzer - WiFi Configuration</title><meta charset='UTF-8'>" 
           "<style>" 
           "*{margin:0;padding:0;box-sizing:border-box;}" 
           "body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:linear-gradient(135deg,#0c0c0c 0%,#1a1a1a 100%);color:#e0e0e0;min-height:100vh;padding:20px;}" 
           ".container{max-width:600px;margin:0 auto;}" 
           "h1{text-align:center;margin-bottom:30px;font-size:2.5em;background:linear-gradient(45deg,#00d4ff,#7c3aed);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 20px rgba(0,212,255,0.5);}" 
           ".panel{background:linear-gradient(145deg,#1e1e1e,#2a2a2a);border:1px solid #333;border-radius:12px;padding:25px;margin:20px 0;box-shadow:0 8px 32px rgba(0,0,0,0.3);backdrop-filter:blur(10px);}" 
           ".panel h2{color:#00d4ff;margin-bottom:15px;font-size:1.3em;}" 
           "form{display:flex;flex-direction:column;gap:15px;}" 
           "label{color:#00d4ff;font-weight:600;margin-bottom:5px;}" 
           "input[type='text'],input[type='password']{background:#1a1a1a;border:1px solid #444;border-radius:6px;padding:12px;color:#e0e0e0;font-size:16px;}" 
           "input[type='text']:focus,input[type='password']:focus{outline:none;border-color:#00d4ff;box-shadow:0 0 10px rgba(0,212,255,0.3);}" 
           "button{background:linear-gradient(45deg,#007bff,#0056b3);color:white;border:none;padding:12px 24px;margin:8px 0;border-radius:8px;cursor:pointer;font-weight:600;transition:all 0.3s ease;box-shadow:0 4px 15px rgba(0,123,255,0.4);}" 
           "button:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(0,123,255,0.6);background:linear-gradient(45deg,#0056b3,#003d82);}" 
           "button.secondary{background:linear-gradient(45deg,#6c757d,#545b62);box-shadow:0 4px 15px rgba(108,117,125,0.4);}" 
           "button.secondary:hover{background:linear-gradient(45deg,#545b62,#495057);box-shadow:0 6px 20px rgba(108,117,125,0.6);}" 
           ".status{font-size:16px;color:#00ff88;background:rgba(0,255,136,0.1);padding:15px;border-radius:8px;margin:15px 0;border-left:4px solid #00ff88;text-align:center;}" 
           ".current-wifi{background:rgba(0,212,255,0.1);padding:15px;border-radius:8px;margin:15px 0;border-left:4px solid #00d4ff;}" 
           "</style></head><body>" 
           "<div class='container'>" 
           "<h1>WiFi Configuration</h1>" 
           "<div class='panel'>" 
           "<h2>Current Status</h2>" 
           "<div class='current-wifi'>" 
           "<div><strong>Mode:</strong> <span id='current-mode'>Loading...</span></div>" 
           "<div><strong>Network:</strong> <span id='current-network'>Loading...</span></div>" 
           "<div><strong>IP Address:</strong> <span id='current-ip'>Loading...</span></div>" 
           "</div></div>" 
           "<div class='panel'>" 
           "<h2>Connect to WiFi Network</h2>" 
           "<form id='wifi-form'>" 
           "<div>" 
           "<label for='ssid'>Network Name (SSID):</label>" 
           "<input type='text' id='ssid' name='ssid' required placeholder='Enter your WiFi network name' />" 
           "</div>" 
           "<div>" 
           "<label for='password'>Password:</label>" 
           "<input type='password' id='password' name='password' placeholder='Enter WiFi password (leave blank for open networks)' />" 
           "</div>" 
           "<button type='submit'>Save and Connect</button>" 
           "</form>" 
           "<div id='status' class='status' style='display:none;'></div>" 
           "</div>" 
           "<div class='panel'>" 
           "<h2>Access Point Mode</h2>" 
           "<p>Switch back to Access Point mode to use the device without WiFi connection.</p>" 
           "<button class='secondary' onclick='switchToAP()'>Switch to AP Mode</button>" 
           "</div>" 
           "<div class='panel'>" 
           "<p style='text-align:center;'><a href='/' style='color:#00d4ff;text-decoration:none;'>← Back to Logic Analyzer</a></p>" 
           "</div></div>" 
           "<script>" 
           "function updateStatus(){" 
           "fetch('/api/status').then(r=>r.json()).then(d=>{" 
           "document.getElementById('current-mode').textContent = d.ap_mode ? 'Access Point' : (d.wifi_connected ? 'WiFi Client' : 'Disconnected');" 
           "document.getElementById('current-network').textContent = d.wifi_ssid || 'None';" 
           "document.getElementById('current-ip').textContent = d.ip_address || 'None';" 
           "}).catch(e=>console.error('Error:',e));}" 
           "document.getElementById('wifi-form').addEventListener('submit', function(e){" 
           "e.preventDefault();" 
           "const ssid = document.getElementById('ssid').value;" 
           "const password = document.getElementById('password').value;" 
           "const status = document.getElementById('status');" 
           "const formData = new FormData();" 
           "formData.append('ssid', ssid);" 
           "formData.append('password', password);" 
           "status.style.display = 'block';" 
           "status.textContent = 'Saving credentials and connecting...';" 
           "fetch('/api/wifi/config', {method: 'POST', body: formData})" 
           ".then(r => r.json())" 
           ".then(d => {" 
           "status.textContent = d.message || 'Configuration saved. Device will restart to connect to WiFi.';" 
           "setTimeout(() => { window.location.href = '/'; }, 5000);" 
           "})" 
           ".catch(e => {" 
           "status.textContent = 'Error: ' + e.message;" 
           "status.style.borderLeftColor = '#ff4444';" 
           "status.style.backgroundColor = 'rgba(255,68,68,0.1)';" 
           "status.style.color = '#ff4444';" 
           "});});" 
           "function switchToAP(){" 
           "if(confirm('Switch to Access Point mode? This will clear saved WiFi credentials.')){" 
           "fetch('/api/wifi/ap',{method:'POST'}).then(r=>r.json()).then(d=>{" 
           "document.getElementById('status').style.display='block';" 
           "document.getElementById('status').textContent='Switching to AP mode...';" 
           "setTimeout(()=>{window.location.href='/';},3000);" 
           "});}}" 
           "updateStatus();" 
           "</script></body></html>";
}

// WiFi connection monitoring with 30-second timeout fallback
void checkWiFiConnection() {
    static unsigned long lastCheck = 0;
    unsigned long now = millis();
    
    // Check WiFi status every 5 seconds
    if (now - lastCheck < 5000) {
        return;
    }
    lastCheck = now;
    
    bool currently_connected = (WiFi.status() == WL_CONNECTED);
    
    if (currently_connected && !wifi_connected) {
        // WiFi reconnected
        wifi_connected = true;
        last_wifi_connected_time = now;
        wifi_monitoring_active = true;
        analyzer.addLogEntry("WiFi reconnected: " + WiFi.SSID());
        analyzer.setAPMode(false);
        
        // If we were in AP mode, we could stop AP here
        // For now, we'll keep AP running for dual access
        
    } else if (!currently_connected && wifi_connected) {
        // WiFi disconnected
        wifi_connected = false;
        analyzer.addLogEntry("WiFi disconnected, monitoring for " + String(WIFI_DISCONNECT_TIMEOUT/1000) + "s timeout");
        
    } else if (!currently_connected && !ap_mode && wifi_monitoring_active) {
        // Check if we've been disconnected too long
        if (now - last_wifi_connected_time > WIFI_DISCONNECT_TIMEOUT) {
            analyzer.addLogEntry("WiFi timeout reached, starting AP mode fallback");
            startAPMode();
            analyzer.setAPMode(true);
            ap_mode = true;
        }
    } else if (currently_connected && wifi_connected) {
        // Still connected, update timestamp
        last_wifi_connected_time = now;
    }
}

// Handle WiFi reconnection attempts
void handleWiFiReconnection() {
    if (!wifi_connected && !ap_mode && saved_ssid.length() > 0) {
        analyzer.addLogEntry("Attempting WiFi reconnection to: " + saved_ssid);
        if (connectToWiFi()) {
            wifi_connected = true;
            last_wifi_connected_time = millis();
            analyzer.addLogEntry("WiFi reconnection successful");
            analyzer.setAPMode(false);
        }
    }
}
