// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
// Public copy, written by tools/make_public_fixtures.py: the verbatim sketch sections, only the
// default channelIndex comes from config_rf.h (the example Novy channel).


// --- button commands
enum ButtonCommand {
  CMD_LIGHT = 0,
  CMD_POWER = 1,
  CMD_PLUS = 2,
  CMD_MINUS = 3,
  CMD_NOVY = 4,
  CMD_BRIGHTDIM = 5,
  CMD_BRIGHTDIMLOW = 6,
  // Ceiling Fan Commands
  CMD_FAN_ALL_OFF = 7,
  CMD_FAN_OFF = 8,
  CMD_FAN_LIGHT = 9,
  CMD_FAN_FORWARD = 10,
  CMD_FAN_REVERSE = 11,
  CMD_FAN_WAVE = 12,
  CMD_FAN_SPEED1 = 13,
  CMD_FAN_SPEED2 = 14,
  CMD_FAN_SPEED3 = 15,
  CMD_FAN_SPEED4 = 16,
  CMD_FAN_SPEED5 = 17,
  CMD_FAN_SPEED6 = 18,
  CMD_FAN_TIMER1H = 19,
  CMD_FAN_TIMER2H = 20,
  CMD_FAN_TIMER4H = 21
};

// --- fan cycle index
#define FAN_CMD_COUNT  15
#define FAN_CMD_OFFSET CMD_FAN_ALL_OFF
int fanCycleIndex[FAN_CMD_COUNT] = {0};

// --- transmitter
RCSwitch transmitter = RCSwitch();

// --- device state
static bool   novyLightOn    = false;
static int    fanSpeed        = 0;   // 0=off, 1-6
static bool   fanLightOn      = false;
static String fanDirection    = "forward"; // "forward" or "reverse"

// --- button table
struct ButtonConfig {
  String name;
  String icon;
  ButtonCommand cmd;
};
ButtonConfig buttons[] = {
  {"Light", "mdi:lightbulb", CMD_LIGHT},
  {"Power", "mdi:power", CMD_POWER},
  {"Plus", "mdi:plus", CMD_PLUS},
  {"Minus", "mdi:minus", CMD_MINUS},
  {"Novy", "mdi:fan", CMD_NOVY},
  {"Brightness", "mdi:brightness-6", CMD_BRIGHTDIM},
  {"Brightness-low", "mdi:brightness-5", CMD_BRIGHTDIMLOW},
  // Ceiling Fan Buttons
  {"Fan-All-Off", "mdi:power-off", CMD_FAN_ALL_OFF},
  {"Fan-Off", "mdi:fan-off", CMD_FAN_OFF},
  {"Fan-Light", "mdi:lightbulb", CMD_FAN_LIGHT},
  {"Fan-Forward", "mdi:rotate-right", CMD_FAN_FORWARD},
  {"Fan-Reverse", "mdi:rotate-left", CMD_FAN_REVERSE},
  {"Fan-Wave", "mdi:wave", CMD_FAN_WAVE},
  {"Fan-Speed1", "mdi:numeric-1-circle", CMD_FAN_SPEED1},
  {"Fan-Speed2", "mdi:numeric-2-circle", CMD_FAN_SPEED2},
  {"Fan-Speed3", "mdi:numeric-3-circle", CMD_FAN_SPEED3},
  {"Fan-Speed4", "mdi:numeric-4-circle", CMD_FAN_SPEED4},
  {"Fan-Speed5", "mdi:numeric-5-circle", CMD_FAN_SPEED5},
  {"Fan-Speed6", "mdi:numeric-6-circle", CMD_FAN_SPEED6},
  {"Fan-Timer1H", "mdi:timer-1", CMD_FAN_TIMER1H},
  {"Fan-Timer2H", "mdi:timer-2", CMD_FAN_TIMER2H},
  {"Fan-Timer4H", "mdi:timer-4", CMD_FAN_TIMER4H}
};
const int BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);

// --- declarations
void handleButtonPress(ButtonCommand cmd);
void sendRFCommand(ButtonCommand cmd, int channelIndex = REF_NOVY_CHANNEL_INDEX);
void sendFanRFCommand(ButtonCommand cmd);
void publishMQTTStatus(String button, String state = "ON");
void mqttCallback(char* topic, byte* payload, unsigned int length);
String getButtonNameByCommand(ButtonCommand cmd);
String toLowerCase(String str);

// --- String toLowerCase
String toLowerCase(String str) {
  str.toLowerCase();
  return str;
}

