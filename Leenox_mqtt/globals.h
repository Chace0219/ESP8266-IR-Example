
// Global definitions

#ifndef GLOBALS_H

#define GLOBALS_H

// Slots for RAW data recording
#define SLOTS_NUMBER 20 // Number of slots
#define SLOT_SIZE 300   // Size of single slot
#define SEQ_SIZE 10     // Raw sequnece size

// 
#define DEBUG X
#define VERSION "0.10"

// pinout setting
#define RESETPIN 12     // D6
#define DHT11PIN 0      // D3

#define RECV_PIN 13     // D7 - GPIO13
#define TRANS_PIN 4     // D2
#define TRIGGER_PIN 15  // D8
#define LED_PIN 2       // D4 - GPIO2
#define BUTTON_ACTIVE_LEVEL HIGH
#define MQTTRETRYINTERVAL 10000 // mqtt retry interval when it is disconnected from broker
#define TRANSMITTER_FREQ 38

#define SUFFIX_TEMP  "/pub/temp"
#define SUFFIX_HUMI  "/pub/humi"
#define SUFFIX_BUTT  "/pub/button"
#define SUFFIX_SUBS  "/sub/IR"

#define SUFFIX_WILL  "/status"
#define SUFFIX_NOTIFY  "/notify"
#define DEFAULT_MQTT_PORT 1883

// ----------------------------------------------------------------
// Global includes
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "FS.h"
#include <IRremoteESP8266.h>      // https://github.com/markszabo/IRremoteESP8266 (use local copy)
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient (id: 89)
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic (id: 567)
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson (id: 64)
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>
#include <IRutils.h>
// The following are only needed for extended decoding of A/C Messages
#include <ir_Coolix.h>
#include <ir_Daikin.h>
#include <ir_Fujitsu.h>
#include <ir_Gree.h>
#include <ir_Haier.h>
#include <ir_Kelvinator.h>
#include <ir_Mitsubishi.h>
#include <ir_Midea.h>
#include <ir_Panasonic.h>
#include <ir_Samsung.h>
#include <ir_Toshiba.h>

// Global variables
extern char mqtt_server[40];
extern char mqtt_port[5];
extern char mqtt_user[32];
extern char mqtt_pass[32];
extern char device_id[40];
extern char mqtt_secure[1];
extern bool mqtt_secure_b;
extern int mqtt_port_i;

extern char pubIntervalTmp[10];
extern uint32_t pubInterval;


extern bool shouldSaveConfig ; //flag for saving data
extern String clientName; // MQTT client name
extern bool rawMode; // Raw mode receiver status
extern unsigned long lastTSAutoStart; // Last timestamp of auto sender
extern unsigned long lastTSMQTTReconect; // Last timestamp of MQTT reconnect
extern unsigned long autoStartFreq; // Frequency of autostart
extern bool autoStartSecond;

// ------------------------------------------------
// STRUCTURES
struct EEpromDataStruct {
  bool autoSendMode;
};
// ------------------------------------------------
// Global objects

 extern IRrecv irrecv;
 extern IRsend irsend;
 extern WiFiClient wifiClient;
 extern WiFiClientSecure wifiClientSecure;
 extern PubSubClient mqttClient;
 extern EEpromDataStruct EEpromData;
// ------------------------------------------------
// Functions declaration
unsigned long StrToUL(String inputString);
String macToStr(const uint8_t* mac);

void saveConfigCallback ();
void MQTTcallback(char* topic, byte* payload, unsigned int length);
void connect_to_MQTT();
void sendToDebug(String message);

#define RAWDATA01LEN  199 // we need to change them as fit to new data
#define RAWDATA02LEN  199
extern uint16_t rawData01[RAWDATA01LEN];
extern uint16_t rawData02[RAWDATA02LEN]; //?

#define RAWDATALEN  199
extern const uint64_t IRData[];
extern const uint16_t IRcommands[][RAWDATALEN];
int8_t findIndexIRData(uint64_t command);
#endif


