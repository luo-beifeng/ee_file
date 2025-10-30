/**
 * EE_FILE Basic Usage Example
 *
 * This example demonstrates how to use the EE_FILE library
 * to store and retrieve data from EEPROM with validity tracking.
 */

#include <Arduino.h>
#include <eefile.h>

// Step 1: Define your file types
typedef enum {
    IIC_START = 0,   // I2C address storage
    KAL_MAN,         // Kalman filter parameters
    USER_SETTINGS,   // User configuration
    END              // Must end with END
} EEFileType;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== EE_FILE Basic Example ===\n");

    // Step 2: Initialize EEPROM
    EE_INIT();
    Serial.println("[1] EEPROM initialized");

    // Step 3: Register files (addresses auto-allocated)
    EE_REG(IIC_START, 1);        // 1 byte for I2C address
    EE_REG(KAL_MAN, 4);          // 4 bytes for Kalman parameters
    EE_REG(USER_SETTINGS, 16);   // 16 bytes for user settings
    Serial.println("[2] Files registered");

    // Step 4: Display file allocation info
    Serial.println("\n[3] File allocation:");
    EE_STATUS();

    // ========== Example 1: Simple Write/Read ==========
    Serial.println("\n=== Example 1: Simple Write/Read ===");

    uint8_t i2c_addr = 0x42;
    Serial.print("Writing I2C address: 0x");
    Serial.println(i2c_addr, HEX);

    EE_WRITE(IIC_START, &i2c_addr, 1);
    EE_SET_VALID(IIC_START, true);  // Mark as valid

    // Read back
    if (EE_IS_VALID(IIC_START)) {
        uint8_t read_addr;
        EE_READ(IIC_START, &read_addr, 1);
        Serial.print("Read I2C address: 0x");
        Serial.println(read_addr, HEX);
    }

    // ========== Example 2: Struct Storage ==========
    Serial.println("\n=== Example 2: Struct Storage ===");

    struct KalmanParams {
        float Q;  // Process noise
        float R;  // Measurement noise
    } kalman;

    kalman.Q = 0.01;
    kalman.R = 0.5;

    Serial.print("Writing Kalman params: Q=");
    Serial.print(kalman.Q, 4);
    Serial.print(", R=");
    Serial.println(kalman.R, 4);

    EE_WRITE(KAL_MAN, &kalman, sizeof(kalman));
    EE_SET_VALID(KAL_MAN, true);

    // Read back
    if (EE_IS_VALID(KAL_MAN)) {
        KalmanParams read_kalman;
        EE_READ(KAL_MAN, &read_kalman, sizeof(read_kalman));
        Serial.print("Read Kalman params: Q=");
        Serial.print(read_kalman.Q, 4);
        Serial.print(", R=");
        Serial.println(read_kalman.R, 4);
    }

    // ========== Example 3: Power-Loss Safe Config ==========
    Serial.println("\n=== Example 3: Power-Loss Safe Config ===");

    struct UserConfig {
        uint8_t brightness;
        uint8_t volume;
        uint16_t timeout_ms;
    } config;

    // Check if valid config exists
    if (EE_IS_VALID(USER_SETTINGS)) {
        // Load existing config
        EE_READ(USER_SETTINGS, &config, sizeof(config));
        Serial.println("Loaded existing config:");
    } else {
        // First boot - use defaults
        config.brightness = 128;
        config.volume = 50;
        config.timeout_ms = 5000;

        EE_WRITE(USER_SETTINGS, &config, sizeof(config));
        EE_SET_VALID(USER_SETTINGS, true);
        Serial.println("First boot - saved default config:");
    }

    Serial.print("  Brightness: ");
    Serial.println(config.brightness);
    Serial.print("  Volume: ");
    Serial.println(config.volume);
    Serial.print("  Timeout: ");
    Serial.print(config.timeout_ms);
    Serial.println(" ms");

    // ========== Example 4: File Erase ==========
    Serial.println("\n=== Example 4: File Erase ===");

    Serial.println("Erasing IIC_START...");
    EE_ERASE(IIC_START);

    if (!EE_IS_VALID(IIC_START)) {
        Serial.println("File erased - data is now invalid");
    }

    // ========== Final Status ==========
    Serial.println("\n=== Final Status ===");
    EE_STATUS();

    Serial.println("\nSetup complete!");
}

void loop() {
    // Nothing to do in loop
    delay(1000);
}