// --- void handleButtonPress
void handleButtonPress(ButtonCommand cmd) {
  // Check if it's a ceiling fan command
  if (cmd >= CMD_FAN_ALL_OFF && cmd <= CMD_FAN_TIMER4H) {
    sendFanRFCommand(cmd);
  } else {
    sendRFCommand(cmd);
  }

  // Update device state
  switch (cmd) {
    case CMD_LIGHT:       novyLightOn = !novyLightOn; break;
    case CMD_FAN_LIGHT:   fanLightOn  = !fanLightOn;  break;
    case CMD_FAN_ALL_OFF: fanSpeed = 0; fanLightOn = false; break;
    case CMD_FAN_OFF:     fanSpeed = 0; break;
    case CMD_FAN_SPEED1:  fanSpeed = 1; break;
    case CMD_FAN_SPEED2:  fanSpeed = 2; break;
    case CMD_FAN_SPEED3:  fanSpeed = 3; break;
    case CMD_FAN_SPEED4:  fanSpeed = 4; break;
    case CMD_FAN_SPEED5:  fanSpeed = 5; break;
    case CMD_FAN_SPEED6:  fanSpeed = 6; break;
    case CMD_FAN_FORWARD: fanDirection = "forward"; break;
    case CMD_FAN_REVERSE: fanDirection = "reverse"; break;
    default: break;
  }

  // Publish device state to MQTT
  if (client.connected()) {
    String base = MQTT_PREFIX + "/" + HOSTNAME + "/state/";
    client.publish((base + "novy_light").c_str(),   novyLightOn ? "ON" : "OFF", true);
    client.publish((base + "fan_speed").c_str(),    String(fanSpeed).c_str(),   true);
    client.publish((base + "fan_light").c_str(),    fanLightOn  ? "ON" : "OFF", true);
    client.publish((base + "fan_direction").c_str(), fanDirection.c_str(),      true);
  }

  // Trigger flash red on any button press
  flashRed();

  // Publish MQTT status for Home Assistant
  String buttonName = getButtonNameByCommand(cmd);
  publishMQTTStatus(toLowerCase(buttonName), "PRESS");
  publishMQTTStatus(toLowerCase(buttonName), "");
}

// --- String getButtonNameByCommand
String getButtonNameByCommand(ButtonCommand cmd) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (buttons[i].cmd == cmd) {
      return buttons[i].name;
    }
  }
  return "unknown";
}

// --- void sendRFCommand
void sendRFCommand(ButtonCommand cmd, int channelIndex) {
  // Reconfigure transmitter for NOVY (may have been changed by fan commands)
  transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
  transmitter.setPulseLength(350);
  transmitter.setProtocol(12);

  String command;
  int repeatCount = 1; // Default RF repeater

  switch(cmd) {
    case CMD_LIGHT:
      command = NOVY_COMMAND_LIGHT;
      repeatCount = 2; // Only send twice for LIGHT. 3 or more will cause brighten/dim
      break;
    case CMD_BRIGHTDIM:
      command = NOVY_COMMAND_LIGHT; // Light command is used for brightness
      repeatCount = 4; // Send four times for LIGHT brightness.
      break;
    case CMD_BRIGHTDIMLOW:
      command = NOVY_COMMAND_LIGHT; // Light command is used for brightness
      repeatCount = 10; // Send ten times for LIGHT brightness. Brightness: 1= 4 repeats. 2= 6 repeats. 3= 8 repeats. 4= 10 repeats.
      break;
    case CMD_POWER:
      command = NOVY_COMMAND_POWER;
      break;
    case CMD_PLUS:
      command = NOVY_COMMAND_PLUS;
      break;
    case CMD_MINUS:
      command = NOVY_COMMAND_MINUS;
      break;
    case CMD_NOVY:
      command = NOVY_COMMAND_NOVY;
      break;
  }

  String rfCode = NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + command;

  for(int i = 0; i < repeatCount; i++) {
    transmitter.send(rfCode.c_str());
    yield();
  }
  addToLogBuffer("RF sent: " + getButtonNameByCommand(cmd) + " (" + rfCode + "), repeat signal: " + String(repeatCount));
}

