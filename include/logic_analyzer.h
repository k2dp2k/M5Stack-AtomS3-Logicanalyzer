#ifndef LOGIC_ANALYZER_H
#define LOGIC_ANALYZER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>

#ifdef ATOMS3_BUILD
    #include <M5AtomS3.h>
    #include <M5GFX.h>
#endif

// Configuration constants - Optimized for GPIO1 only
#define MAX_CHANNELS 1  // Only GPIO1 for maximum efficiency
#define BUFFER_SIZE 16384  // 4x larger buffer for single channel
#define DEFAULT_SAMPLE_RATE 1000000  // 1MHz
#define MIN_SAMPLE_RATE 1000         // 1kHz
#define MAX_SAMPLE_RATE 10000000     // 10MHz (higher rate possible with single channel)

// GPIO pins for logic analyzer inputs - GPIO1 only for maximum performance
#ifdef ATOMS3_BUILD
    // AtomS3 GPIO1 - Optimized single channel analysis
    #define CHANNEL_0_PIN 1   // GPIO1 - HIGH PERFORMANCE SINGLE CHANNEL
#else
    // Original ESP32 pin for compatibility
    #define CHANNEL_0_PIN 2   // GPIO2 - Single channel mode
#endif

struct Sample {
    uint32_t timestamp;  // Timestamp in microseconds
    bool data;           // Single bit data for GPIO1 (more memory efficient)
};

enum TriggerMode {
    TRIGGER_NONE,
    TRIGGER_RISING_EDGE,
    TRIGGER_FALLING_EDGE,
    TRIGGER_BOTH_EDGES,
    TRIGGER_HIGH_LEVEL,
    TRIGGER_LOW_LEVEL
};

class LogicAnalyzer {
private:
    Sample buffer[BUFFER_SIZE];
    volatile uint16_t writeIndex;
    volatile uint16_t readIndex;
    volatile bool capturing;
    
    uint32_t sampleRate;
    uint8_t gpio1Pin;  // Single GPIO1 pin for optimized performance
    
    // Trigger configuration for GPIO1
    TriggerMode triggerMode;
    bool lastState;      // Single bit for GPIO1 state
    bool triggerArmed;
    
    // Timing
    uint32_t lastSampleTime;
    uint32_t sampleInterval;
    
    // Serial logging
    std::vector<String> serialLogBuffer;
    std::vector<String> uartLogBuffer;
    static const size_t MAX_LOG_ENTRIES = 100;
    static const size_t MAX_UART_ENTRIES = 1000;  // Increased from 200 to 1000 entries
    static const size_t UART_MSG_MAX_LENGTH = 300;  // Max length per UART message
    
    // UART monitoring configuration
    struct UartConfig {
        uint32_t baudrate = 115200;
        uint8_t dataBits = 8;
        uint8_t parity = 0;  // 0=None, 1=Odd, 2=Even
        uint8_t stopBits = 1;
        uint8_t rxPin = 43;  // AtomS3 GPIO43 (G43)
        uint8_t txPin = 44;  // AtomS3 GPIO44 (G44)
        bool enabled = false;
    };
    
    UartConfig uartConfig;
    HardwareSerial* uartSerial;
    bool uartMonitoringEnabled;
    String uartRxBuffer;
    String uartTxBuffer;
    uint32_t lastUartActivity;
    uint32_t uartBytesReceived;
    uint32_t uartBytesSent;
    
    // Preferences for persistent storage
    Preferences* preferences;
    
    // Private methods - optimized for GPIO1
    void initializeGPIO1();
    bool readGPIO1();
    bool checkTrigger(bool currentState);
    void addSample(bool data);
    
public:
    LogicAnalyzer();
    ~LogicAnalyzer();
    
    // Core functions
    void begin();
    void process();
    
    // Capture control
    void startCapture();
    void stopCapture();
    bool isCapturing() const;
    
    // Configuration
    void setSampleRate(uint32_t rate);
    uint32_t getSampleRate() const;
    
    // Trigger configuration for GPIO1
    void setTrigger(TriggerMode mode);
    void disableTrigger();
    TriggerMode getTriggerMode() const;
    
    // Data access
    String getDataAsJSON();
    void clearBuffer();
    uint16_t getBufferUsage() const;
    bool isBufferFull() const;
    
    // Serial logging
    void addLogEntry(const String& message);
    String getLogsAsJSON();
    String getLogsAsPlainText();
    void clearLogs();
    
    // UART logging with configurable settings
    void configureUart(uint32_t baudrate, uint8_t dataBits, uint8_t parity, uint8_t stopBits, uint8_t rxPin, uint8_t txPin);
    void enableUartMonitoring();
    void disableUartMonitoring();
    void addUartEntry(const String& data, bool isRx = true);
    String getUartLogsAsJSON();
    String getUartLogsAsPlainText();
    String getUartConfigAsJSON();
    void clearUartLogs();
    void processUartData();
    void saveUartConfig();
    void loadUartConfig();
    void setPreferences(Preferences* prefs);
    
    // UART buffer management
    size_t getUartLogCount() const;
    size_t getUartMemoryUsage() const;
    bool isUartBufferFull() const;
    void compactUartLogs();  // Remove oldest entries when buffer is getting full
    
    // Data export
    String getDataAsCSV();
    
    // Utility functions
    void printStatus();
    void printChannelStates();
    
#ifdef ATOMS3_BUILD
    // Modern display functions
    void initDisplay();
    void updateDisplay();
    
    // New clean design functions
    void drawGradientBackground();
    void drawHeader();
    void drawInitialStatus();
    void drawStatusSection();
    void drawChannelSection();
    void drawBottomSection();
    void drawCard(int x, int y, int w, int h, uint16_t color, const char* title = nullptr);
    void drawGeminiCard(int x, int y, int w, int h, uint16_t grad1, uint16_t grad2, const char* title = nullptr);
    void updateWiFiStatus(bool isConnected, bool isAP, const String& networkName, const String& ipAddress);
#endif
};

#endif // LOGIC_ANALYZER_H
