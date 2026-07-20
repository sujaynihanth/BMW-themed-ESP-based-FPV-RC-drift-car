#include <esp_now.h>
#include <WiFi.h>

// Replace with your validated receiver MAC address array
uint8_t broadcastAddress[] = {0xD4, 0xE9, 0xF4, 0xB3, 0x57, 0x14};

const int steeringPin = 34;
const int throttlePin = 35;
const int modeButton = 4;   // Drive mode push button on GPIO 4
const int lightButton = 2;  // Head light push button on GPIO 2 (Corrected from rough notes)

typedef struct struct_message {
  int steering;
  int throttle;
  int driveMode;
  bool headlights;
  bool reverseLights;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

int currentMode = 0; // 0: Racing, 1: Drift, 2: Cruise Control
bool headLightsOn = false;
bool lastLightButtonState = HIGH;

// Non-blocking double-press timing structure
int lastModeButtonState = HIGH;
unsigned long lastPressTime = 0;
int pressCount = 0;
unsigned long doublePressWindow = 400; 

void setup() {
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
  // --- Non-Blocking Rapid Double-Press Mode Switching Logic ---
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
    currentMode = (currentMode + 1) % 3; // Cycles seamlessly 0 -> 1 -> 2 -> 0
    pressCount = 0; 
  }
  lastModeButtonState = modeReading;

  // --- Headlight Single-Press Toggle Logic ---
  int lightReading = digitalRead(lightButton);
  if (lightReading == LOW && lastLightButtonState == HIGH) {
    headLightsOn = !headLightsOn;
  }
  lastLightButtonState = lightReading;

  // --- Analog Input Mapping & Profile Physics Execution ---
  int rawSteer = analogRead(steeringPin);
  int rawThrottle = analogRead(throttlePin);
  int baseThrottle = map(rawThrottle, 0, 4095, -255, 255);

  int finalSteering;
  float throttleSF = 1.0;

  if (currentMode == 0) { 
    // 1) Racing Mode: Full speed response, restricted steering angle (45° - 135°)
    throttleSF = 1.0;
    finalSteering = map(rawSteer, 0, 4095, 45, 135); 
  } 
  else if (currentMode == 1) { 
    // 2) Drift Mode: Full speed response, full 0° - 180° sweep
    throttleSF = 1.0;
    finalSteering = map(rawSteer, 0, 4095, 0, 180);
  } 
  else { 
    // 3) Cruise Control / Beginner Mode: Capped at 35% throttle multiplier, cushioned steering limits (70° - 110°)
    throttleSF = 0.35;
    finalSteering = map(rawSteer, 0, 4095, 70, 110);
  }

  // --- Populate Data Payload Structure ---
  myData.steering = finalSteering;
  myData.throttle = (int)(baseThrottle * throttleSF);
  myData.driveMode = currentMode;
  myData.headlights = headLightsOn;
  myData.reverseLights = (myData.throttle < -10);

  // Broadcast data securely via ESP-NOW
  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

  delay(10);
}
