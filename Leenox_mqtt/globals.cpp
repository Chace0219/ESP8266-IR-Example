#include "globals.h"

uint16_t rawIrData[SLOT_SIZE+1];
uint16_t rawSequence[SEQ_SIZE];

uint16_t rawIR1[SLOT_SIZE+1];
uint16_t rawIR2[SLOT_SIZE+1];

uint16_t rawIR1size, rawIR2size;

char mqtt_server[40];
char mqtt_port[5];
char device_id[40];

char mqtt_user[32];
char mqtt_pass[32];
char mqtt_secure[1];
bool mqtt_secure_b;
int mqtt_port_i;

char pubIntervalTmp[10];
uint32_t pubInterval;

bool buttonState = 1 - BUTTON_ACTIVE_LEVEL; // State of control button
bool autoSendMode = false;
bool shouldSaveConfig = false; //flag for saving data
String clientName; // MQTT client name
bool rawMode = false; // Raw mode receiver status

unsigned long lastTSAutoStart;
unsigned long lastTSMQTTReconect;
unsigned long autoStartFreq = 300000; // Frequency of autostart
bool autoStartSecond = false;

// ------------------------------------------------
// Global objects
IRrecv irrecv(RECV_PIN);
IRsend irsend(TRANS_PIN);

WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;
EEpromDataStruct EEpromData;

/* **************************************************************
* Convert String to unsigned long
*/
unsigned long StrToUL(String inputString)
{
	unsigned long result = 0;
	for (int i = 0; i < inputString.length(); i++)
	{
		char c = inputString.charAt(i);
		if (c < '0' || c > '9') break;
		result *= 10;
		result += (c - '0');
	}
	return result;
}

/**********************************************
* Convert MAC to String
*/
String macToStr(const uint8_t* mac)
{
	String result;
	for (int i = 0; i < 6; ++i)
	{
		result += String(mac[i], 16);
		if (i < 5)
			result += ':';
	}
	return result;
}

/**********************************************
* callback for wifimanager to notifying us of the need to save config
*/
void saveConfigCallback()
{
	sendToDebug("*IR: Should save config\n");
	shouldSaveConfig = true;
}

void sendToDebug(String message)
{
	Serial.print("** Lennox : ");
	Serial.println(message);
}

// We can check RAW data, is that compatible with lennox, I have looked in your video, dumped commands 
// are larger than 200 bytes, I want to confirm them, 
uint16_t rawData01[RAWDATA01LEN] = {
4422,4404,560,1592,564,512,490,1664,488,590,488,588,562,512,490,588,488,1668,562,1588,488,590,558,518,486,590,564,512,488,588,564,1620,532,512,564,512,540,1656,444,1666,558,516,566,1588,542,568,510,1612,486,590,540,1612,562,1592,560,1616,540,1590,540,1614,558,1594,566,1590,564,1588,562,1592,560,1636,440,1668,564,1588,566,1588,564,1590,560,1594,564,1588,540,536,540,1614,560,1594,486,590,564,510,488,1666,566,1588,562,1592,488,5266,4422,4404,542,536,560,1594,564,546,532,1588,558,1594,488,1666,560,1594,562,516,556,518,558,1594,564,1590,560,1594,558,1594,564,1590,562,514,488,1664,566,1588,486,590,560,516,560,1594,490,588,558,1596,558,516,564,1622,530,512,486,632,444,592,538,536,564,512,538,538,540,536,560,518,538,538,560,516,560,516,558,520,558,518,488,588,488,588,486,590,564,1588,558,518,560,516,564,1622,508,1612,562,514,562,512,560,516,540
};

uint16_t rawData02[RAWDATA02LEN] = { 
4420,4406,562,1592,512,566,486,1666,494,584,512,564,488,590,486,590,486,1668,562,1590,510,568,562,512,510,566,486,590,512,564,514,1638,560,518,558,516,564,1590,510,1642,562,516,510,1642,564,544,480,1640,486,590,486,1668,558,1594,486,1668,488,1664,488,1666,564,1590,512,1644,556,1594,490,1666,512,564,512,564,488,588,488,588,488,1666,510,566,486,1666,488,588,488,588,510,1642,562,514,562,516,486,590,562,1590,486,590,486,5264,4424,4400,512,566,564,1588,512,566,564,1588,558,1596,512,1640,564,1590,564,512,526,552,510,1642,486,1666,488,1666,562,1592,562,1590,490,588,512,1640,488,1666,562,512,486,590,512,1640,486,592,488,1664,512,564,512,1642,490,588,562,546,454,588,510,566,490,588,562,514,558,518,556,520,558,516,562,1590,486,1668,486,1666,510,1642,490,588,510,1640,514,564,558,1592,490,1664,510,566,558,1594,560,1594,484,1710,444,592,484,1668,488
}; // done, we can test again
