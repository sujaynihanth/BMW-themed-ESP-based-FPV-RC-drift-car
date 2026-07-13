#include <esp_now.h>
#include <WiFi.h>

// MAC Address of the receiver
uint8_t broadcastAddress[] = {0xD4, 0xE9, 0xF4, 0xB3, 0x57, 0x14};

const int steeringPin = 34;
const int throttlePin = 35;
const int modeButton = 4;
const int lightButton = 23; 

typedef struct struct_message {
  int steering;
  int throttle;
  int driveMode;
  bool headlights;
  bool reverseLights;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

int currentMode = 0; 
bool headLightsOn = false;
bool lastLightButtonState = HIGH;

// Safety variables for double-press logic
int lastModeButtonState = HIGH;
unsigned long lastPressTime = 0;
int pressCount = 0;
unsigned long doublePressWindow = 400; 

void setup() {
  // Serial is kept for potential future use but is now passive
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  pinMode(modeButton, INPUT_PULLUP);
  pinMode(lightButton, INPUT_PULLUP);
}

void loop() {
  // Double-press safety logic for Mode Switching
  int modeReading = digitalRead(modeButton);
  if (modeReading == LOW && lastModeButtonState == HIGH) {
    unsigned long currentTime = millis();
    if (currentTime - lastPressTime < doublePressWindow) {
      pressCount++;
    } else {
      pressCount = 1;
    }
    lastPressTime = currentTime;
  }
  
  if (pressCount >= 2) {
    currentMode = (currentMode + 1) % 3;
    pressCount = 0; 
  }
  lastModeButtonState = modeReading;

  // Headlight toggle (Single press)
  int lightReading = digitalRead(lightButton);
  if (lightReading == LOW && lastLightButtonState == HIGH) {
    headLightsOn = !headLightsOn;
  }
  lastLightButtonState = lightReading;

  // Raw Inputs & Scaling
  int rawSteer = analogRead(steeringPin);
  int rawThrottle = analogRead(throttlePin);
  int baseThrottle = map(rawThrottle, 0, 4095, -255, 255);

  int finalSteering;
  float throttleSF;

  if (currentMode == 0) { // Racing
    throttleSF = 1.0;
    finalSteering = map(rawSteer, 0, 4095, 45, 135); 
  } else if (currentMode == 1) { // Drifting
    throttleSF = 1.0;
    finalSteering = map(rawSteer, 0, 4095, 0, 180);
  } else { // Cruise Control
    throttleSF = 0.3;
    finalSteering = map(rawSteer, 0, 4095, 70, 110);
  }

  // Populate data
  myData.steering = finalSteering;
  myData.throttle = (int)(baseThrottle * throttleSF);
  myData.driveMode = currentMode;
  myData.headlights = headLightsOn;
  myData.reverseLights = (myData.throttle < -10);

  // Send packet
  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

  // Minimal delay to maintain stability without blocking transmission
  delay(10);
}
