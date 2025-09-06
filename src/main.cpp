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
const char* ap_ssid = "AtomS3-LogicAnalyzer";
const char* ap_password = "logic123";  // At least 8 characters

// WiFi Configuration
Preferences preferences;
String saved_ssid = "";
String saved_password = "";
bool wifi_connected = false;
bool ap_mode = false;
const int WIFI_CONNECT_TIMEOUT = 15000;  // 15 seconds

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
String getWiFiStatus();

void setup() {
#ifdef ATOMS3_BUILD
    // Initialize M5AtomS3
    auto cfg = M5.config();
    AtomS3.begin(cfg);
    display = AtomS3.Display;
    
    Serial.println("AtomS3 Logic Analyzer Starting...");
    
    // Initialize logic analyzer
    analyzer.begin();
    analyzer.initDisplay();
#else
    Serial.begin(115200);
    Serial.println("ESP32 Logic Analyzer Starting...");
    
    // Initialize logic analyzer
    analyzer.begin();
#endif
    
    // Initialize preferences
    preferences.begin("logic_analyzer", false);
    
    // Pass preferences instance to analyzer
    analyzer.setPreferences(&preferences);
    
    // Load saved WiFi credentials
    loadWiFiCredentials();
    
    // Load UART configuration
    analyzer.loadUartConfig();
    
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
    
    // Handle button press (start/stop capture)
    if (AtomS3.BtnA.wasPressed()) {
        if (analyzer.isCapturing()) {
            analyzer.stopCapture();
        } else {
            analyzer.startCapture();
        }
    }
    
    // Main logic analyzer processing
    analyzer.process();
    
    // Update display
    analyzer.updateDisplay();
#else
    // Main logic analyzer processing
    analyzer.process();
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
        doc["buffer_size"] = 16384;
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
    
    // UART configuration endpoint
    server.on("/api/uart/config", HTTP_POST, [](AsyncWebServerRequest *request){
        uint32_t baudrate = 115200;
        uint8_t dataBits = 8;
        uint8_t parity = 0;
        uint8_t stopBits = 1;
        uint8_t rxPin = 43;
        uint8_t txPin = 44;
        
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
        
        analyzer.configureUart(baudrate, dataBits, parity, stopBits, rxPin, txPin);
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
        doc["max_entries"] = 1000;  // MAX_UART_ENTRIES value
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Clear buffer data endpoint
    server.on("/api/data/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        analyzer.clearBuffer();
        analyzer.addLogEntry("Capture buffer cleared by user");
        request->send(200, "application/json", "{\"status\":\"cleared\",\"message\":\"Buffer cleared\"}");
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
        String filename = "atoms3_logs_" + timestamp + ".txt";
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", logs);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        response->addHeader("Content-Type", "text/plain; charset=utf-8");
        request->send(response);
        
        analyzer.addLogEntry("Serial logs downloaded as " + filename);
    });
    
    server.on("/download/uart", HTTP_GET, [](AsyncWebServerRequest *request){
        String uartLogs = analyzer.getUartLogsAsPlainText();
        String timestamp = String(millis());
        String filename = "atoms3_uart_" + timestamp + ".txt";
        
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
            filename = "atoms3_capture_" + timestamp + ".csv";
            contentType = "text/csv";
        } else {
            data = analyzer.getDataAsJSON();
            filename = "atoms3_capture_" + timestamp + ".json";
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
    return "<!DOCTYPE html><html><head><title>AtomS3 Logic Analyzer</title><meta charset='UTF-8'>" 
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
           "<h1>AtomS3 Logic Analyzer</h1>" 
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
           "<h2>⚡ GPIO1 High-Performance Analysis</h2>" 
           "<div class='info-grid'>" 
           "<div class='info-item'><strong>Channel:</strong> GPIO1 Only</div>" 
           "<div class='info-item'><strong>Buffer:</strong> 16,384 samples</div>" 
           "<div class='info-item'><strong>Max Rate:</strong> 10MHz</div>" 
           "</div>"
           "<div class='controls'>" 
           "<button class='gemini-btn success' onclick='startCapture()'>▶️ Start Capture</button>" 
           "<button class='gemini-btn danger' onclick='stopCapture()'>⏹️ Stop Capture</button>" 
           "<button class='gemini-btn' onclick='getData()'>📊 Get Data</button>" 
           "</div>" 
           "<div id='status' class='gemini-status'><span class='gemini-indicator ready'></span>Ready</div>" 
           "<div id='gpio-status' class='gpio-status'>🔌 GPIO1: High-Performance Mode</div>"
           "</div>" 
           "<div class='grid'>" 
           "<div class='gemini-card'>" 
           "<h3>📊 Capture Data</h3>" 
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
           "<div class='gemini-card'>" 
           "<h3>📡 UART Monitor</h3>" 
           "<div class='controls'>" 
           "<button class='gemini-btn success' onclick='enableUartMonitoring()' id='uart-enable'>▶️ Start UART</button>" 
           "<button class='gemini-btn danger' onclick='disableUartMonitoring()' id='uart-disable'>⏹️ Stop UART</button>" 
           "<button class='gemini-btn secondary' onclick='toggleUartConfig()'>⚙️ Configure</button>" 
           "<button class='gemini-btn secondary' onclick='clearUartLogs()'>🗑️ Clear UART</button>" 
           "<button class='gemini-btn secondary' onclick='compactUartLogs()'>📋 Compact Buffer</button>" 
           "<button class='gemini-btn' onclick='downloadUartLogs()'>📥 Download UART</button>"
           "</div>" 
           "<div id='uart-config' style='display:none;margin:15px 0;padding:20px;background:rgba(156,39,176,0.1);border-radius:12px;'>" 
           "<h4 style='margin-bottom:15px;color:#9c27b0;'>⚙️ UART Configuration</h4>" 
           "<div class='info-grid'>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>Baudrate:</label>" 
           "<select id='uart-baudrate' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'>" 
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
           "<input id='uart-rxpin' type='number' value='43' min='0' max='48' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'></div>" 
           "<div style='display:flex;flex-direction:column;margin:5px;'><label>TX Pin:</label>" 
           "<input id='uart-txpin' type='number' value='44' min='0' max='48' style='padding:8px;border-radius:4px;background:#1a1a1a;color:#e0e0e0;border:1px solid #444;'></div>" 
           "</div>" 
           "<div style='margin-top:15px;'><button class='gemini-btn' onclick='saveUartConfig()'>✅ Apply Configuration</button></div>" 
           "</div>" 
           "<div id='uart-status' class='info-grid' style='margin:10px 0;'>" 
           "<div class='info-item'><strong>Status:</strong> <span id='uart-monitoring-status'>Disabled</span></div>" 
           "<div class='info-item'><strong>Config:</strong> <span id='uart-current-config'>115200 8N1</span></div>" 
           "<div class='info-item'><strong>Pins:</strong> <span id='uart-pins'>RX:43 TX:44</span></div>" 
           "<div class='info-item'><strong>Bytes:</strong> <span id='uart-bytes'>RX:0 TX:0</span></div>" 
           "<div class='info-item'><strong>Buffer:</strong> <span id='uart-buffer-info'>0/1000 (0KB)</span></div>" 
           "<div class='info-item'><strong>Memory:</strong> <span id='uart-memory-usage'>0KB used</span></div>"
           "</div>" 
           "<div id='uart-logs' class='gemini-mono'>UART monitoring disabled...</div>" 
           "</div>"
           "</div>" 
           "<script>"
           "function startCapture(){fetch('/api/start',{method:'POST'}).then(()=>updateAll());}" 
           "function stopCapture(){fetch('/api/stop',{method:'POST'}).then(()=>updateAll());}" 
           "function getData(){fetch('/api/data').then(r=>r.json()).then(d=>document.getElementById('data').innerHTML='<pre>'+JSON.stringify(d,null,2)+'</pre>');}" 
           "function clearData(){fetch('/api/data/clear',{method:'POST'}).then(()=>{document.getElementById('data').innerHTML='No data captured yet...';updateAll();});}"
           "function clearLogs(){fetch('/api/logs/clear',{method:'POST'}).then(()=>loadLogs());}" 
           "function loadLogs(){fetch('/api/logs').then(r=>r.json()).then(d=>{const logs=d.logs.map(log=>'<div style=\"margin-bottom:5px;padding:5px;background:rgba(0,212,255,0.1);border-radius:4px;\">' + log + '</div>').join('');document.getElementById('logs').innerHTML=logs||'No logs available';});}" 
           "function downloadLogs(){window.open('/download/logs','_blank');}" 
           "function downloadData(format){window.open('/download/data?format='+format,'_blank');}" 
           "function enableUartMonitoring(){fetch('/api/uart/enable',{method:'POST'}).then(()=>loadUartLogs());}" 
           "function disableUartMonitoring(){fetch('/api/uart/disable',{method:'POST'}).then(()=>loadUartLogs());}" 
           "function clearUartLogs(){fetch('/api/uart/clear',{method:'POST'}).then(()=>loadUartLogs());}" 
           "function loadUartLogs(){fetch('/api/uart/logs').then(r=>r.json()).then(d=>{document.getElementById('uart-monitoring-status').textContent=d.monitoring_enabled?'Active':'Disabled';const config=d.config;document.getElementById('uart-current-config').textContent=config.baudrate+' '+config.data_bits+config.parity_string.charAt(0)+config.stop_bits;document.getElementById('uart-pins').textContent='RX:'+config.rx_pin+' TX:'+config.tx_pin;document.getElementById('uart-bytes').textContent='RX:'+d.bytes_received+' TX:'+d.bytes_sent;document.getElementById('uart-buffer-info').textContent=d.count+'/'+d.max_entries+' ('+(d.memory_usage/1024).toFixed(1)+'KB)';document.getElementById('uart-memory-usage').textContent=(d.memory_usage/1024).toFixed(1)+'KB used';const logs=d.uart_logs.map(log=>'<div style=\"margin-bottom:5px;padding:5px;background:rgba(156,39,176,0.1);border-radius:4px;\">' + log + '</div>').join('');document.getElementById('uart-logs').innerHTML=logs||'No UART data logged';}).catch(e=>console.error('UART logs error:',e));}" 
           "function compactUartLogs(){fetch('/api/uart/compact',{method:'POST'}).then(()=>loadUartLogs());}"
           "function downloadUartLogs(){window.open('/download/uart','_blank');}" 
           "function toggleUartConfig(){const config=document.getElementById('uart-config');config.style.display=config.style.display==='none'?'block':'none';if(config.style.display==='block'){loadUartConfig();}}" 
           "function loadUartConfig(){fetch('/api/uart/config').then(r=>r.json()).then(d=>{document.getElementById('uart-baudrate').value=d.baudrate;document.getElementById('uart-databits').value=d.data_bits;document.getElementById('uart-parity').value=d.parity;document.getElementById('uart-stopbits').value=d.stop_bits;document.getElementById('uart-rxpin').value=d.rx_pin;document.getElementById('uart-txpin').value=d.tx_pin;}).catch(e=>console.error('UART config load error:',e));}"
           "function saveUartConfig(){const formData=new FormData();formData.append('baudrate',document.getElementById('uart-baudrate').value);formData.append('data_bits',document.getElementById('uart-databits').value);formData.append('parity',document.getElementById('uart-parity').value);formData.append('stop_bits',document.getElementById('uart-stopbits').value);formData.append('rx_pin',document.getElementById('uart-rxpin').value);formData.append('tx_pin',document.getElementById('uart-txpin').value);fetch('/api/uart/config',{method:'POST',body:formData}).then(()=>{loadUartLogs();document.getElementById('uart-config').style.display='none';});}"
           "function updateAll(){" 
           "fetch('/api/status').then(r=>r.json()).then(d=>{" 
           "const indicator=d.capturing?'<span class=\"gemini-indicator capturing\"></span>':'<span class=\"gemini-indicator ready\"></span>';" 
           "document.getElementById('status').innerHTML=indicator+(d.capturing?'Capturing':'Ready')+' | Rate: '+d.sample_rate+'Hz | Buffer: '+d.buffer_usage+'/'+d.buffer_size+' | GPIO1 Only';"
           "document.getElementById('network-mode').textContent = d.ap_mode ? 'Access Point' : (d.wifi_connected ? 'WiFi Client' : 'Disconnected');" 
           "document.getElementById('network-name').textContent = d.wifi_ssid || 'None';" 
           "document.getElementById('network-ip').textContent = d.ip_address || 'None';" 
           "});loadLogs();loadUartLogs();}" 
           "setInterval(updateAll,2000);updateAll();setInterval(loadUartLogs,3000);"
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
