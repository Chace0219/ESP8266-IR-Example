

#include "globals.h"
#include "FiniteStateMachine.h"
#include "FBD.h"
#include "dht_nonblocking.h"

// there are two FSM instanecs, 
// one is for mqtt connection
// so, if mqtt connection is disconnecetd by any reason, it will retry in order to connnect again within 30secs
// in all time, it is checking mqtt connection

// another fsm is for action of IR signal
// if any event button or mqtt command os triggered,
// program will start sedning IR signal task ( RAW01 , 5 sec standby, RAW02) without program blocking
// by using those two FSM we can extend logic easily

/* mqtt mode state machine */
void mqttModeEnter();
void mqttModeUpdate();
void mqttNoneModeEnter();
void mqttNoneModeUpdate();

State mqttMode(mqttModeEnter, mqttModeUpdate, NULL);
State mqttNoneMode(mqttNoneModeEnter, mqttNoneModeUpdate, NULL);
FiniteStateMachine mqttMachine(mqttNoneMode);

/*  */
State airConIdle(NULL);
State airConStep1(NULL);
State airConStep2(NULL);
FiniteStateMachine airConController(airConIdle);

// FBD blocks
TON buttonTON(200);
Rtrg buttonPressed;
Ftrg buttonReleased;

// nonblocking dht11 object
DHT_nonblocking dht_sensor(DHT11PIN, DHT_TYPE_11);

/* init setup routine */
void setup(void)
{
	// 
	Serial.begin(115200);

	// delay for reset button
	delay(5000);

	// setup button trigger pin mode 
	pinMode(TRIGGER_PIN, INPUT_PULLUP);

	// setup reset button pin
	pinMode(RESETPIN, INPUT_PULLUP);

#ifdef LED_PIN
	pinMode(LED_PIN, OUTPUT);
#endif

	/* part for setting configuration*/
	if (SPIFFS.begin())
	{
		sendToDebug(" checking file system on SPIFFS\n");
		if (SPIFFS.exists("/config.json"))
		{
			//file exists, reading and loading
			sendToDebug(" reading config file\n");
			File configFile = SPIFFS.open("/config.json", "r");
			if (configFile)
			{
				sendToDebug(" opened config file\n");
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.
				std::unique_ptr<char[]> buf(new char[size]);
				configFile.readBytes(buf.get(), size);
				DynamicJsonBuffer jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(buf.get());
				char tmpBuff[400];
				json.printTo(tmpBuff, sizeof(tmpBuff));
				sendToDebug(String(" config: " + String(tmpBuff) + "\n"));
				if (json.success())
				{
					sendToDebug("*IR: parsed json\n");
					if (json.containsKey("mqtt_server"))
						strcpy(mqtt_server, json["mqtt_server"]);
					if (json.containsKey("mqtt_port"))
						strcpy(mqtt_port, json["mqtt_port"]);
					if (json.containsKey("mqtt_secure"))
						strcpy(mqtt_secure, json["mqtt_secure"]);
					if (json.containsKey("mqtt_user"))
						strcpy(mqtt_user, json["mqtt_user"]);
					if (json.containsKey("mqtt_pass"))
						strcpy(mqtt_pass, json["mqtt_pass"]);
					if (json.containsKey("device_id"))
						strcpy(device_id, json["device_id"]);
					if (json.containsKey("interval"))
						strcpy(pubIntervalTmp, json["interval"]);
					
				}
				else
				{
					sendToDebug("*IR: failed to load json config\n");
				}
			}
		}
	}
	else
		sendToDebug("*IR: failed to mount FS\n");

	sendToDebug("*IR: Start setup\n");
	WiFiManagerParameter custom_mqtt_secure("secure", "is secure server 0-no / 1-yes", mqtt_secure, 2);
	WiFiManagerParameter custom_mqtt_server("server", "MQTT server address", mqtt_server, 40);
	WiFiManagerParameter custom_mqtt_port("port", "MQTT server port", mqtt_port, 5);
	WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 32);
	WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", mqtt_pass, 32);
	WiFiManagerParameter custom_device_id("deviceid", "controller id", device_id, 80);
	WiFiManagerParameter custom_interval("pubInterval", "Data Publsihinbg Interval", pubIntervalTmp, 10);

	//WiFiManager
