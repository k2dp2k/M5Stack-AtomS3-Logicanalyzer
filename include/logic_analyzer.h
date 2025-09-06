#ifndef LOGIC_ANALYZER_H
#define LOGIC_ANALYZER_H

#include <Arduino.h>
#include <ArduinoJson.h>
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
    static const size_t MAX_LOG_ENTRIES = 100;
    
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
