# ESP32 Novy Controller

The main goal of this project is to control the lights of a Novy cooking hood.
The ESP32-C3-mini connects to WiFi, MQTT and HomeAssistant (via MQTT). It will serve a webpage where you can control the Novy hood via a 433mhz signal.

Webpage example:

![Webpage](https://github.com/renedis/ESP32_Novy_Controller/blob/main/novy-webpage.jpg?raw=true)

Hardware example:

![ESP32 with a 433mhz transmitter soldered onto it](https://github.com/renedis/ESP32_Novy_Controller/blob/main/novy-controller.jpeg?raw=true)

I soldered a FS1000A 433mhz transmitter directly to the ESP32 on PIN 3, PIN 4 and Ground. You can also solder VCC of the FS1000A directly to 3.3V of the ESP for more current.
By setting pin 4 to HIGH, it powers the transmitter while pin 3 is used to send data.

## Usage

To use this in your own setup, make a copy of the [***config.example.h***](https://github.com/renedis/ESP32_Novy_Controller/blob/main/config.example.h) file. Rename it to ***config.h***.
Change the contents of the file to your own needs.

### Webpage control
To control it manually via the webpage you simply click the button on the webpage.

### HomeAssistant auto discovery
Auto discovery is added. It's done via MQTT and should add all buttons. It's done as a button and not a toggle switch because 433MHz is a one-way protocol.

## Case
 3D printable .stl file is here! It's already made but fitment could be better.



## LEGACY options
### Manual HA HTTP shell_command request control
To control it via a home automation like HomeAssistant you can simply call buttons via weburl (e.g. http://192.168.1.5/toggleLight) and it will toggle the light. Where 192.168.1.5 is the IP address of the ESP32 novy controller.
To make it a clickable button in HomeAssistant you can add a shell_command button e.g.:
```
shell_command:
  novylight: 'curl -k "http://192.168.1.5/toggleLight"'
```
### MQTT control
Controlling via MQTT can be done by sending a RAW payload to the topic (using minus as an example):
```
NOVY/button/minus/ON
```

Adding it in HomeAssistant .yaml can be done like this (using light as an example):
```
mqtt:
  - button:
      unique_id: novy_light
      name: "NOVY-light"
      command_topic: "NOVY/button/light"
      payload_press: "ON"
```
