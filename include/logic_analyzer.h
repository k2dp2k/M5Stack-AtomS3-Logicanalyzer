#ifndef LOGIC_ANALYZER_H
#define LOGIC_ANALYZER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include <LittleFS.h>

#ifdef ATOMS3_BUILD
    #include <M5AtomS3.h>
    #include <M5GFX.h>
#endif

// Configuration constants - Optimized for GPIO1 only
#define MAX_CHANNELS 1  // Only GPIO1 for maximum efficiency
#define BUFFER_SIZE 16384  // Safe RAM buffer size
#define MAX_BUFFER_SIZE 262144  // Max buffer size (flash storage required)
#define FLASH_BUFFER_SIZE 1000000   // 1M samples in flash (~4.8MB storage) - Default Flash size
#define MAX_FLASH_BUFFER_SIZE 2000000  // 2M samples max (~9.6MB) - Large captures
#define FLASH_CHUNK_SIZE 4096  // Write chunks of 4KB to flash
#define DEFAULT_SAMPLE_RATE 1000000  // 1MHz
#define MIN_SAMPLE_RATE 10           // 10Hz (ultra-low frequency monitoring)
#define MAX_SAMPLE_RATE 40000000     // 40MHz (ESP32-S3 direct register access limit)

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

// Compressed sample structures
struct CompressedSample {
    uint32_t timestamp;  // Base timestamp
    uint16_t count;      // Run length or delta count
    bool data;           // Data value
    uint8_t type;        // Compression type flag
};

// Flash storage metadata
struct FlashStorageHeader {
    uint32_t magic;         // Magic number for validation
    uint32_t version;       // Storage format version
    uint32_t sample_count;  // Total samples stored
    uint32_t buffer_size;   // Buffer configuration
    uint32_t sample_rate;   // Sample rate used
    uint32_t compression;   // Compression type
    uint32_t crc32;         // Data integrity check
};

enum TriggerMode {
    TRIGGER_NONE,
    TRIGGER_RISING_EDGE,
    TRIGGER_FALLING_EDGE,
    TRIGGER_BOTH_EDGES,
    TRIGGER_HIGH_LEVEL,
    TRIGGER_LOW_LEVEL
};

enum BufferMode {
    BUFFER_RAM,           // Standard RAM buffer (24K samples)
    BUFFER_FLASH,         // Flash storage (1M+ samples)
    BUFFER_STREAMING,     // Continuous streaming to flash
    BUFFER_COMPRESSED     // Compressed storage (RLE + Delta)
};

enum CompressionType {
    COMPRESS_NONE,
    COMPRESS_RLE,         // Run-Length Encoding
    COMPRESS_DELTA,       // Delta compression
    COMPRESS_HYBRID       // RLE + Delta combined
};

enum UartDuplexMode {
    UART_FULL_DUPLEX,     // Traditional RX + TX on separate pins
    UART_HALF_DUPLEX      // Single wire bidirectional communication
};

class LogicAnalyzer {
private:
    Sample buffer[BUFFER_SIZE];
    volatile uint32_t writeIndex;
    volatile uint32_t readIndex;
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
    static const size_t MAX_UART_ENTRIES = 1000000;  // 1M entries - utilize full 8MB Flash capacity
    static const size_t UART_MSG_MAX_LENGTH = 1000;  // Increased to 1000 chars per message for longer data
    
    // Logic Analyzer configuration
    struct LogicConfig {
        uint32_t sampleRate = DEFAULT_SAMPLE_RATE;  // 1MHz default
        uint8_t gpioPin = CHANNEL_0_PIN;           // GPIO1 on AtomS3
        TriggerMode triggerMode = TRIGGER_NONE;    // No trigger by default
        uint32_t bufferSize = FLASH_BUFFER_SIZE;   // 1M samples - Default Flash storage
        uint8_t preTriggerPercent = 10;            // % of buffer for pre-trigger data
        BufferMode bufferMode = BUFFER_FLASH;      // Default to Flash buffer for more storage
        CompressionType compression = COMPRESS_NONE; // No compression by default
        bool enabled = true;
        bool streamingMode = false;                // Continuous streaming
        uint32_t maxFlashSamples = FLASH_BUFFER_SIZE; // Flash buffer limit
    };
    
    // UART monitoring configuration
    struct UartConfig {
        uint32_t baudrate = 115200;
        uint8_t dataBits = 8;
        uint8_t parity = 0;  // 0=None, 1=Odd, 2=Even
        uint8_t stopBits = 1;
        uint8_t rxPin = 7;   // AtomS3 GPIO7 (G7) - RX/bidirectional pin
        int8_t txPin = -1;   // TX disabled (not connected) - use signed int8_t
        UartDuplexMode duplexMode = UART_FULL_DUPLEX;  // Default to full duplex
        bool enabled = false;
    };
    
    LogicConfig logicConfig;
    UartConfig uartConfig;
    HardwareSerial* uartSerial;
    bool uartMonitoringEnabled;
    String uartRxBuffer;
    String uartTxBuffer;
    uint32_t lastUartActivity;
    uint32_t uartBytesReceived;
    uint32_t uartBytesSent;
    
    // Half-Duplex specific variables
    bool halfDuplexTxMode;          // True when in TX mode for half-duplex
    uint32_t halfDuplexTxTimeout;   // Timeout for TX mode
    String halfDuplexTxQueue;       // Queue for commands to send
    bool halfDuplexBusy;            // Busy flag for half-duplex operations
    
    // Preferences for persistent storage
    Preferences* preferences;
    
    // Dynamic UART buffer management
    size_t maxUartEntries;  // Configurable max entries
    bool useFlashStorage;   // Use LittleFS instead of RAM for UART logs
    String uartLogFileName; // Flash storage file name
    
