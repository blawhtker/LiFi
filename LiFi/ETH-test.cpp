#include <ETH.h>
#include <WiFi.h>

// --- HARDWARE PIN DEFINITIONS ---
// Optical Interface (Using Hardware Serial1)
#define OPTICAL_TX_PIN  4   // Connected to TC4427 Gate Driver Input
#define OPTICAL_RX_PIN  5   // Connected to Comparator Output

// Ethernet PHY (LAN8720) - Standard ESP32-S3 RMII Pinout
#define ETH_PHY_ADDR    1
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_PHY_POWER   -1  // No external power pin control needed
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN // 50MHz Clock from PHY to GPIO 0

// --- CONFIGURATION ---
#define SERIAL_BAUD     921600  // Start with ~1 Mbps for stability. Max is ~4-5 Mbps.
#define UDP_PORT        8888    // Port to listen for "Air Gap" traffic

// Globals
WiFiUDP udp;
char packetBuffer[1024];

void setup() {
  // 1. Debug Console (USB)
  Serial.begin(115200);
  while (!Serial);
  Serial.println(">>> SYSTEM DIAGNOSTIC STARTING <<<");

  // 2. Optical Link Init (Hardware UART)
  // This maps the Serial1 hardware to your LED and Sensor pins.
  // Invert logic? NO (LED ON = High, LED OFF = Low)
  Serial1.begin(SERIAL_BAUD, SERIAL_8N1, OPTICAL_RX_PIN, OPTICAL_TX_PIN);
  Serial.println("[OK] Optical Hardware Initialized (UART1)");

  // 3. Ethernet Init
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_TYPE, ETH_CLK_MODE);
  Serial.print("[...] Connecting to Ethernet...");
  
  // Wait for IP
  unsigned long timeout = millis();
  while (!ETH.linkUp() && millis() - timeout < 5000) {
    delay(100);
    Serial.print(".");
  }
  
  if (ETH.linkUp()) {
    Serial.println("\n[OK] Ethernet Connected!");
    Serial.print("     IP Address: ");
    Serial.println(ETH.localIP());
    udp.begin(UDP_PORT);
  } else {
    Serial.println("\n[FAIL] Ethernet not detected. Check PHY wiring/Clock.");
  }
}

void loop() {
  // --- PATH 1: ETHERNET -> LIGHT (TX) ---
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 1024);
    if (len > 0) {
      packetBuffer[len] = 0; // Null terminate
      
      // 1. Log to USB
      Serial.printf("TX [Net->Air]: %s\n", packetBuffer);
      
      // 2. Blast out the LED (Hardware UART)
      Serial1.write((const uint8_t*)packetBuffer, len);
    }
  }

  // --- PATH 2: LIGHT -> ETHERNET (RX) ---
  if (Serial1.available()) {
    // Read the incoming light pulses
    int len = 0;
    while(Serial1.available() && len < 1024) {
      packetBuffer[len++] = Serial1.read();
      delayMicroseconds(10); // Small buffer wait
    }
    
    // 1. Log to USB
    Serial.printf("RX [Air->Net]: %.*s\n", len, packetBuffer);

    // 2. Echo back to Computer via UDP (if we know the sender)
    // For benchtop, we just broadcast or reply to last known IP
    if (udp.remoteIP()) {
      udp.beginPacket(udp.remoteIP(), UDP_PORT);
      udp.write((const uint8_t*)packetBuffer, len);
      udp.endPacket();
    }
  }
}