#ifdef LED_PIN
	digitalWrite(LED_PIN, LOW);
#endif
	WiFiManager wifiManager;
	wifiManager.setTimeout(180);

	//set config save notify callback
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.addParameter(&custom_mqtt_secure);
	wifiManager.addParameter(&custom_mqtt_server);
	wifiManager.addParameter(&custom_mqtt_port);
	wifiManager.addParameter(&custom_mqtt_user);
	wifiManager.addParameter(&custom_mqtt_pass);
	wifiManager.addParameter(&custom_device_id);
	wifiManager.addParameter(&custom_interval);
	////////////////////////////////////////////////////////////

	// when boot, if button is pressed or jumper is pluuged, it will erase setting on SPIFFS
	// and enter AP mode for configuration, so if you want to change wifi setting, device id, mqtt info etc,

	// plugged jumper and reset esp8266, if so , you can find softap of ESP8266 on Phone etc
	if (digitalRead(RESETPIN) == LOW || (!SPIFFS.exists("/config.json")))
	{
		Serial.println("reseted setting");
		// Force enter configuration
		wifiManager.resetSettings();
	}

	/*  */
	char mySSID[20];
	char myPASS[12];
	sprintf(mySSID, "LENNOX-00%06X", ESP.getChipId());
	strcpy(myPASS, "lennox2018");
	if (!wifiManager.autoConnect(mySSID, myPASS))
	{
		sendToDebug("*IR: failed to connect and hit timeout\n");
		delay(3000);
		//reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(5000);
	}

#ifdef LED_PIN
	digitalWrite(LED_PIN, HIGH);
#endif

	///////////////////////////////////////////////////////////

	// read updated parameters from SPIFFS
	strcpy(mqtt_secure, custom_mqtt_secure.getValue());
	strcpy(mqtt_server, custom_mqtt_server.getValue());
	strcpy(mqtt_port, custom_mqtt_port.getValue());
	strcpy(mqtt_user, custom_mqtt_user.getValue());
	strcpy(mqtt_pass, custom_mqtt_pass.getValue());
	strcpy(device_id, custom_device_id.getValue());
	strcpy(pubIntervalTmp, custom_interval.getValue());

	// check interval number
	String tmpInt = String(pubIntervalTmp);
	if (tmpInt.toInt())
		pubInterval = tmpInt.toInt();
	else
		pubInterval = 30;

	// check mqtt port number
	String tmp = mqtt_port;
	if (tmp.toInt())
		mqtt_port_i = tmp.toInt();
	else
		mqtt_port_i = DEFAULT_MQTT_PORT;

	// check secure parameter
	if (mqtt_secure[0] == '1')
		mqtt_secure_b = true;
	else
		mqtt_secure_b = false;

	/////////////////////////////////////////

	// IR module init 
	irsend.begin();
	irrecv.enableIRIn();  // Start the receiver

						  // save the custom parameters to FS, when you complete setup on AP page
	if (shouldSaveConfig)
	{
		sendToDebug("*IR: saving config\n");
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["mqtt_server"] = mqtt_server;
		json["mqtt_user"] = mqtt_user;
		json["mqtt_pass"] = mqtt_pass;
		json["device_id"] = device_id;
		json["mqtt_port"] = mqtt_port;
		json["mqtt_secure"] = mqtt_secure;
		json["interval"] = pubIntervalTmp;

		File configFile = SPIFFS.open("/config.json", "w");
		if (configFile)
		{
			char tmpBuff[400];
			json.printTo(tmpBuff, sizeof(tmpBuff));
			sendToDebug(String("*IR: writing config: " + String(tmpBuff) + "\n"));
			json.printTo(configFile);
			configFile.close();
		}
		else
		{
			sendToDebug("*IR: failed to open config file for writing\n");
		}
	}

	// connect to wifi AP and mqtt broker using saved information
	sendToDebug(String("* Connected to ") + String(WiFi.SSID()) + "\n");
	sendToDebug(String("* IP address: ") + String(WiFi.localIP()) + "\n");

	clientName += "LEENOX-";
	uint8_t mac[6];
	WiFi.macAddress(mac);
	clientName += macToStr(mac);
	clientName += "-";
	clientName += String(micros() & 0xff, 16);
	connect_to_MQTT();
}


