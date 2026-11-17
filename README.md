# X FORCE – Push-up Posture Monitoring System

An ESP32-based push-up posture monitoring system that uses four MPU6050 motion sensors and an ultrasonic distance sensor to monitor upper-body movement, provide real-time feedback, and count push-up repetitions through a Web Bluetooth dashboard.

## 1. Project Overview

X FORCE is a wearable posture-monitoring project designed to help users observe their upper-body alignment and movement while performing push-ups.

The system uses four MPU6050 sensors positioned on the upper arms and wrists. An ESP32 collects sensor readings through a TCA9548A I2C multiplexer, processes the orientation data, and transmits it wirelessly to a browser-based dashboard using Bluetooth Low Energy (BLE).

An HC-SR04 ultrasonic sensor measures the distance between the user's body and the sensor. The dashboard displays this distance alongside live sensor readings.

## 2. Project Objectives

- Monitor upper-body movement during push-ups.
- Measure sagittal and lateral orientation angles using MPU6050 sensors.
- Display live sensor readings through a web dashboard.
- Record a reference movement profile for comparison.
- Detect push-up repetitions and classify them according to the recorded movement range.
- Provide feedback to help users identify possible posture inconsistencies.

## 3. System Architecture

The system consists of three main sections:

**Input Layer**
- Four MPU6050 accelerometer/gyroscope modules.
- One HC-SR04 ultrasonic distance sensor.

**Processing Layer**
- ESP32 microcontroller.
- TCA9548A I2C multiplexer.
- Sensor data acquisition and angle calculations.
- Data packaging and BLE transmission.

**Output Layer**
- Browser-based dashboard.
- Live sensor readings.
- Calibration and reference recording.
- Repetition counting and posture feedback.

## 4. Hardware Components

| Component | Quantity | Purpose |
|---|---:|---|
| ESP32 DevKit | 1 | Main controller and BLE communication |
| MPU6050 | 4 | Measures motion and orientation |
| TCA9548A | 1 | Connects multiple MPU6050 sensors |
| HC-SR04 | 1 | Measures distance |
| Jumper wires | As required | Electrical connections |
| Power supply | 1 | Powers the system |

## 5. Sensor Placement

The four MPU6050 sensors are assigned as follows:

| TCA9548A Channel | Sensor Position |
|---|---|
| Channel 2 | Right Upper Arm |
| Channel 3 | Right Wrist |
| Channel 4 | Left Upper Arm |
| Channel 5 | Left Wrist |

This arrangement allows the system to monitor movement on both sides of the upper body.

## 6. Circuit Connections

### ESP32 to TCA9548A

| TCA9548A Pin | ESP32 Pin |
|---|---|
| SDA | GPIO 22 |
| SCL | GPIO 21 |
| VCC | 3.3V |
| GND | GND |

### MPU6050 Connections

Each MPU6050 connects to its corresponding TCA9548A channel.

| MPU6050 Pin | Connection |
|---|---|
| SDA | Selected TCA9548A channel SDA |
| SCL | Selected TCA9548A channel SCL |
| VCC | Regulated 3.3V |
| GND | Common GND |

All four MPU6050 modules use the I2C address `0x68`.

### HC-SR04 to ESP32

| HC-SR04 Pin | ESP32 Connection |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | GPIO 5 |
| ECHO | GPIO 18 through a voltage divider |

**Important:** The HC-SR04 ECHO output is 5V, while ESP32 GPIO pins are not 5V tolerant. A voltage divider must be used to reduce the ECHO signal before connecting it to GPIO 18.

Example voltage divider:
- ECHO → 1kΩ resistor → GPIO 18
- GPIO 18 → 2kΩ resistor → GND

All circuit grounds must be connected together.

## 7. Software and Technologies

- Arduino IDE
- ESP32 Arduino Core
- C++ / Arduino programming
- I2C communication
- BLE (Bluetooth Low Energy)
- HTML
- CSS
- JavaScript
- Web Bluetooth API
- GitHub Pages for dashboard hosting

## 8. Firmware Description

The ESP32 firmware performs the following operations:

1. Initializes I2C communication.
2. Selects individual TCA9548A channels.
3. Reads accelerometer data from the four MPU6050 sensors.
4. Applies exponential smoothing to the accelerometer readings.
5. Calculates sagittal and lateral orientation angles.
6. Reads distance data from the HC-SR04.
7. Packs sensor readings into an 18-byte data packet.
8. Transmits the packet through BLE notifications.
9. Restarts BLE advertising after a client disconnects.

The firmware uses a nominal 50 ms update interval. The actual update rate may be slower depending on sensor communication and ultrasonic measurement time.

### Angle Calculations

The firmware calculates the angles using the following formulas:

```cpp
float sag = atan2(ay[i], -ax[i]) * 57.2958;
float lat = atan2(az[i], -ax[i]) * 57.2958;
```

These are accelerometer-based orientation estimates. They are not full gyroscope-and-accelerometer sensor fusion or direct anatomical elbow-joint measurements.

