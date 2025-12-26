#include <Arduino.h>
#include "driver/rmt.h"

// --- Tuning Constants (Adjust based on your RMT_CLK_DIV) ---
// Assuming 1 tick = 50ns (from previous step)
// Target Short = 500ns (10 ticks). Target Long = 1000ns (20 ticks).
// We add tolerance margins (+/- 2-3 ticks) to handle noise.

#define TICK_SHORT_MIN  7
#define TICK_SHORT_MAX  13
#define TICK_LONG_MIN   17
#define TICK_LONG_MAX   23

// --- Manchester Decoder Function ---
// Returns number of bytes decoded. Fills 'out_buf' with data.
int decode_manchester(rmt_item32_t* items, size_t item_count, uint8_t* out_buf) {
    int byte_idx = 0;
    int bit_idx = 7;     // MSB first
    uint8_t current_byte = 0;
    
    // State Machine Variables
    // Level: 0 or 1. We track what level we just finished processing.
    int last_level = -1; 
    
    // We expect the first edge to be the start of a bit.
    // In IEEE 802.3 Manchester:
    // Logic 1 = Low -> High transition
    // Logic 0 = High -> Low transition
    
    for (size_t i = 0; i < item_count; i++) {
        int duration = items[i].duration0;
        int level = items[i].level0; // The level DURING this duration

        // 1. Classification
        bool is_short = (duration >= TICK_SHORT_MIN && duration <= TICK_SHORT_MAX);
        bool is_long  = (duration >= TICK_LONG_MIN && duration <= TICK_LONG_MAX);
        
        if (!is_short && !is_long) {
            // Noise glitch! Reset or ignore.
            // For now, let's just skip this item (or abort packet).
            continue; 
        }

        // 2. State Machine
        // Logic: A "Long" pulse means we skipped a transition at the boundary.
        // A "Short" pulse means we are halfway through a bit (or just starting one).
        
        // This simple decoder assumes we are synced to the start of the packet.
        // A robust implementation uses a PLL, but this works for "Store and Forward".
        
        // --- Simplified Logic for "Day 1" ---
        // If we see a transition, we record the bit.
        // Since we are capturing *edges*, every item IS a transition.
        
        // If current level is HIGH, it means we transitioned TO High.
        // Low->High = Logic 1.
        
        // Wait... RMT gives us duration of the level.
        // If we have a LONG pulse, it counts for TWO half-bit windows.
        
        // Let's use a "Half-Bit Counter" approach. It's easier to debug.
        int half_bits = is_long ? 2 : 1;

        for (int h = 0; h < half_bits; h++) {
            // We need to sample the level at the SECOND half of the bit period
            // to determine the value (IEEE 802.3 standard).
            
            // To do this properly, we need a "Phase" tracker.
            // Phase 0: First half of bit. Phase 1: Second half of bit.
            static int phase = 0; 
            
            if (phase == 1) {
                // We are in the second half. The level HERE determines the bit.
                // If Level is High, it was Low->High (Logic 1).
                // If Level is Low, it was High->Low (Logic 0).
                
                if (level == 1) {
                    current_byte |= (1 << bit_idx);
                }
                // (If level is 0, bit remains 0)

                bit_idx--;
                if (bit_idx < 0) {
                    out_buf[byte_idx++] = current_byte;
                    current_byte = 0;
                    bit_idx = 7;
                }
            }
            
            // Flip phase for next half-bit
            phase = !phase;
        }
    }
    return byte_idx;
}