void mqttModeEnter()
{

}

void mqttModeUpdate()
{
	mqttClient.loop();
	if (!mqttClient.connected())
	{
		sendToDebug(String("disconnected from broker it will retry within 30 secs"));
		mqttMachine.transitionTo(mqttNoneMode);
	}
}

void mqttNoneModeEnter()
{

}

void mqttNoneModeUpdate()
{
	if (mqttMachine.timeInCurrentState() > MQTTRETRYINTERVAL)
	{
		connect_to_MQTT();
		if (mqttClient.connected())
			mqttMachine.transitionTo(mqttMode);
		else
			mqttMachine.transitionTo(mqttNoneMode);
	}
}


// publish temperature data
bool publishTemperatureData(double farenheight) // here is where you change the time to publish, it is relevatnt to payload
{
	StaticJsonBuffer<128> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	String tempData;

	tempData = String(farenheight, 2);
	root["dht11_temp"] = tempData.c_str();
	char buff[64];
	root.printTo(buff);

	if (mqttClient.connected())
	{
		String topicNotify = String(device_id) + SUFFIX_NOTIFY;
		mqttClient.publish(topicNotify.c_str(), tempData.c_str(), true);
		// mqttClient.publish(topicNotify.c_str(), buff, true);
		return true;
	}
	return false;
}

bool publishDht11Error()
{
	StaticJsonBuffer<128> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["notify"] = "dht11 sensor does not exist";
	char buff[64];
	root.printTo(buff);

	if (mqttClient.connected())
	{
		String topicNotify = String(device_id) + SUFFIX_NOTIFY;
		mqttClient.publish(topicNotify.c_str(), buff, true);
		return true;
	}

	return false;
}

/****************************************************************
* Main loop
*/
void loop(void)
{
	//
	static float temperature, tempFarenheight, humidity;
	static bool dht11_Error = false;

	static uint32_t dht11_TimeStamp = millis();
	// Measure once every four seconds.
	if (millis() - dht11_TimeStamp > 4000ul)
	{
		if (dht_sensor.measure(&temperature, &humidity) == true)
		{
			tempFarenheight = temperature * 1.8F + 32.0F;
			Serial.print(F("current farenheight temperature is "));
			Serial.println(tempFarenheight, 2);
			dht11_TimeStamp = millis();
		}
		else if (millis() - dht11_TimeStamp > 12000ul)
		{
			Serial.println(F("dht11 sensor does not exist"));
			dht11_TimeStamp = millis();
			if (!dht11_Error)
				publishDht11Error();
			dht11_Error = true;
		}
	}

	static uint32_t dht11SendingTime = millis();
	if (millis() - dht11SendingTime > pubInterval * 1000) // time is here, we will add setting for time, so you can change in setting page,
											 // give me 5 mins, I will implememnt setting for time, 
	{
		dht11SendingTime = millis();
		Serial.println(F("published temperature data"));
		publishTemperatureData(tempFarenheight);
	}

	// Finite machine for mqtt connection
	mqttMachine.update();

	// checking button event 
	buttonTON.IN = (digitalRead(TRIGGER_PIN) == BUTTON_ACTIVE_LEVEL);
	buttonTON.update();
	buttonPressed.IN = buttonTON.Q;
	buttonPressed.update();
	buttonReleased.IN = buttonTON.Q;
	buttonReleased.update();

	// if button is pressed 
	if (buttonPressed.Q)
	{
		// excuted only once when button is pressed
		Serial.println(F("button pressed."));

		StaticJsonBuffer<128> jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		root["notify"] = "button_pressed";
		char buff[64];
		root.printTo(buff);

		if (mqttClient.connected())
		{
			String topicNotify = String(device_id) + SUFFIX_NOTIFY;
			mqttClient.publish(topicNotify.c_str(), buff, true);
		}

		if (airConController.isInState(airConIdle))
		{
			Serial.println(F("entered into step1 from idle, send raw1 code"));

			// send raw01 via IR module
			irsend.sendRaw(rawData01, RAWDATA01LEN, TRANSMITTER_FREQ);

			// enter 
			airConController.transitionTo(airConStep1);
		}
	}

	// when button is released 
	if (buttonReleased.Q)
	{
		// excuted only once when button is released
		Serial.println(F("button released."));
	}

	// if you press button or valid mqtt command is received,
	// program will send RAW1 Data and after 5sec 
	// send RAW2 data
	// meanwhile, if any button or mqtt is triggered, 
	// command is not transmmitted
	// I have integrated FSM in order to avoid blocking login int this part.
	if (airConController.isInState(airConStep1))
	{
		if (airConController.timeInCurrentState() > 5000)
		{
			Serial.println(F("entered into step2 from step1, send raw2 code"));
			irsend.sendRaw(rawData02, RAWDATA02LEN, TRANSMITTER_FREQ);
			airConController.transitionTo(airConStep2);
		}
	}

	if (airConController.isInState(airConStep2))
	{
		if (airConController.timeInCurrentState() > 5000)
		{
			Serial.println(F("entered into idle from step2"));
			airConController.transitionTo(airConIdle);
		}
	}
	airConController.update();


}

