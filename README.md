🏎️ ESP32-CAM FPV & VR RC Car
An ultra-low-latency, FPV (First Person View) remote-controlled car powered by dual ESP32 microcontrollers. It uses ESP-NOW for instant, router-free control telemetry and hosts a local Wi-Fi video stream so you can drive it using a smartphone or a VR headset with live telemetry overlays (HUD).

🔍 How It All Works (System Architecture)
This project relies on a seamless division of labor between two ESP32 microcontrollers communicating peer-to-peer:

The Transmitter (Remote Control):

Reads analog inputs from two 10k potentiometers (steering and throttle) powered by a single 18650 cell running through a TP4056 module.

Monitors two push buttons using custom non-blocking logic (a double-press to cycle through driving modes, and a single click to toggle headlights).

Instantly bundles this data into a lightweight struct packet and fires it wirelessly using the ESP-NOW protocol.

The Receiver (The Car):

Operates in a dual-wireless mode (WIFI_AP_STA), simultaneously hosting a local Wi-Fi Access Point (VR_Car_Stream) for the live FPV/VR video feed and listening for incoming ESP-NOW control packets.

Asynchronously parses incoming commands to adjust the steering servo, regulate motor speed via PWM (using the freed-up flash LED pin), and automatically trigger reverse tail lights or headlights.

Streams a high-FPS, low-latency CIF-resolution MJPEG video feed directly to any connected smartphone or VR viewer.

### Parts List

#### Transmitter (Remote Control)
| Part | Quantity |
| :--- | :---: |
| ESP32 Dev Module V1 | 1 |
| 10k Potentiometers | 2 |
| Push Buttons | 2 |
| 18650 Li-ion Battery | 1 |
| TP4056 Charging Module | 1 |

#### Receiver (The RC Car)
| Part | Quantity |
| :--- | :---: |
| ESP32-CAM (AI-Thinker module) | 1 |
| L298N Motor Driver | 1 |
| 3V–9V High-Speed Motor | 1 |
| 9g Servo Motor | 1 |
| White LEDs (Headlights) | 2 |
| Red LEDs (Tail/Reverse Lights) | 2 |
| Custom 2S Battery Setup (2x 18650 Cells) | 1 |
| TP4056 Charging Modules | 2 |
| 1S BMS Units (Salvaged) | 2 |
| 4-Pin Female Connector | 1 |
| 4-Pin Male Connectors | 2 |
🔌 Pin Connections
Transmitter (ESP32)
Steering Potentiometer Signal: GPIO 34

Throttle Potentiometer Signal: GPIO 35

Drive Mode Push Button: GPIO 4

Headlight Push Button: GPIO 2

Potentiometers Power: Outer pins connected in parallel to 3V3 and GND

Push Buttons Common Side: Connected to GND (used with internal pull-ups)

Receiver (ESP32-CAM Car)
L298N IN1: GPIO 14

L298N IN2: GPIO 15

L298N ENA (PWM Speed Control): GPIO 4 (Note: Requires on-board flash LED desoldered or disabled)

Front Headlights (+) (White LEDs): GPIO 13

Tail/Reverse Lights (+) (Red LEDs): GPIO 12

Servo Signal: GPIO 2

🛠️ Setup Guide: How to Find Your Car's MAC Address
Because ESP-NOW requires a target receiver MAC address to pair the remote to the car, you need to flash a quick diagnostic script onto your ESP32-CAM first to grab its unique hardware address.

Step 1: Run the MAC Finder Sketch given in the MAC Adress Finder tab
Step 2: Connect and Configure
1) Open your Arduino IDE Serial Monitor (set to 115200 baud).
2) Copy the formatted broadcastAddress[] array outputted by the serial monitor.
3) Paste that array directly into the top of your Transmitter Code so it knows precisely which car to talk to!

🚀 Driving Modes

1) Racing Mode: 100% throttle response, with restricted steering ($45^\circ$ to $135^\circ$) to prevent spin-outs during high-speed tracks.
2) Drifting Mode (#UnleashedBeast): 100% throttle response and a full $0^\circ$ to $180^\circ$ steering sweep for maximum slide control and agility.
3) Cruise Control / Beginner Mode: Capped at 35% throttle with cushioned steering limits ($70^\circ$ to $110^\circ$), perfect for learning the FPV layout safely or handing the controller to friends.

### Digital Transmitter Schematic
![Transmitter Schematic](transmitter_schematic.png)