// --- void sendFanRFCommand
void sendFanRFCommand(ButtonCommand cmd) {
  // Reconfigure transmitter for ceiling fan
  transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
  transmitter.setProtocol(1);
  transmitter.setPulseLength(248);
  transmitter.setRepeatTransmit(3); // Reduced from 10 to 3

  const unsigned long* codes = nullptr;
  int cycleSize = 0;
  String commandName = "";

  // Select the appropriate code array based on command
  switch(cmd) {
    case CMD_FAN_ALL_OFF:
      codes = ALL_OFF_CODES;
      cycleSize = ALL_OFF_CYCLE_SIZE;
      commandName = "All Off";
      break;
    case CMD_FAN_OFF:
      codes = FAN_OFF_CODES;
      cycleSize = FAN_OFF_CYCLE_SIZE;
      commandName = "Fan Off";
      break;
    case CMD_FAN_LIGHT:
      codes = LIGHT_TOGGLE_CODES;
      cycleSize = LIGHT_TOGGLE_CYCLE_SIZE;
      commandName = "Light Toggle";
      break;
    case CMD_FAN_FORWARD:
      codes = FAN_FORWARD_CODES;
      cycleSize = FAN_FORWARD_CYCLE_SIZE;
      commandName = "Forward";
      break;
    case CMD_FAN_REVERSE:
      codes = FAN_REVERSE_CODES;
      cycleSize = FAN_REVERSE_CYCLE_SIZE;
      commandName = "Reverse";
      break;
    case CMD_FAN_WAVE:
      codes = FAN_WAVE_CODES;
      cycleSize = FAN_WAVE_CYCLE_SIZE;
      commandName = "Wave";
      break;
    case CMD_FAN_SPEED1:
      codes = SPEED_1_CODES;
      cycleSize = SPEED_1_CYCLE_SIZE;
      commandName = "Speed 1";
      break;
    case CMD_FAN_SPEED2:
      codes = SPEED_2_CODES;
      cycleSize = SPEED_2_CYCLE_SIZE;
      commandName = "Speed 2";
      break;
    case CMD_FAN_SPEED3:
      codes = SPEED_3_CODES;
      cycleSize = SPEED_3_CYCLE_SIZE;
      commandName = "Speed 3";
      break;
    case CMD_FAN_SPEED4:
      codes = SPEED_4_CODES;
      cycleSize = SPEED_4_CYCLE_SIZE;
      commandName = "Speed 4";
      break;
    case CMD_FAN_SPEED5:
      codes = SPEED_5_CODES;
      cycleSize = SPEED_5_CYCLE_SIZE;
      commandName = "Speed 5";
      break;
    case CMD_FAN_SPEED6:
      codes = SPEED_6_CODES;
      cycleSize = SPEED_6_CYCLE_SIZE;
      commandName = "Speed 6";
      break;
    case CMD_FAN_TIMER1H:
      codes = TIMER_1H_CODES;
      cycleSize = TIMER_1H_CYCLE_SIZE;
      commandName = "Timer 1H";
      break;
    case CMD_FAN_TIMER2H:
      codes = TIMER_2H_CODES;
      cycleSize = TIMER_2H_CYCLE_SIZE;
      commandName = "Timer 2H";
      break;
    case CMD_FAN_TIMER4H:
      codes = TIMER_4H_CODES;
      cycleSize = TIMER_4H_CYCLE_SIZE;
      commandName = "Timer 4H";
      break;
    default:
      addToLogBuffer("Unknown fan command");
      return;
  }

  if (codes != nullptr && cycleSize > 0) {
    // Get the current command code from the cycle
    int cmdIdx = (int)cmd - FAN_CMD_OFFSET;
    unsigned long buttonCounter = codes[fanCycleIndex[cmdIdx]];
    
    // Combine device ID with command code
    unsigned long fullCode = (KNOWN_DEVICE_ID << 12) | buttonCounter;
    
    // Send once - RCSwitch will handle the repeats
    transmitter.send(fullCode, 32);
    
    addToLogBuffer("Fan RF sent: " + commandName + 
                  " (DeviceID: 0x" + String(KNOWN_DEVICE_ID, HEX) + 
                  ", Counter: " + String(buttonCounter) + 
                  ", FullCode: " + String(fullCode) + 
                  ", Cycle: " + String(fanCycleIndex[cmdIdx]) + "/" + String(cycleSize) + ")");
    
    // Advance this button's cycle index independently
    fanCycleIndex[cmdIdx] = (fanCycleIndex[cmdIdx] + 1) % cycleSize;
  }
  
}

// --- void publishMQTTStatus
void publishMQTTStatus(String button, String state) {
  if (client.connected()) {
    String topic = MQTT_PREFIX + "/" + HOSTNAME + "/button/" + button + "/state";
    client.publish(topic.c_str(), state.c_str());
  }
}

// --- void mqttCallback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Properly null-terminate the payload string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  String logMsg = "HA (MQTT) message received [" + String(topic) + "] " + String(message);
  addToLogBuffer(logMsg);

  // Parse topic to determine command
  String topicStr = String(topic);
  String baseTopic = MQTT_PREFIX + "/" + HOSTNAME + "/button/";

  if (topicStr.startsWith(baseTopic) && topicStr.endsWith("/set")) {
    // Extract button name from topic
    String buttonName = topicStr.substring(baseTopic.length());
    buttonName = buttonName.substring(0, buttonName.length() - 4); // Remove "/set"

    // Find matching button and execute command
    for (int i = 0; i < BUTTON_COUNT; i++) {
      String buttonNameLower = toLowerCase(buttons[i].name);
      if (buttonNameLower == buttonName) {
        flashRed();
        if (buttons[i].cmd >= CMD_FAN_ALL_OFF && buttons[i].cmd <= CMD_FAN_TIMER4H) {
          sendFanRFCommand(buttons[i].cmd);
        } else {
          sendRFCommand(buttons[i].cmd);
        }
        publishMQTTStatus(buttonName, "PRESS");
        publishMQTTStatus(buttonName, "");
        break;
      }
    }
  }
}

// --- setup() transmitter lines, wrapped
void sketch_setup_rf() {
  transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
  transmitter.setPulseLength(350);
  transmitter.setProtocol(12);
}
