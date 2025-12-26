#include <Arduino.h>
#include <ETH.h>

// This flag lets us know if we have a hardware link
static bool eth_connected = false;

// Event Handler: The "Interrupt" that tells you what's happening
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // Set the hostname so you can find it on your router
      ETH.setHostname("esp32-spear-bridge");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Cable Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.println(", FULL_DUPLEX");
      }
      Serial.print(", Speed: ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Cable Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- S.P.E.A.R. Hardware Test ---");

  // Register the event handler
  WiFi.onEvent(WiFiEvent);

  // Initialize the ETH Interface
  // Since we defined pins in platformio.ini, we can just call begin()!
  ETH.begin();
}

void loop() {
  if (eth_connected) {
    // If you see this, your Day 1 Hardware goal is complete.
    Serial.println("Hardware Link Active - Ready for Optical Code");
    delay(5000);
  }
}
void loop() {
    // 1. Poll Optical RX
    RingbufHandle_t rb = NULL;
    size_t num_items = 0;
    rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);

    if (rb) {
        rmt_item32_t* items = (rmt_item32_t*) xRingbufferReceive(rb, &num_items, 0);
        if (items) {
            // We have raw pulses!
            int item_count = num_items / sizeof(rmt_item32_t);
            
            // Prepare a buffer for the decoded Ethernet frame
            uint8_t decoded_frame[1600]; 
            
            // Run the Decoder
            int len = decode_manchester(items, item_count, decoded_frame);
            
            if (len > 0) {
                Serial.printf("Decoded %d bytes from optical link!\n", len);
                
                // Inject into Ethernet (Send to Laptop)
                eth_phy_inject(decoded_frame, len);
            }

            vRingbufferReturnItem(rb, (void*) items);
        }
    }
}