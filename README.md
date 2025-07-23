# ESP32 Novy Controller

A WiFi-enabled controller for Novy cooking hoods using ESP32-C3-mini with 433MHz RF transmission (STX882 or FS1000A).

## Overview

This project enables remote control of Novy cooking hood lights through:
- **Web interface** – Direct browser control
- **MQTT integration** – For home automation systems
- **Home Assistant** – Auto-discovery support via MQTT

### Web Interface Preview

![Webpage](https://github.com/renedis/ESP32_Novy_Controller/blob/main/files/novy-webpage.jpg?raw=true)

### Hardware Setup

![ESP32 with a 433mhz transmitter soldered onto it](https://github.com/renedis/ESP32_Novy_Controller/blob/main/files/ESPHW1.jpeg?raw=true)

## Hardware Requirements

- ESP32-C3-mini development board (with external antenna if range is WiFi range is not enough)
- TX882 433MHz transmitter (with antenna for better range) ~~FS1000A 433MHz transmitter~~
- Basic soldering equipment

### Wiring

Connect the 433MHz transmitter to your ESP32:
- **Data pin** → GPIO 4
- **VCC pin** → 3.3V or GPIO 3  (directly connect **VCC** → 3.3V for more current)
- **GND pin** → GND

> **💡 Tip:** The STX882 transmitter is a much better drop-in replacement for the FS1000A – similar price but stronger signal output.

The power control setup (GPIO 3 → HIGH) powers the transmitter only when needed, while GPIO 4 handles data transmission.

## Setup Instructions

1. **Clone the repository**
   ```bash
   git clone https://github.com/renedis/ESP32_Novy_Controller.git
   ```

2. **Configure your settings**
   - Copy `config.example.h` to `config.h`
   - Update the configuration with your WiFi credentials and MQTT settings

3. **Upload to ESP32**
   - Use Arduino IDE or PlatformIO to compile and upload

## Usage

### Web Control

Access the web interface by navigating to your ESP32's IP address in a browser. Click the buttons to control your Novy hood.

### Home Assistant Integration

#### Automatic Discovery (Recommended)

The controller supports MQTT auto-discovery. Once connected, all buttons will automatically appear in Home Assistant as button entities.

> **Note:** Buttons are used instead of toggle switches because 433MHz is a one-way communication protocol.

#### Manual Configuration (Legacy)

<details>
<summary>Click to expand legacy configuration options</summary>

##### HTTP Shell Commands

```yaml
shell_command:
  novylight: 'curl -k "http://192.168.1.5/toggleLight"'
```

##### Manual MQTT Setup

```yaml
mqtt:
  - button:
      unique_id: novy_light
      name: "NOVY-light"
      command_topic: "NOVY/button/light"
      payload_press: "ON"
```

##### Direct MQTT Control

Send payload to topic:
```
NOVY/button/minus/ON
```

</details>

## 3D Printed Case

A printable case is available in the repository:

![ESP32 printed case](https://github.com/renedis/ESP32_Novy_Controller/blob/main/files/ESPHW2.jpeg?raw=true)

> **Note:** The case fits but could use some refinement for better fitment.

## API Endpoints

| Endpoint                          | Action               |
|------------------------------------|----------------------|
| `http://<ESP32_IP>/toggleLight`    | Toggle hood light    |
| `http://<ESP32_IP>/...`            | Other controls as configured |

## Contributing

Feel free to submit issues and pull requests to improve this project.
