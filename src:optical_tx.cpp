#include <Arduino.h>
#include "driver/rmt.h"

// --- Configuration ---
#define PIN_OPTICAL_TX      4       // Your LED Driver Pin
#define RMT_TX_CHANNEL      RMT_CHANNEL_0

// Speed Settings
// 80MHz (APB Clock) / 4 = 20MHz counter.
// 1 tick = 50ns.
// For 1 Mbps Manchester, we need 500ns pulses (half-bit width).
// 500ns / 50ns = 10 ticks.
#define RMT_CLK_DIV         4
#define PULSE_TICK_WIDTH    10      

// --- Setup the RMT Peripheral ---
void setupOpticalTX() {
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(PIN_OPTICAL_TX, RMT_TX_CHANNEL);
    config.clk_div = RMT_CLK_DIV; // Set resolution to 50ns
    
    // Install the driver
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(RMT_TX_CHANNEL, 0, 0));
    
    Serial.println("Optical TX Initialized at ~1 Mbps (Manchester)");
}

// --- The Manchester Encoder ---
// Converts one byte (8 bits) into 16 RMT items (transitions)
// Logic: 
// Data 1 -> Low-to-High (0 -> 1)
// Data 0 -> High-to-Low (1 -> 0)
void sendByte(uint8_t data) {
    // We need 8 bits * 2 items per bit = 16 items
    rmt_item32_t items[16]; 

    for (int bit = 7; bit >= 0; bit--) {
        int idx = (7 - bit) * 2;
        bool isOne = (data >> bit) & 0x01;

        if (isOne) {
            // Manchester 1: Low for 500ns, then High for 500ns
            // Note: The structure is {duration, level, duration, level}
            items[idx] =   {{{ PULSE_TICK_WIDTH, 0, PULSE_TICK_WIDTH, 1 }}};
        } else {
            // Manchester 0: High for 500ns, then Low for 500ns
            items[idx] =   {{{ PULSE_TICK_WIDTH, 1, PULSE_TICK_WIDTH, 0 }}};
        }
    }
    
    // Send the block (Blocking mode for simplicity in this test)
    rmt_write_items(RMT_TX_CHANNEL, items, 16, true);
}

// --- Main Test Loop ---
void setup() {
    Serial.begin(115200);
    setupOpticalTX();
}

void loop() {
    // Send a "Preamble" pattern (0xAA = 10101010)
    // This creates a perfect square wave 1010... on the wire.
    // Great for checking with an oscilloscope!
    sendByte(0xAA);
    sendByte(0xAA);
    sendByte(0xAA);
    sendByte(0xAA);
    
    // Add a tiny delay so we don't flood the serial monitor (optional)
    // In real code, you remove this delay to transmit continuously.
    delay(10); 
}