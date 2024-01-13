# ESP32 Novy Controller

The main goal of this project is to control the lights of a Novy cooking hood.
The ESP32C3 connects to WiFi and MQTT. It will serve a webpage where you can control the Novy hood via a 433mhz signal.

Webpage example:

![Webpage](https://github.com/renedis/ESP32_Novy_Controller/blob/main/novy-webpage.jpg?raw=true)

Hardware example:

![ESP32 with a 433mhz transmitter soldered onto it](https://github.com/renedis/ESP32_Novy_Controller/blob/main/novy-controller.jpeg?raw=true)

I soldered a FS1000A 433mhz transmitter directly to the ESP32 on PIN 3, PIN 4 and Ground.
By setting pin 4 to HIGH, it powers the transmitter while pin 3 is used to send data.

## Usage

To use this in your own setup, make a copy of the [***config.example.h***](https://github.com/renedis/ESP32_Novy_Controller/blob/main/config.example.h) file. Rename it to ***config.h***.
Change the contents of the file to your own needs.

To control it manually via the webpage you simply click the button on the webpage.

To control it via a home automation like HomeAssistant you can simply call buttons via weburl (e.g. http://192.168.1.5/toggleLight) and it will toggle the light. Where 192.168.1.5 is the IP address of the ESP32 novy controller.
To make it a clickable button in HomeAssistant you can add a shell_command button e.g.:


```
shell_command:
  novylight: 'curl -k "http://192.168.1.5/toggleLight"'
```

Controlling via MQTT is a work in progress. (it already subscribes but won't sent a the 433mhz signal if a message is received via MQTT.

## Case

Working on a case!
