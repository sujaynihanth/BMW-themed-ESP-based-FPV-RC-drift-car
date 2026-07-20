#include "esp_camera.h"
#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h> 
#include "camera_pins.h"

// --- Physical Pin Allocations ---
#define SERVO_PIN      2
#define MOTOR_IN1      14
#define MOTOR_IN2      15
#define ENA_PIN        4   // PWM Speed Control via Freed Flash LED Pin (GPIO 4)
#define HEADLIGHT_PIN  13  // Front Headlights (Toggles On/Off)
#define BACKLIGHT_PIN  12  // Reverse Backlights (Automatic)

// --- Wi-Fi AP Settings ---
const char* ssid = "VR_Car_Stream";
const char* password = "12345678";

// --- ESP-NOW Synchronized Structure (Strictly Uniform) ---
typedef struct struct_message {
  int steering;
  int throttle;
  int driveMode;
  bool headlights;
  bool reverseLights;
} struct_message;

struct_message incomingReadings;
Servo steeringServo;

// Global state trackers for HUD display
int currentActiveMode = 0; 
bool headlightState = false;

// Function declarations from app_httpd.cpp
void startCameraServer();
void updateHUDState(int mode, bool lights);

// ESP-NOW Receive Callback with bounds-checking
void OnDataRecv(const esp_now_recv_info_t * recv_info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) {
    return; // Drop corrupted packets to prevent memory faults
  }
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  // 1. Constrain & Smooth Steering Servo Control
  int servoAngle = incomingReadings.steering;
  if(servoAngle < 0) servoAngle = 0;
  if(servoAngle > 180) servoAngle = 180;
  steeringServo.write(servoAngle);

  // 2. Throttle, Direction & ENA PWM Speed Control Logic
  int speed = incomingReadings.throttle;
  if (speed > 10) { 
    // Forward
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(ENA_PIN, speed);
  } 
  else if (speed < -10) { 
    // Reverse
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    analogWrite(ENA_PIN, abs(speed));
  } 
  else { 
    // Idling / Stopped
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(ENA_PIN, 0);
  }
  
  // Apply automatic reverse backlights based on struct flag
  digitalWrite(BACKLIGHT_PIN, incomingReadings.reverseLights ? HIGH : LOW);

  // 3. Headlights & Mode Updates from Transmitter
  headlightState = incomingReadings.headlights;
  digitalWrite(HEADLIGHT_PIN, headlightState ? HIGH : LOW);

  currentActiveMode = incomingReadings.driveMode;
  updateHUDState(currentActiveMode, headlightState);
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240); // Lock CPU frequency to max for peak web server performance

  // Initialize Hardware Output Pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  
  // Allocate hardware PWM timers exclusively for smooth servo tracking
  ESP32PWM::allocateTimer(0);
  steeringServo.setPeriodHertz(50); 
  steeringServo.attach(SERVO_PIN, 500, 2400);

  // --- High-Speed Camera Hardware Configuration ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // --- ANTI-LAG PERFORMANCE TUNING ---
  config.frame_size = FRAMESIZE_CIF;  // 400x296 resolution for a massive FPS boost
  config.jpeg_quality = 14;          // Higher compression = lighter weight packets over Wi-Fi
  config.fb_count = 3;               // Extra frame buffer prevents pipeline stalls

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return;
  }

  // --- INVERSION FIXES ---
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);   // Flips upside down video right-side up
    s->set_hmirror(s, 1); // Fixes left-to-right mirror swap
  }

  // Dual Wireless Modes (Allows ESP-NOW + AP Web Server simultaneously)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);
  
  if (esp_now_init() != ESP_OK) {
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  
  // Launch VR Stream Server
  startCameraServer();
}

void loop() {
  delay(1); // Keeps the background FreeRTOS task watchdog happy
}
