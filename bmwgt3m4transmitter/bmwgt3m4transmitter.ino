#include <esp_now.h>
#include <WiFi.h>

// YOUR CAR'S MAC ADDRESS
uint8_t broadcastAddress[] = {0xD4, 0xE9, 0xF4, 0xB3, 0x57, 0x14};

// Pin definitions for your controller
const int steeringPin = 34; // Potentiometer
const int throttlePin = 35; // Potentiometer or Trigger
const int lightPin = 23;    // Button for headlights

// The exact structure matching the receiver
typedef struct struct_message {
  int steering;
  int throttle;
  int headlight;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Callback function for ESP32 Core 3.x
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("\r\nPacket Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  pinMode(lightPin, INPUT_PULLUP);
}

void loop() {
  // Read inputs
  int targetSteer = analogRead(steeringPin);
  int targetSpeed = analogRead(throttlePin);
  
  // Map values properly
  myData.steering = map(targetSteer, 0, 4095, 0, 180);
  myData.throttle = map(targetSpeed, 0, 4095, -255, 255); // Forward/Reverse
  myData.headlight = !digitalRead(lightPin); // Active low button

  // Send the data package
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  delay(50);
}