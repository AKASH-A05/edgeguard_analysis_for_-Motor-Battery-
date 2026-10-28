# EdgeGuard — Cloud-Connected Edge AI Motor Monitoring System

## 1. Project Overview

EdgeGuard is an embedded IoT-based motor monitoring prototype designed to collect vibration and temperature data, process sensor readings at the edge, transmit the data to a cloud database, and visualize the results through a web dashboard.

The system uses an STM32 microcontroller for sensor acquisition and vibration signal processing, an ESP32 for Wi-Fi connectivity and cloud communication, and Firebase Realtime Database for storing live sensor readings.

The project aims to establish a foundation for condition monitoring and future predictive maintenance of electric motors.

## 2. Objectives

- Acquire motor vibration data using the MPU6050 sensor.
- Monitor temperature using DS18B20 temperature probes.
- Perform vibration signal processing on the STM32.
- Transmit sensor readings from the STM32 to the ESP32 using UART.
- Upload sensor data to Firebase Realtime Database over Wi-Fi.
- Visualize live sensor readings through a web dashboard.
- Establish a foundation for future edge-based fault detection and predictive maintenance.

## 3. System Architecture

```text
       MPU6050 Vibration Sensor
                  |
          I2C Communication
                  |
       DS18B20 Temperature Sensors
                  |
                  v
         STM32 Nucleo-F446RE
       Sensor Acquisition & FFT
                  |
             UART / JSON
                  |
                  v
           ESP32-WROOM-32
           Wi-Fi Connectivity
                  |
            Internet / HTTPS
                  |
                  v
       Firebase Realtime Database
                  |
          Firebase JavaScript SDK
                  |
                  v
          EdgeGuard Web Dashboard
```

## 4. Hardware Components

| Component | Purpose |
|---|---|
| STM32 Nucleo-F446RE | Sensor acquisition and signal processing |
| MPU6050 | Accelerometer and gyroscope measurements for vibration monitoring |
| DS18B20 temperature probes | Temperature measurements |
| ESP32-WROOM-32 | Wi-Fi communication and cloud data transmission |
| Computer | Firmware development and dashboard access |
| Motor or rotating test setup | Source of vibration and temperature data during testing |

## 5. Software and Technologies

- **STM32CubeIDE:** STM32 firmware development and debugging
- **STM32 HAL:** Peripheral configuration and hardware access
- **Arduino IDE:** ESP32 firmware development
- **C/C++:** Embedded firmware
- **I2C:** MPU6050 communication
- **OneWire:** DS18B20 communication
- **UART:** STM32-to-ESP32 serial communication
- **FFT:** Vibration signal frequency analysis
- **Wi-Fi:** ESP32 network connectivity
- **Firebase Realtime Database:** Cloud data storage
- **Firebase Authentication:** User authentication
- **HTML, CSS and JavaScript:** Web dashboard

## 6. Working Principle

### 6.1 Sensor Acquisition

The MPU6050 provides accelerometer and gyroscope measurements. The STM32 acquires vibration samples and reads temperature values from two DS18B20 probes.

### 6.2 Edge Signal Processing

The STM32 firmware processes vibration samples using a Fast Fourier Transform (FFT) to obtain frequency-domain information.

The current implementation uses a 128-point FFT and an assumed sampling frequency of 500 Hz. The resulting frequency resolution is approximately 3.90625 Hz per FFT bin.

Sampling timing and frequency accuracy must be validated before using these values for motor fault diagnosis.

### 6.3 UART Communication

The STM32 sends sensor information to the ESP32 through UART as newline-terminated JSON packets.

Example packet:

```json
{
  "frequency_hz": 3.91,
  "fft_magnitude": 4395.82,
  "temp1_c": 28.56,
  "temp2_c": 28.62
}
```

The values above are illustrative examples, not guaranteed or calibrated measurements.

### 6.4 Cloud Connectivity

The ESP32 receives and parses the UART packets, connects to a Wi-Fi network, and uploads the sensor values to Firebase Realtime Database.

### 6.5 Web Dashboard

The web dashboard authenticates with Firebase and listens for updates to the live sensor data. It displays sensor readings and provides visual monitoring of the available parameters.

## 7. Database Structure

The current live data is stored at:

```text
/edgeguard/live
```

