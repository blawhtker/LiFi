/*
 * 2-Way LiFi Communication Device ("Glow Phone")
 * Based on: "PWM for Sample Values" Modulation Scheme 
 * * HARDWARE MAPPING:
 * - MIC_PIN (A0): Output from Microphone Pre-amplifier [cite: 102]
 * - RX_PIN (A1): Output from Photodiode Transimpedance Amplifier (TIA) [cite: 103]
 * - PTT_PIN (2): Push-to-Talk Button (Active LOW) 
 * - TX_LED_PIN (3): Drives the Transmitting LED (via Driver) [cite: 122]
 * - SPK_PIN (11): Drives the Speaker Audio Amp (via Low Pass Filter) 
 *
 * NOTE: This code uses Direct Register Access for the ATmega328P (Uno/Nano) 
 * to achieve high-speed PWM and fast ADC sampling required for audio.
 */

// Pin Definitions
const int MIC_PIN = A0;      // Audio Input for TX
const int RX_PIN = A1;       // Light Sensor Input for RX
const int PTT_PIN = 2;       // Push Button (Connect to GND when pressed)
const int TX_LED_PIN = 3;    // PWM Output A (Timer 2)
const int SPK_PIN = 11;      // PWM Output B (Timer 2)

// Global variables
byte audioSample;

void setup() {
  // 1. Configure I/O
  pinMode(PTT_PIN, INPUT_PULLUP); // Button is HIGH when open, LOW when pressed
  pinMode(TX_LED_PIN, OUTPUT);
  pinMode(SPK_PIN, OUTPUT);
  
  // 2. Configure ADC for High-Speed Sampling [cite: 295]
  // Standard analogRead() is too slow for good audio. We adjust the prescaler.
  // Set ADC prescaler to 16 (16MHz / 16 = 1MHz ADC clock).
  // This allows sampling rates > 40kHz.
  ADCSRA &= ~(bit (ADPS0) | bit (ADPS1) | bit (ADPS2)); // Clear bits
  ADCSRA |= bit (ADPS2); // Set bit 2 (Prescaler 16)

  // 3. Configure Timer2 for Fast PWM (Carrier Frequency) 
  // We need a carrier frequency > 30kHz so the "whine" isn't audible.
  // This sets up a ~62.5kHz PWM on Pins 3 and 11.
  TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20); // No prescaler
}

void loop() {
  // Check PTT State 
  if (digitalRead(PTT_PIN) == LOW) {
    // === TRANSMIT MODE (Talking) ===
    transmitAudio();
  } else {
    // === RECEIVE MODE (Listening) ===
    receiveAudio();
  }
}

// === TRANSMIT FUNCTION ===
// Captures Mic Audio -> Modulates LED PWM [cite: 102]
void transmitAudio() {
  // 1. Read Analog Audio from Microphone
  // We subtract 512 to center the wave, amplify if needed, then re-center to 0-255 range.
  int input = analogRead(MIC_PIN); 
  
  // 2. Map 10-bit ADC (0-1023) to 8-bit PWM (0-255)
  // This implements the "PWM for Sample Values" modulation 
  // Ideally, your Mic Pre-amp biases the signal to 2.5V (ADC value ~512).
  audioSample = map(input, 0, 1023, 0, 255);

  // 3. Write to LED (Pin 3 - OCR2B)
  OCR2B = audioSample; 
  
  // Turn off Speaker PWM to prevent feedback
  OCR2A = 0; 
}

// === RECEIVE FUNCTION ===
// Captures Photodiode Signal -> Demodulates to Speaker PWM [cite: 103]
void receiveAudio() {
  // 1. Read Analog Signal from Photodiode (via TIA)
  int input = analogRead(RX_PIN);
  
  // 2. Map 10-bit ADC to 8-bit Audio Output
  // This acts as the Demodulation and DAC step [cite: 350, 358]
  audioSample = map(input, 0, 1023, 0, 255);

  // 3. Write to Speaker (Pin 11 - OCR2A)
  // This PWM signal must go through a Low-Pass Filter (RC Filter) 
  // before the Audio Amp to reconstruct analog sound.
  OCR2A = audioSample;

  // Turn off LED to save power/reduce interference
  OCR2B = 0; 
}