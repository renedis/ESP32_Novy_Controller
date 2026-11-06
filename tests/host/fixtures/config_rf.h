// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
// Public copy, written by tools/make_public_fixtures.py from a real extraction: device names,
// fan codes, KNOWN_DEVICE_ID and the Novy channel replaced by the example values.


const String HOSTNAME = "NOVY-RF";
const String MQTT_PREFIX = "ESP";

// NOVY Hardware Configuration
const int TRANSMIT_433MHZ_PIN = 4;  // GPIO pin for STX882 DATA

// NOVY Device Codes
static const String NOVY_DEVICE_CODE[] = {
    "0101", "1001", "0001", "1110", "0110",
    "1010", "0010", "1100", "0100", "1000",
};

static const String NOVY_PREFIX = "0101";
static const String NOVY_COMMAND_LIGHT = "0111010001";
static const String NOVY_COMMAND_POWER = "0111010011";
static const String NOVY_COMMAND_PLUS = "0101";
static const String NOVY_COMMAND_MINUS = "0110";
static const String NOVY_COMMAND_NOVY = "0100";

// CEILING FAN Configuration
const unsigned long KNOWN_DEVICE_ID = 0x12345;

// Ceiling Fan Button Cycle Codes
const unsigned long ALL_OFF_CODES[] = {1,   2,   3,   4,   5,   6,   7,   8};
const int ALL_OFF_CYCLE_SIZE = 8;

const unsigned long FAN_OFF_CODES[] = {9,  10,  11,  12,  13,  14,  15,  16};
const int FAN_OFF_CYCLE_SIZE = 8;

const unsigned long LIGHT_TOGGLE_CODES[] = {17,  18,  19,  20,  21,  22,  23,  24};
const int LIGHT_TOGGLE_CYCLE_SIZE = 8;

const unsigned long FAN_FORWARD_CODES[] = {25,  26,  27,  28,  29,  30,  31,  32};
const int FAN_FORWARD_CYCLE_SIZE = 8;

const unsigned long FAN_REVERSE_CODES[] = {33,  34,  35,  36,  37,  38,  39,  40};
const int FAN_REVERSE_CYCLE_SIZE = 8;

const unsigned long FAN_WAVE_CODES[] = {41,  42,  43,  44,  45,  46,  47,  48};
const int FAN_WAVE_CYCLE_SIZE = 8;

const unsigned long SPEED_1_CODES[] = {49,  50,  51,  52,  53,  54,  55,  56};
const int SPEED_1_CYCLE_SIZE = 8;

const unsigned long SPEED_2_CODES[] = {57,  58,  59,  60,  61,  62,  63,  64};
const int SPEED_2_CYCLE_SIZE = 8;

const unsigned long SPEED_3_CODES[] = {65,  66,  67,  68,  69,  70,  71,  72};
const int SPEED_3_CYCLE_SIZE = 8;

const unsigned long SPEED_4_CODES[] = {73,  74,  75,  76,  77,  78,  79,  80};
const int SPEED_4_CYCLE_SIZE = 8;

const unsigned long SPEED_5_CODES[] = {81,  82,  83,  84,  85,  86,  87,  88};
const int SPEED_5_CYCLE_SIZE = 8;

const unsigned long SPEED_6_CODES[] = {89,  90,  91,  92,  93,  94,  95,  96};
const int SPEED_6_CYCLE_SIZE = 8;

const unsigned long TIMER_1H_CODES[] = {97,  98,  99, 100, 101, 102, 103, 104};
const int TIMER_1H_CYCLE_SIZE = 8;

const unsigned long TIMER_2H_CODES[] = {105, 106, 107, 108, 109, 110, 111, 112};
const int TIMER_2H_CYCLE_SIZE = 8;

const unsigned long TIMER_4H_CODES[] = {113, 114, 115, 116, 117, 118, 119, 120};
const int TIMER_4H_CYCLE_SIZE = 8;

// The sketch's default channelIndex is replaced by the example channel.
#define REF_NOVY_CHANNEL_INDEX 3
