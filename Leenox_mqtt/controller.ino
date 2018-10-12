//


#include "globals.h"
#include "FiniteStateMachine.h"
#include "FBD.h"
#include "dht_nonblocking.h"


// there are two FSM instanecs, 
// one is for mqtt connection
// so, if mqtt connection is disconnecetd by any reason, 
// it will retry in order to connnect again within 30 sec
// in all time, it is checking mqtt connection

// another fsm is for action of IR signal
// if any event button or mqtt command os triggered,
// program will start sedning IR signal task (RAW01, 5sec standby, RAW02) without program blocking
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
    WiFiManagerParameter custom_mqtt_server("server", "MQTT server address", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT server port", mqtt_port, 5);
    WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 32);
    WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", mqtt_pass, 32);
    WiFiManagerParameter custom_device_id("topic", "device topic", device_id, 80);
    WiFiManagerParameter custom_interval("pubInterval", "Data Publsihinbg Interval", pubIntervalTmp, 20);

    //WiFiManager
#ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
#endif

    WiFiManager wifiManager;
    wifiManager.setTimeout(180);

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_device_id);
    wifiManager.addParameter(&custom_interval);
    ////////////////////////////////////////////////////////////

    // when boot, if button is pressed or jumper is pluuged, it will erase setting on SPIFFS
    // and enter AP mode for configuration, so if you want to change wifi setting, device id, mqtt info etc,
    if(digitalRead(RESETPIN) == LOW)
    Serial.println(F("reset button is pressed"));
    else
    Serial.println(F("reset button is not pressed"));
  
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
        pubInterval = 300;

    // check mqtt port number
    String tmp = mqtt_port;
    if (tmp.toInt())
        mqtt_port_i = tmp.toInt();
    else
        mqtt_port_i = DEFAULT_MQTT_PORT;

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
            sendToDebug("*IR: failed to open config file for writing\n");
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

// publish temperature and humidity data
bool publishTempHumiData(double farenheight, double humidity)
{
    if (mqttClient.connected())
    {
        String temperTopic = String(device_id) + SUFFIX_TEMP;
        String humiTopic = String(device_id) + SUFFIX_HUMI;
        mqttClient.publish(temperTopic.c_str(), String(farenheight, 0).c_str());
        mqttClient.publish(humiTopic.c_str(), String(humidity, 0).c_str());
        return true;
    }
    return false;
}

// publish button state data
bool publishButtonState(bool buttonStatus)
{
    if (mqttClient.connected())
    {
        String buttonTopic = String(device_id) + SUFFIX_BUTT;
        if(buttonStatus)
            mqttClient.publish(buttonTopic.c_str(), "on");
        else
            mqttClient.publish(buttonTopic.c_str(), "off");
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
    // measure temperature and humidity from DHT11 once every 4 sec.
    static float temperature, tempFarenheight, humidity;
    static uint32_t dht11_TimeStamp = millis();
    if (millis() - dht11_TimeStamp > 2000ul)
    {
        if (dht_sensor.measure(&temperature, &humidity) == true)
        {
            tempFarenheight = temperature * 1.8F + 32.0F;
            Serial.print(tempFarenheight, 2);
            Serial.print(F("F, "));
            Serial.print(humidity, 2);
            Serial.println(F("%"));
            dht11_TimeStamp = millis();
        }
        else if (millis() - dht11_TimeStamp > 12000ul)
        {
            Serial.println(F("DHT11 sensor error"));
            dht11_TimeStamp = millis();
            // publishDht11Error();
        }
    }

    static uint32_t dht11SendingTime = millis();
    if (millis() - dht11SendingTime > pubInterval * 1000) // time is here, we will add setting for time, so you can change in setting page,
    {
        dht11SendingTime = millis();
        Serial.println(F("published temperature data"));
        publishTempHumiData(tempFarenheight, humidity);
    }

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

        /*
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
        */

        publishButtonState(true);

        if (airConController.isInState(airConIdle))
        {
            Serial.println(F("entered into step1 from idle, send raw1 code"));

            // send raw01 via IR module
            // irsend.sendRaw(rawData01, RAWDATA01LEN, TRANSMITTER_FREQ);
            // enter 
            airConController.transitionTo(airConStep1);
        }
    }

    // when button is released 
    if (buttonReleased.Q)
    {
        // excuted only once when button is released
        Serial.println(F("button released."));
        publishButtonState(false);

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
            // irsend.sendRaw(rawData02, RAWDATA02LEN, TRANSMITTER_FREQ);
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
    mqttMachine.update();
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

    unsigned long irSendCommandHex = hexToDec(msgString);
    Serial.print("IRDATA is 0x");
    Serial.println(String(irSendCommandHex, HEX));
    // irsend.setRaw(0xA1A06AFFFF44);
    // irsend.send();
    
    /*
    if (commandIndex != -1)
    {
        irsend.sendMidea(irSendCommandHex, );
        // irsend.sendRaw((uint16_t*)IRcommands[commandIndex], RAWDATALEN, TRANSMITTER_FREQ);
    }*/
    /*
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
    */

}

/************************************************
*  connect to MQTT broker
*/
void connect_to_MQTT()
{
    char myTopic[100];
    sendToDebug("connecting to nonTLS server:");
    mqttClient.setClient(wifiClient);

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
        sendToDebug("connected to MQTT broker successfully\n");
        String topicSubscribe = String(device_id) + SUFFIX_SUBS;
        sendToDebug(String("subscribe topic is ") + topicSubscribe.c_str() + "\n");
        if (mqttClient.subscribe(topicSubscribe.c_str()))
            sendToDebug("successfully subscribed \n");
    }
    else
    {
        sendToDebug(String("MQTT connect failed, rc=") + mqttClient.state());
        sendToDebug(String("it will retry within 10 secs"));
    }
}

unsigned long hexToDec(String hexString) {

    unsigned long decValue = 0;
    int nextInt;

    for (int i = 0; i < hexString.length(); i++) {

        nextInt = int(hexString.charAt(i));
        if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
        if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
        if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
        nextInt = constrain(nextInt, 0, 15);

        decValue = (decValue * 16) + nextInt;
    }

    return decValue;
}