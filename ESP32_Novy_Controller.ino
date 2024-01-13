#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <RCSwitch.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "config.h"

RCSwitch transmitter = RCSwitch();
AsyncWebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);

const int bufferSize = 10; // Adjust this value based on the number of lines you want to show in logging on webpage
String logBuffer[bufferSize];
int logIndex = 0;

void PressLight(int channelIndex);
void PressPower(int channelIndex);
void PressPlus(int channelIndex);
void PressMinus(int channelIndex);
void PressNovy(int channelIndex);

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void addToLogBuffer(String message) {
    logBuffer[logIndex] = message;
    logIndex = (logIndex + 1) % bufferSize;
}

String getLogContent() {
    String content = "";
    for (int i = 0; i < bufferSize; i++) {
        int index = (logIndex + i) % bufferSize;
        content += logBuffer[index] + "<br>";
    }
    return content;
}

void connectToMQTT() {
    while (!client.connected()) {
        addToLogBuffer("Connecting to MQTT...");
        if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            addToLogBuffer("Connected to MQTT");
            client.subscribe((String(HOSTNAME) + "/button/light").c_str());
            client.subscribe((String(HOSTNAME) + "/button/power").c_str());
            client.subscribe((String(HOSTNAME) + "/button/plus").c_str());
            client.subscribe((String(HOSTNAME) + "/button/minus").c_str());
            client.subscribe((String(HOSTNAME) + "/button/novy").c_str());
        } else {
            addToLogBuffer("Failed, rc=" + String(client.state()) + " Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(POWER_433MHZ_PIN, OUTPUT);
    digitalWrite(POWER_433MHZ_PIN, HIGH);
    transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
    transmitter.setPulseLength(350);
    transmitter.setProtocol(12);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
        return;
    }
    addToLogBuffer("IP Address: " + WiFi.localIP().toString());
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);

    // Hostname dfor OTA image update
    ArduinoOTA.setHostname(HOSTNAME.c_str());
    // OTA password for updates image via WiFi
    ArduinoOTA.setPassword(OTAPASSWORD.c_str());
    // ArduinoOTA.setPassword("novy");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head>";
    html += "<style>";
    html += "body {";
    html += "    background-color: #333333; /* Dark mode background color */";
    html += "    color: #ffffff; /* Dark mode text color */";
    html += "    font-family: Arial, sans-serif;";
    html += "    margin: 0;";
    html += "}";
    html += "header {";
    html += "    background-color: #343a40;";
    html += "    padding: 15px;";
    html += "    text-align: center;";
    html += "    color: #fff;";
    html += "}";
    html += ".container {";
    html += "    max-width: 600px;";
    html += "    margin: auto;";
    html += "    padding: 20px;";
    html += "}";
    html += ".button {";
    html += "    display: block;";
    html += "    width: 100%;";
    html += "    padding: 10px;";
    html += "    margin-bottom: 10px;";
    html += "    background-color: #007aff;";
    html += "    color: #fff;";
    html += "    border: 1px solid #007aff;";
    html += "    border-radius: 5px;";
    html += "    text-align: center;";
    html += "    text-decoration: none;";
    html += "    font-size: 16px;";
    html += "}";
    html += ".button:hover {";
    html += "    background-color: #0056b3;";
    html += "    border: 1px solid #0056b3;";
    html += "}";
    html += ".log-container {";
    html += "    background-color: #000000; /* Black background color */";
    html += "    color: #ffffff; /* White text color */";
    html += "    padding: 10px;";
    html += "    margin-top: 20px;";
    html += "    width: 100%;";
    html += "    border: 1px solid #007aff;";  // Border color same as the button color
    html += "    border-radius: 5px;";
    html += "}";
    html += "</style>";
    html += "<script>";
    html += "function toggleLight() {";
    html += "    fetch('/toggleLight');";
    html += "}";
    html += "function togglePower() {";
    html += "    fetch('/togglePower');";
    html += "}";
    html += "function togglePlus() {";
    html += "    fetch('/togglePlus');";
    html += "}";
    html += "function toggleMinus() {";
    html += "    fetch('/toggleMinus');";
    html += "}";
    html += "function toggleNovy() {";
    html += "    fetch('/toggleNovy');";
    html += "}";
    html += "function updateLog() {";
    html += "    fetch('/getLog').then(response => response.text()).then(data => {";
    html += "        document.getElementById('log-container').innerHTML = data;";
    html += "    });";
    html += "}";
    html += "setInterval(updateLog, 1000);";
    html += "</script>";
    html += "</head><body>";
    html += "<header>";
    html += "    <h1>" + HOSTNAME + "</h1>";
    html += "</header>";
    html += "<div class='container'>";
    html += "    <a class='button' href='#' onclick='toggleLight()'>Toggle Light</a>";
    html += "    <a class='button' href='#' onclick='togglePower()'>Toggle Power</a>";
    html += "    <a class='button' href='#' onclick='togglePlus()'>Toggle Plus</a>";
    html += "    <a class='button' href='#' onclick='toggleMinus()'>Toggle Minus</a>";
    html += "    <a class='button' href='#' onclick='toggleNovy()'>Toggle Novy</a>";
    html += "    <div id='log-container' class='log-container'>" + getLogContent() + "</div>";
    html += "</div>";
    html += "</body></html>";

    request->send(200, "text/html", html);
});

    server.on("/getLog", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Send the current log content as plain text
        request->send(200, "text/plain", getLogContent());
    });

    server.on("/toggleLight", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Light toggled");
        int channelIndex = 0;
        PressLight(channelIndex);
        client.publish((String(HOSTNAME) + "/button/light").c_str(), "ON");
    });

    server.on("/togglePower", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Power toggled");
        int channelIndex = 0;
        PressPower(channelIndex);
        client.publish((String(HOSTNAME) + "/button/power").c_str(), "ON");
    });

    server.on("/togglePlus", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Plus toggled");
        int channelIndex = 0;
        PressPlus(channelIndex);
        client.publish((String(HOSTNAME) + "/button/plus").c_str(), "ON");
    });

    server.on("/toggleMinus", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Minus toggled");
        int channelIndex = 0;
        PressMinus(channelIndex);
        client.publish((String(HOSTNAME) + "/button/minus").c_str(), "ON");
    });

    server.on("/toggleNovy", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Novy toggled");
        int channelIndex = 0;
        PressNovy(channelIndex);
        client.publish((String(HOSTNAME) + "/button/novy").c_str(), "ON");
    });

    server.onNotFound(notFound);

    server.begin();
}