## 9. Bluetooth Low Energy Communication

The ESP32 advertises itself using the BLE device name:

`PushupCoach`

### BLE UUIDs

**Service UUID**
```text
12345678-1234-1234-1234-1234567890ab
```

**Characteristic UUID**
```text
12345678-1234-1234-1234-1234567890ac
```

The characteristic sends notifications containing an 18-byte packet.

### Data Packet Structure

| Bytes | Data |
|---|---|
| 0–3 | Right upper arm: sagittal and lateral angles |
| 4–7 | Right wrist: sagittal and lateral angles |
| 8–11 | Left upper arm: sagittal and lateral angles |
| 12–15 | Left wrist: sagittal and lateral angles |
| 16–17 | Ultrasonic distance in millimetres |

Each sensor record contains:
- Signed 16-bit sagittal angle × 100.
- Signed 16-bit lateral angle × 100.

The distance is an unsigned 16-bit value.

All values are transmitted in little-endian format.

**Ultrasonic no-echo condition:** The firmware sends `0` when no valid echo is received.

## 10. Web Dashboard

The browser-based dashboard connects to the ESP32 using the Web Bluetooth API.

Its main functions include:

- BLE device connection and disconnection.
- Live sensor data display.
- Right and left arm monitoring.
- Wrist and upper-arm angle readings.
- Ultrasonic distance display.
- Reference movement recording.
- Standing calibration.
- Push-up repetition detection.
- Good and bad repetition counters.
- Posture feedback and activity logs.
- Adjustable angle tolerance.

The dashboard is designed to make sensor information easier to understand without requiring users to inspect raw serial data.

## 11. Calibration and Reference Recording

The system uses calibration and reference recording to establish individual movement values.

**Standing Calibration**

The user stands in the required calibration position so the dashboard can record baseline sensor readings.

**Reference Movement Recording**

The user records a reference push-up movement. The recorded upper and lower movement positions are used as individual targets during repetition detection.

This allows the system to compare subsequent repetitions against the user's own recorded movement range.

## 12. Push-up Repetition Detection

The dashboard uses the recorded reference values to identify the movement phases of a push-up.

The detection process includes:

1. Detecting movement away from the upper position.
2. Tracking the lowest angle reached by each arm.
3. Checking whether both arms reach their respective recorded lower targets.
4. Detecting the return to the upper position.
5. Counting the completed repetition.
6. Classifying the repetition according to whether both arms reached the required movement range.

The angle tolerance can be adjusted through the dashboard.

The ultrasonic distance is displayed as an additional measurement and is not used as the basis for the current repetition classification.

## 13. Installation and Setup

### ESP32 Firmware

1. Install the Arduino IDE.
2. Install the ESP32 board package.
3. Connect the ESP32 to the computer.
4. Open the firmware file from the `firmware` folder.
5. Select the correct ESP32 board and COM port.
6. Upload the firmware.
7. Open the dashboard and connect to the ESP32 through BLE.

### Web Dashboard

The dashboard can be hosted using GitHub Pages.

1. Create a GitHub repository.
2. Upload `index.html` to the repository root.
3. Open **Settings → Pages**.
4. Select **Deploy from a branch**.
5. Select the `main` branch and `/(root)` folder.
6. Save the settings.
7. Open the generated HTTPS website in a browser that supports Web Bluetooth.

Web Bluetooth requires a secure context, such as HTTPS or localhost. Browser and device compatibility may vary.

## 14. Repository Structure

```text
xforce-pushup-posture-monitor/
│
├── README.md
├── index.html
│
├── firmware/
│   └── xforce_esp32.ino
│
├── docs/
│   ├── system_architecture.png
│   ├── circuit_diagram.png
│   ├── sensor_placement.png
│   ├── hardware_prototype.jpg
│   ├── website_dashboard.png
│   ├── live_sensor_readings.png
│   ├── calibration_process.png
│   └── pushup_detection.png
│
└── LICENSE
```

## 15. Limitations

- The current angle calculations are based on accelerometer readings.
- The system does not currently implement full sensor fusion.
- Sensor placement and mounting can affect measurements.
- The system relies on individual calibration and reference recording.
- BLE performance and update rate depend on the connected device and browser.
- The ultrasonic sensor requires a suitable position and unobstructed measurement path.
- The current repetition classification is based on the recorded arm movement ranges.

## 16. Future Improvements

- Implement complementary or Kalman filtering for improved orientation estimation.
- Improve sensor mounting and wearable integration.
- Add more detailed movement analysis.
- Improve feedback and repetition classification.
- Add session history and performance tracking.
- Develop a mobile-friendly interface.
- Evaluate the system against independently measured reference movements.

## 17. Project Status

**Development Stage:** Prototype

The project combines embedded systems, sensor integration, wireless communication, and browser-based visualization to develop a practical push-up posture monitoring system.

---

**X FORCE – Push-up Posture Monitoring System**  
*Embedded Systems | Mechatronics | Wearable Technology | Human Movement Monitoring*
