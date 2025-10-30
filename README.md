# EE_FILE - Lightweight EEPROM File System

A minimal and efficient EEPROM management library for embedded systems, designed for Arduino-compatible platforms.

## Features

- **Automatic Address Management**: No manual address calculation needed
- **Validity Tracking**: Built-in data validity flag for reliable power-loss recovery
- **Type-Safe Access**: Use enums instead of raw addresses
- **Minimal Overhead**: Only 1 byte per file for validity marker
- **Multi-Sector Support**: Configurable sector allocation
- **Easy-to-Use API**: Simplified macros for common operations

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
[env:myenv]
lib_deps =
    ee_file
```

Or install via PlatformIO CLI:

```bash
pio pkg install --library "ee_file"
```

## Quick Start

### 1. Define Your File Types

```cpp
#include <eefile.h>

// Define what data you want to store
typedef enum {
    IIC_START = 0,   // I2C address
    KAL_MAN,         // Kalman parameters
    USER_SETTINGS,   // User settings
    END              // Must end with END
} EEFileType;
```

### 2. Initialize and Register Files

```cpp
void setup() {
    // Initialize EEPROM
    EE_INIT();

    // Register files (type, max_size_in_bytes)
    EE_REG(IIC_START, 1);        // 1 byte for I2C address
    EE_REG(KAL_MAN, 4);          // 4 bytes for Kalman params
    EE_REG(USER_SETTINGS, 16);   // 16 bytes for settings
}
```

### 3. Write and Read Data

```cpp
void loop() {
    // Write data
    uint8_t i2c_addr = 0x42;
    EE_WRITE(IIC_START, &i2c_addr, 1);
    EE_SET_VALID(IIC_START, true);  // Mark as valid

    // Read data (check validity first)
    if (EE_IS_VALID(IIC_START)) {
        uint8_t read_addr;
        EE_READ(IIC_START, &read_addr, 1);
        Serial.println(read_addr, HEX);  // 0x42
    }
}
```

## API Reference

### Initialization

```cpp
EE_INIT()                          // Initialize and enable EEPROM
```

### File Registration

```cpp
EE_REG(type, max_size)             // Register file with auto address allocation
```

### Read/Write Operations

```cpp
EE_WRITE(type, data, length)       // Write data to file
EE_READ(type, buffer, length)      // Read data from file
```

### Validity Management

```cpp
EE_IS_VALID(type)                  // Check if file data is valid
EE_SET_VALID(type, valid)          // Set validity flag (true/false)
```

### File Operations

```cpp
EE_ERASE(type)                     // Erase file (sets invalid)
EE_ENABLE(type)                    // Enable file
EE_DISABLE(type)                   // Disable file
```

### Query Functions

```cpp
EE_GET_LEN(type)                   // Get actual data length
EE_IS_MODIFIED(type)               // Check if file was modified
EE_CLEAR_MODIFIED(type)            // Clear modified flag
EE_GET_ADDR(type)                  // Get file start address (debug)
```

### Debug Functions

```cpp
EE_STATUS()                        // Print all files status
EE_INFO(type)                      // Print specific file info
```

## Configuration

Edit `eefile.h` to customize:

```cpp
#define EEFILE_MAX_FILES 10        // Maximum number of files
#define EEFILE_SECTOR_SIZE 256     // Sector size in bytes
#define EEFILE_NUM_SECTORS 2       // Number of sectors to use
```

## Storage Format

Each file is stored as:

```
[Validity Marker: 1 byte] [User Data: N bytes] [Padding: 0xFF]
```

- **Validity Marker**: `0x01` = valid, `0x00` = invalid
- **User Data**: Your actual data
- **Padding**: Unused space filled with `0xFF`

## Example Use Case: Power-Loss Safe Settings

```cpp
typedef enum {
    MOTOR_SPEED = 0,
    SENSOR_CALIB,
    END
} ConfigType;

void setup() {
    EE_INIT();
    EE_REG(MOTOR_SPEED, 2);
    EE_REG(SENSOR_CALIB, 4);

    // On first boot, data is invalid
    if (!EE_IS_VALID(MOTOR_SPEED)) {
        uint16_t default_speed = 1000;
        EE_WRITE(MOTOR_SPEED, &default_speed, 2);
        EE_SET_VALID(MOTOR_SPEED, true);
    }
}

void saveMotorSpeed(uint16_t speed) {
    EE_WRITE(MOTOR_SPEED, &speed, 2);
    EE_SET_VALID(MOTOR_SPEED, true);
    // Even if power lost here, data remains valid
}

uint16_t loadMotorSpeed() {
    if (EE_IS_VALID(MOTOR_SPEED)) {
        uint16_t speed;
        EE_READ(MOTOR_SPEED, &speed, 2);
        return speed;
    }
    return 1000;  // Default value
}
```

## Dependencies

- **EEPROM**: Arduino EEPROM library (or compatible)

## Platform Support

Compatible with any Arduino-framework platform that supports EEPROM, including:

- ATmega series (Arduino Uno, Nano, Mega)
- STM32 (with EEPROM emulation)
- ESP32/ESP8266
- PY32F003 (with Flash-based EEPROM)

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! Please submit issues and pull requests on GitHub.

## Author

luobeifeng <1824246644@qq.com>
