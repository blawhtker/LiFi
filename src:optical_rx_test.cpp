#include <Arduino.h>
#include "driver/rmt.h"

// --- Configuration ---
#define PIN_OPTICAL_RX      5       // Connect to Comparator Output (TLV3501)
#define RMT_RX_CHANNEL      RMT_CHANNEL_1 // Channel 0 is used for TX
#define RMT_CLK_DIV         4       // Same as TX: 1 tick = 50ns

// Buffer Size: How many "edges" to capture before processing?
// A standard Ethernet frame is ~12,000 bits (24,000 edges).
// The ESP32 RMT has limited RAM (usually 4 blocks = 256 items).
// We will start small for testing.
#define RMT_RX_MEM_BLOCKS   4       

void setupOpticalRX() {
    rmt_config_t config = RMT_DEFAULT_CONFIG_RX(PIN_OPTICAL_RX, RMT_RX_CHANNEL);
    config.clk_div = RMT_CLK_DIV; // 50ns resolution
    
    // Filter: Ignore spikes shorter than 100ns (noise filtering)
    config.rx_config.filter_en = true;
    config.rx_config.filter_ticks_thresh = 2; // 2 * 50ns = 100ns
    
    // Idle Threshold: If line is quiet for 100us, consider the packet "Done"
    // 100us / 50ns = 2000 ticks
    config.rx_config.idle_threshold = 2000; 

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(RMT_RX_CHANNEL, 0, 0));
    
    Serial.println("Optical RX Listening...");
}

void loop() {
    // 1. Try to read a block of pulses from the RMT ringbuffer
    RingbufHandle_t rb = NULL;
    size_t num_items = 0;
    
    // Check if we have data
    rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);
    if (rb) {
        // "Receive" the raw packet
        rmt_item32_t* items = (rmt_item32_t*) xRingbufferReceive(rb, &num_items, 0);
        
        if (items) {
            // We got a signal! Let's analyze it.
            // num_items is bytes, so divide by size of item to get count
            int pulse_count = num_items / sizeof(rmt_item32_t);
            
            Serial.printf("Captured Packet! Edges: %d\n", pulse_count);
            
            // Print the first 10 pulses to debug timing
            Serial.print("Durations (ticks): ");
            for (int i = 0; i < 10 && i < pulse_count; i++) {
                // Duration is the lower 15 bits
                int duration = items[i].duration0; 
                int level = items[i].level0;
                
                // Print: (Level:Duration)
                Serial.printf("(%d:%d) ", level, duration);
            }
            Serial.println();
            
            // Return the memory to the system
            vRingbufferReturnItem(rb, (void*) items);
        }
    }
    
    // Tiny delay to prevent WDT crash if loop is too tight
    delay(5);
}

void setup() {
    Serial.begin(115200);
    setupOpticalRX();
}