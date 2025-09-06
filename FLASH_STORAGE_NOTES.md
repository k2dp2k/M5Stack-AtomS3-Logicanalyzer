# Flash Storage Implementation Notes

## Warum Flash Storage für UART Logs?

### Vorteile:
- **Mehr Speicherplatz**: Flash hat ~3MB verfügbar vs. ~300KB RAM
- **Persistenz**: Daten überleben Neustarts und Resets  
- **Größere Buffers**: 100K+ Einträge möglich statt nur 10K
- **Memory-efficient**: RAM bleibt für Echtzeit-Operationen frei

### Nachteile:
- **Langsamerer Zugriff**: ~1ms Flash vs ~1μs RAM
- **Write-Cycles**: Flash hat begrenzte Schreibzyklen (~100K)
- **Komplexere Implementierung**: File-Management erforderlich

## Aktuelle Speicherverwendung (RAM-basiert):

```cpp
// Aktuell im RAM:
std::vector<String> uartLogBuffer;  // bis zu 50K Einträge
static const size_t MAX_UART_ENTRIES = 10000;
static const size_t UART_MSG_MAX_LENGTH = 500;
// = ~25MB potentiell (unrealistisch für ESP32)
```

## Flash-basierte Alternative:

```cpp
// Flash-Storage mit LittleFS:
File uartLogFile = LittleFS.open("/uart_logs.txt", "a");
// Bis zu 3MB verfügbar
// ~6000 Zeilen x 500 Zeichen = ~3MB
// Deutlich größere Kapazität!
```

## Implementation Strategy:

1. **Hybrid Approach**: 
   - Kleine Buffer im RAM für schnellen Zugriff (1000 Einträge)
   - Automatisches Flush zu Flash bei Buffer-Full
   - Flash als Hauptspeicher für große Logs

2. **Configuration Options**:
   - `useFlashStorage = true/false` (Konfigurierbar)
   - Automatische Wahl basierend auf Buffer-Größe
   - < 5K Einträge: RAM
   - > 5K Einträge: Flash

3. **Performance Optimization**:
   - Batch-Writes zu Flash (nicht jeden Eintrag einzeln)
   - Circular buffer Implementierung in Flash
   - Background-Thread für Flash I/O (optional mit FreeRTOS)

## Code Integration:

### Header Changes (bereits gemacht):
```cpp
bool useFlashStorage;   // Use LittleFS instead of RAM
String uartLogFileName; // Flash file name
```

### Implementation Points:
```cpp
void LogicAnalyzer::enableFlashStorage(bool enable) {
    if (enable && !LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    useFlashStorage = enable;
    uartLogFileName = "/uart_logs_" + String(millis()) + ".txt";
}

void LogicAnalyzer::addUartEntry(const String& data, bool isRx) {
    if (useFlashStorage) {
        // Flash-based storage
        File logFile = LittleFS.open(uartLogFileName, "a");
        if (logFile) {
            logFile.println(data);
            logFile.close();
        }
    } else {
        // RAM-based storage (current implementation)
        uartLogBuffer.push_back(data);
    }
}
```

## Memory Comparison:

| Storage Type | Max Entries | Estimated Size | Persistence | Speed |
|--------------|-------------|----------------|-------------|--------|
| RAM Current  | 10,000      | ~5MB           | No          | Fast   |
| RAM Realistic| 1,000       | ~500KB         | No          | Fast   |
| Flash Option | 100,000     | ~50MB          | Yes         | Medium |

## Empfehlung:

1. **Kurzfristig**: Die aktuellen RAM-basierten Buffer sind für die meisten Anwendungen ausreichend
2. **Langfristig**: Flash-Storage als Option implementieren für:
   - Lange Monitoring-Sessions 
   - Debugging über Neustarts hinweg
   - Große Datenmengen

## FreeRTOS Überlegungen:

Das Projekt verwendet **kein** explizites FreeRTOS, sondern nur das Arduino Framework. 
Flash-I/O könnte in einem separaten Task laufen, aber das würde FreeRTOS-Features erfordern:

```cpp
// Optional: Background Flash I/O mit FreeRTOS
xTaskCreatePinnedToCore(
    flashWriteTask,   // Task function
    "FlashWriter",    // Task name
    4096,            // Stack size
    this,            // Task parameter
    1,               // Priority
    &flashTaskHandle,// Task handle
    0                // Core ID
);
```

Aber für die aktuelle Implementierung ist das nicht nötig.