void PressLight(int channelIndex) {
    transmitter.send((NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + NOVY_COMMAND_LIGHT).c_str());
}

void PressPower(int channelIndex) {
    transmitter.send((NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + NOVY_COMMAND_POWER).c_str());
}

void PressPlus(int channelIndex) {
    transmitter.send((NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + NOVY_COMMAND_PLUS).c_str());
}

void PressMinus(int channelIndex) {
    transmitter.send((NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + NOVY_COMMAND_MINUS).c_str());
}

void PressNovy(int channelIndex) {
    transmitter.send((NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + NOVY_COMMAND_NOVY).c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "MQTT Message arrived [" + String(topic) + "] " + String((char*)payload);
    addToLogBuffer(message);

    if (strcmp(topic, (String(HOSTNAME) + "/button/light").c_str()) == 0) {
        transmitter.send((NOVY_DEVICE_CODE[0] + NOVY_PREFIX + NOVY_COMMAND_LIGHT).c_str());
    }

    if (strcmp(topic, (String(HOSTNAME) + "/button/power").c_str()) == 0) {
        transmitter.send((NOVY_DEVICE_CODE[0] + NOVY_PREFIX + NOVY_COMMAND_POWER).c_str());
    }

    if (strcmp(topic, (String(HOSTNAME) + "/button/plus").c_str()) == 0) {
        transmitter.send((NOVY_DEVICE_CODE[0] + NOVY_PREFIX + NOVY_COMMAND_PLUS).c_str());
    }

    if (strcmp(topic, (String(HOSTNAME) + "/button/minus").c_str()) == 0) {
        transmitter.send((NOVY_DEVICE_CODE[0] + NOVY_PREFIX + NOVY_COMMAND_MINUS).c_str());
    }

    if (strcmp(topic, (String(HOSTNAME) + "/button/novy").c_str()) == 0) {
        transmitter.send((NOVY_DEVICE_CODE[0] + NOVY_PREFIX + NOVY_COMMAND_NOVY).c_str());
    }
}

void loop() {
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();
    ArduinoOTA.handle();
}