/* **************************************************************
* Processing MQTT message callback routine
*/
void MQTTcallback(char* topic, byte* payload, unsigned int length)
{
	int i = 0;
	String msgString = "";
	String topicString = String(topic);

	// 
	for (unsigned int i = 0; i < length; i++)
		msgString += String((char)payload[i]);

	// 
	sendToDebug(String("new mqtt remote command") + "\n");
	sendToDebug(String("Topic: \"") + topicString + "\"\n");
	sendToDebug(String("Message: \"") + msgString + "\"\n");
	sendToDebug(String("Length: ") + String(length, DEC) + "\n");

	// parse command whether valid command
	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(msgString.c_str());

	if (!root.success()) {
		Serial.println("remote command from mqtt is invalid");
		return;
	}
	// so, when command has {"active":true}, program will enter sending signal cycle ( send raw01 and then after 5 secs send 
	// raw02
	bool active = root["active"];
	if (active == true)
	{
		if (airConController.isInState(airConIdle))
		{
			Serial.println(F("entered into step1 from idle, send raw1 code"));
			irsend.sendRaw(rawData01, RAWDATA01LEN, TRANSMITTER_FREQ);
			airConController.transitionTo(airConStep1);
		}
		else
		{
			Serial.println(F("still air con is in cative state, you can try later again"));
		}
	}

}


/************************************************
*  connect to MQTT broker
*/
void connect_to_MQTT()
{
	char myTopic[100];
	/*
	if (mqtt_secure_b)
	{
	sendToDebug("connecting to TLS server:");
	mqttClient.setClient(wifiClientSecure);
	}
	else*/
	{
		sendToDebug("connecting to nonTLS server:");
		mqttClient.setClient(wifiClient);
	}

	// 
	sendToDebug(String("connecting to ") + mqtt_server + ":" + mqtt_port_i + " as " + clientName + "\n");

	//
	mqttClient.setServer(mqtt_server, mqtt_port_i);
	mqttClient.setCallback(MQTTcallback);

	//
	sendToDebug(String("mqtt user ") + mqtt_user + "\n");
	sendToDebug(String("mqtt pass ") + mqtt_pass + "\n");

	// 
	String topicWill = String(device_id) + SUFFIX_WILL;
	// if (mqttClient.connect((char*)clientName.c_str(), (char*)mqtt_user, (char *)mqtt_pass, topicWill.c_str(), 2, true, "deactive"))
	if (mqttClient.connect((char*)clientName.c_str()))
	{
		mqttClient.publish(topicWill.c_str(), "active", true);
		sendToDebug("connected to MQTT broker successfully\n");
		String topicSubscribe = String(device_id) + SUFFIX_REMOTE;
		sendToDebug(String(" topic is: ") + topicSubscribe.c_str() + "\n");
		if (mqttClient.subscribe(topicSubscribe.c_str()))
			sendToDebug("successfully subscribed \n");
	}
	else
	{
		sendToDebug(String("MQTT connect failed, rc=") + mqttClient.state());
		sendToDebug(String("it will retry within 30 secs"));
	}
}