    // Advanced Logic Analyzer Flash Storage
    File flashDataFile;     // Flash file handle for samples
    String flashLogicFileName; // Logic analyzer flash file
    uint32_t flashSamplesWritten; // Count of samples written to flash
    uint32_t flashWritePosition;  // Current write position in flash
    bool flashStorageActive;      // Flash storage currently active
    FlashStorageHeader flashHeader; // Flash storage metadata
    
    // Compression state
    CompressedSample* compressedBuffer; // Compressed sample buffer
    uint32_t compressedCount;           // Number of compressed samples
    uint32_t lastTimestamp;             // For delta compression
    bool lastData;                      // For run-length encoding
    uint16_t runLength;                 // Current run length
    
    // Streaming capture state
    bool streamingActive;       // Streaming mode active
    uint32_t streamingCount;    // Total samples streamed
    uint8_t* flashWriteBuffer;  // Write buffer for flash chunks
    uint32_t bufferPosition;    // Position in write buffer
    
    // Private methods - optimized for GPIO1
    void initializeGPIO1();
    bool readGPIO1();
    bool checkTrigger(bool currentState);
    void addSample(bool data);
    
    // Half-Duplex private methods
    void setupHalfDuplexPin(bool txMode);
    void processHalfDuplexQueue();
    void switchToRxMode();
    void switchToTxMode();
    
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
    uint32_t getBufferUsage() const;
    bool isBufferFull() const;
    
    // Serial logging
    void addLogEntry(const String& message);
    String getLogsAsJSON();
    String getLogsAsPlainText();
    void clearLogs();
    
    // Logic Analyzer configuration methods
    void configureLogic(uint32_t sampleRate, uint8_t gpioPin, TriggerMode triggerMode, uint32_t bufferSize, uint8_t preTriggerPercent);
    String getLogicConfigAsJSON();
    void saveLogicConfig();
    void loadLogicConfig();
    float calculateBufferDuration() const;  // Calculate buffer duration in seconds
    
    // UART logging with configurable settings
    void configureUart(uint32_t baudrate, uint8_t dataBits, uint8_t parity, uint8_t stopBits, uint8_t rxPin, int8_t txPin, UartDuplexMode duplexMode = UART_FULL_DUPLEX);
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
    void setUartBufferSize(size_t maxEntries);  // Dynamic buffer sizing
    size_t getMaxUartEntries() const;
    float getUartBufferUsagePercent() const;
    
    // Flash storage options (UART)
    void enableFlashStorage(bool enable = true);  // Switch between RAM and Flash storage
    bool isFlashStorageEnabled() const;
    void initFlashStorage();  // Initialize LittleFS
    void clearFlashUartLogs();  // Clear flash-stored logs
    
    // Half-Duplex UART Communication
    bool sendHalfDuplexCommand(const String& command);  // Send command in half-duplex mode
    bool isHalfDuplexMode() const;                      // Check if in half-duplex mode
    bool isHalfDuplexBusy() const;                      // Check if half-duplex is busy
    String getHalfDuplexStatus() const;                 // Get half-duplex status info
    
    // Advanced Logic Analyzer Flash Storage
    void initFlashLogicStorage();   // Initialize flash for logic analyzer
    void enableFlashBuffering(BufferMode mode, uint32_t maxSamples = FLASH_BUFFER_SIZE);
    void writeToFlash(const Sample& sample);     // Write single sample to flash
    void flushFlashBuffer();        // Flush write buffer to flash
    String getFlashDataAsJSON(uint32_t offset = 0, uint32_t count = 1000);
    void clearFlashLogicData();     // Clear flash logic data
    uint32_t getFlashSampleCount() const;
    float getFlashStorageUsedMB() const;
    
    // Compression Methods
    void enableCompression(CompressionType type);
    void compressSample(const Sample& sample);   // Add sample to compression buffer
    void compressRunLength(bool data, uint32_t timestamp, uint16_t count);
    void compressDelta(uint32_t timestamp, bool data);
    String getCompressedDataAsJSON();
    uint32_t getCompressionRatio() const;        // Returns compression percentage
    void clearCompressedBuffer();
    
    // Streaming Capture
    void enableStreamingMode(bool enable = true);
    void processStreamingSample(const Sample& sample);
    bool isStreamingActive() const;
    uint32_t getStreamingSampleCount() const;
    void stopStreaming();
    
    // Advanced Buffer Management
    void setBufferMode(BufferMode mode);
    BufferMode getBufferMode() const;
    String getBufferModeString() const;
    String getAdvancedStatusJSON() const;        // Comprehensive status
    
    // Data export
    String getDataAsCSV();
    
    // Utility functions
    void printStatus();
    void printChannelStates();
    
#ifdef ATOMS3_BUILD
    // Modern display functions
    void initDisplay();
    void updateDisplay();
    
    // New dual-page system
    void drawStartupLogo();
    void drawWiFiPage();
    void drawSystemPage();
    void switchPage();
    
    // Legacy methods (kept for compatibility)
    void drawNetworkPage();
    void drawSystemStatsPage();
    
    // Utilities
    void drawGradientBackground();
    void drawGlassPanel(int x, int y, int w, int h);
    void drawGeminiCard(int x, int y, int w, int h, uint16_t grad1, uint16_t grad2, const char* title = nullptr);
    void setAPMode(bool isAPMode);   // Set AP mode status
    
    // State variables for dual-page system
    uint8_t current_page = 0;        // 0=WiFi page, 1=System page
    bool ap_mode = false;            // Track AP mode state
    unsigned long lastDisplayUpdate = 0;
    const unsigned long DISPLAY_UPDATE_INTERVAL = 2000; // Update every 2 seconds to reduce blinking
#endif
};

#endif // LOGIC_ANALYZER_H