Expected data fields:

| Field | Description |
|---|---|
| `frequency_hz` | Frequency value reported by the STM32 FFT processing |
| `fft_magnitude` | FFT magnitude value reported by the STM32 |
| `temp1_c` | Temperature from the first DS18B20 probe |
| `temp2_c` | Temperature from the second DS18B20 probe |

The actual interpretation of frequency and magnitude depends on the implemented signal-processing algorithm and its validation.

## 8. Repository Structure

```text
EdgeGuard/
├── README.md
├── STM32_Firmware/
│   └── STM32CubeIDE project files
├── ESP32_Firmware/
│   └── ESP32 Arduino sketch
├── Dashboard/
│   └── edgeguard_dashboard.html
└── docs/
    ├── system_architecture.png
    ├── hardware_connections.png
    └── test_results.md
```

Adjust these folder names to match the actual files in your repository.

## 9. Setup and Execution

### STM32 Firmware

1. Open the STM32 project in STM32CubeIDE.
2. Verify the MCU configuration and peripheral settings.
3. Connect the MPU6050 and DS18B20 sensors according to the wiring documentation.
4. Build the firmware and resolve any compilation errors.
5. Flash the firmware to the STM32 Nucleo board.
6. Verify sensor readings and UART output.

### ESP32 Firmware

1. Open the ESP32 sketch in Arduino IDE.
2. Install the required ESP32 board support and Firebase library.
3. Configure Wi-Fi and Firebase authentication credentials locally.
4. Verify the UART connections and baud rate.
5. Upload the firmware to the ESP32.
6. Open Serial Monitor and confirm UART reception and Firebase upload success.

### Firebase

1. Configure Firebase Realtime Database.
2. Enable the required authentication method.
3. Apply database security rules that restrict access to authorized users.
4. Confirm that the ESP32 can write data to `/edgeguard/live`.

### Web Dashboard

1. Open the dashboard HTML file in a compatible browser.
2. Ensure internet access is available.
3. Sign in using the configured Firebase Authentication account.
4. Verify that live sensor readings appear and update.

## 10. Hardware Interface Connections

### MPU6050 to STM32

| MPU6050 | STM32 Nucleo-F446RE |
|---|---|
| SCL | PB8 |
| SDA | PB9 |
| VCC | Appropriate supported supply |
| GND | GND |

### DS18B20 to STM32

| Signal | STM32 connection |
|---|---|
| OneWire data | PA1 |
| Pull-up resistor | 4.7 kΩ between data and 3.3 V |
| GND | GND |
| VDD | Compatible supply for the probe |

### STM32 to ESP32 UART

| STM32 | ESP32-WROOM-32 |
|---|---|
| PA9 (USART1 TX) | GPIO16 (UART RX) |
| PA10 (USART1 RX) | GPIO17 (UART TX) |
| GND | GND |

UART signals must use compatible 3.3 V logic levels. The boards may be powered independently; do not connect their power outputs together without checking the board power design.

## 11. Current Implementation Status

- [x] STM32 firmware development environment configured
- [x] ESP32 Firebase library configured
- [x] UART sensor-data reception on ESP32
- [x] Firebase Realtime Database upload
- [x] Web dashboard created
- [x] Live sensor data displayed on the dashboard
- [ ] Validate the actual sampling interval and FFT frequency accuracy
- [ ] Validate sensor measurements against reference instruments
- [ ] Add actual battery voltage and current sensing hardware
- [ ] Implement and validate motor fault-detection logic
- [ ] Validate predictive-maintenance performance using test data

## 12. Limitations

- Frequency-domain measurements require validation against the actual sample timing.
- FFT magnitude is dependent on signal scaling, windowing, and implementation details.
- Temperature readings depend on probe placement and sensor accuracy.
- Battery voltage and current are not currently measured by dedicated sensing hardware.
- A displayed nominal battery voltage or illustrative current must not be interpreted as a live measurement.
- Predictive maintenance is a future objective until fault-detection methods are implemented and validated.

## 13. Future Enhancements

- Add suitable voltage and current sensors.
- Improve vibration sampling and signal conditioning.
- Extract vibration features for different motor operating conditions.
- Develop and validate edge-based motor fault detection.
- Store historical measurements for longer
