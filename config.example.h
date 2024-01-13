#ifndef CONFIG_H
#define CONFIG_H
const String HOSTNAME = "<HOSTNAME>";
const String OTAPASSWORD = "<OTAPASSWORD>";
const char* SSID = "<YOURWIFINAME>";
const char* PASSWORD =  "<YOURWIFIPASSWORD";
const char* MQTT_SERVER = "YOURMQTTSERVER";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "YOURMQTTUSERNAME";
const char* MQTT_PASSWORD = "YOURMQTTPASSWORD";

// Do not edit MQTT_CLIENT_ID. It matches the hostname.
const char* MQTT_CLIENT_ID = "HOSTNAME";

// This is the GPIO pin which transmits the 433.92 mhz data
const int TRANSMIT_433MHZ_PIN = 3;

// This is the GPIO pin which provides power to the 433.92 mhz transceiver when data is sent
const int POWER_433MHZ_PIN = 4;

// Do not edit. This is the Novy device code identification.
static const String NOVY_DEVICE_CODE[] = {
    "0101",
    "1001",
    "0001",
    "1110",
    "0110",
    "1010",
    "0010",
    "1100",
    "0100",
    "1000",
};

// Do not edit. This is the Novy prefix code identification.
static const String NOVY_PREFIX = "0101";

// Do not edit. This is the Novy command code identification.
static const String NOVY_COMMAND_LIGHT = "0111010001";
static const String NOVY_COMMAND_POWER = "0111010011";
static const String NOVY_COMMAND_PLUS =  "0101";
static const String NOVY_COMMAND_MINUS = "0110";
static const String NOVY_COMMAND_NOVY =  "0100";

#endif  // CONFIG_H
