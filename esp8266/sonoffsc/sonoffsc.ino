/*

Sonoff SC
Copyright (C) 2017 by Xose Pérez <xose dot perez at gmail dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Arduino.h>
#include "config/all.h"

// -----------------------------------------------------------------------------
// PROTOTYPES
// -----------------------------------------------------------------------------

#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

// NEW APP_WEBSITE
#include <AmazonIOTClient.h>
#include <ESP8266AWSImplementations.h>

#include <EEPROM.h> //library to handle with permament data after turning off Device
#include "ESP8266TrueRandom.h"


Esp8266HttpClient httpClient;
Esp8266DateTimeProvider dateTimeProvider;

AmazonIOTClient iotClient;
ActionError actionError;

char shadow[100];
//


void mqttRegister(void (*callback)(unsigned int, const char *, const char *));
template<typename T> bool setSetting(const String& key, T value);
template<typename T> String getSetting(const String& key, T defaultValue);
template<typename T> void mqttSend(const char * topic, T message);
template<typename T> void domoticzSend(unsigned int idx, T value);
template<typename T> void domoticzSend(const char * setting, T value);

// -----------------------------------------------------------------------------
// METHODS
// -----------------------------------------------------------------------------

void initAWS()
{
  iotClient.setAWSRegion("us-east-2");
  iotClient.setAWSEndpoint("amazonaws.com");
  iotClient.setAWSDomain("a1ud2nqx6i62qf.iot.us-east-2.amazonaws.com");
  iotClient.setAWSPath("/things/iotBemo/shadow");
  iotClient.setAWSKeyID("AKIAISLB53XWR6LH266Q");
  iotClient.setAWSSecretKey("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");
  iotClient.setHttpClient(&httpClient);
  iotClient.setDateTimeProvider(&dateTimeProvider);
}



String getIdentifier() {
    char identifier[20];
    sprintf(identifier, "%s_%06X", DEVICE, ESP.getChipId());
    return String(identifier);
}

bool ledStatus(bool status) {
    digitalWrite(LED_PIN, status ? LOW : HIGH);
    return status;
}

bool ledStatus() {
    return (digitalRead(LED_PIN) == LOW);
}

bool ledToggle() {
    return ledStatus(!ledStatus());
}

void ledBlink(unsigned long delayOff, unsigned long delayOn) {
    static unsigned long next = millis();
    if (next < millis()) {
        next += (ledToggle() ? delayOn : delayOff);
    }
}

void showStatus() {
    if (wifiConnected()) {
        if (WiFi.getMode() == WIFI_AP) {
            ledBlink(2000, 2000);
        } else {
            ledBlink(5000, 500);
        }
    } else {
        ledBlink(500, 500);
    }
}

void hardwareSetup() {
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED_PIN, OUTPUT);
    SPIFFS.begin();
}

void hardwareLoop() {

    showStatus();

    // Heartbeat
    static unsigned long last_heartbeat = 0;
    if (mqttConnected()) {
        if ((millis() - last_heartbeat > HEARTBEAT_INTERVAL) || (last_heartbeat == 0)) {
            last_heartbeat = millis();
            mqttSend(MQTT_HEARTBEAT_TOPIC, "1");
            DEBUG_MSG("[BEAT] Free heap: %d\n", ESP.getFreeHeap());
        }
    }

}


// -----------------------------------------------------------------------------
// BOOTING
// -----------------------------------------------------------------------------

void welcome() {

    delay(2000);
    DEBUG_MSG("%s %s\n", (char *) APP_NAME, (char *) APP_VERSION);
    DEBUG_MSG("%s\n%s\n\n", (char *) APP_AUTHOR, (char *) APP_WEBSITE);
    //DEBUG_MSG("Device: %s\n", (char *) getIdentifier().c_str());
    DEBUG_MSG("ChipID: %06X\n", ESP.getChipId());
    DEBUG_MSG("CPU frequency: %d MHz\n", ESP.getCpuFreqMHz());
    DEBUG_MSG("Last reset reason: %s\n", (char *) ESP.getResetReason().c_str());
    DEBUG_MSG("Memory size: %d bytes\n", ESP.getFlashChipSize());
    DEBUG_MSG("Free heap: %d bytes\n", ESP.getFreeHeap());
    DEBUG_MSG("Firmware size: %d bytes\n", ESP.getSketchSize());
    DEBUG_MSG("Free firmware space: %d bytes\n", ESP.getFreeSketchSpace());
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
        DEBUG_MSG("File system total size: %d bytes\n", fs_info.totalBytes);
        DEBUG_MSG("            used size : %d bytes\n", fs_info.usedBytes);
        DEBUG_MSG("            block size: %d bytes\n", fs_info.blockSize);
        DEBUG_MSG("            page size : %d bytes\n", fs_info.pageSize);
        DEBUG_MSG("            max files : %d\n", fs_info.maxOpenFiles);
        DEBUG_MSG("            max length: %d\n", fs_info.maxPathLength);
    }
    DEBUG_MSG("\n\n");

}



void setup() {

    Serial.begin(19200);

    hardwareSetup();
    welcome();

    settingsSetup();
    if (getSetting("hostname").length() == 0) {
        setSetting("hostname", getIdentifier());
        saveSettings();
    }
    DEBUG_MSG("buttonSetup()");
    buttonSetup();
    DEBUG_MSG("wifiSetup()");
    wifiSetup();
    DEBUG_MSG("otaSetup()");
    otaSetup();
    DEBUG_MSG("mqttSetup()");
    // mqttSetup();
    DEBUG_MSG("webSetup()");
    webSetup();
    DEBUG_MSG("commsSetup()");
    //commsSetup();
    DEBUG_MSG("fauxmoSetup()");
    fauxmoSetup();
    DEBUG_MSG("commConfigure()");
	  // commConfigure();

    initAWS();

	mqttRegister(mqttCallback);
}

void loop() {
    //DEBUG_MSG("Main() Loop %d", getTemperature());
    hardwareLoop();
    //commsLoop();

    readUart();



    buttonLoop();
    wifiLoop();
    otaLoop();
    // mqttLoop();
    fauxmoLoop();

}




String readUart(void)
{
    static bool get_at_flag = false;
    static String rec_string = "";
    int16_t index1;

    while ((Serial.available() > 0))
    {
        char a = (char)Serial.read();
        rec_string +=a;

        if(a == 0x1B)
        {
             break;
        }
    }

    StaticJsonBuffer<200> jsonBuffer;

    //DEBUG_MSG("Read: %s", rec_string.c_str());
    if(-1 != rec_string.indexOf(0x1B))
    {

      DEBUG_MSG("Read: %s", rec_string.c_str());
        if(-1 != (index1 = rec_string.indexOf("AT+STATUS?")))
        {
          Serial.write("AT+STATUS=4");
        DEBUG_MSG("Send status=4");
        }
        else if (-1 != (index1 = rec_string.indexOf("AT+UPDATE")))
        {
          Serial.write("AT+SEND=ok");
          String msg = "{"+rec_string.substring(10,rec_string.length()-1)+"}";
          JsonObject& root = jsonBuffer.parseObject(msg.c_str());
          int humidity = root["humidity"];
          double temp  = 1.8*double(root["temperature"])+32;
          int noise    = root["noise"];
          int dust   = root["dusty"];
          int light   = root["light"];
          DEBUG_MSG("UPDATE! %s", msg.c_str());
          DEBUG_MSG("Data: %d %d %d %d %d", humidity, temp, noise, dust, light);

          long mem = ESP.getFreeHeap();
          DEBUG_MSG("freeMemory()= %d", mem);

          strcpy(shadow, ("{\"state\":{\"reported\":{\"test_value1\":66, \"test_value2\":666}} }"));
          char* result = iotClient.update_shadow(shadow, actionError);
          DEBUG_MSG("Message: %s", result);

        }
        else
        {
            /*do nothing*/
        }
        rec_string = "";
    }
    else if(-1 == rec_string.indexOf("AT"))
    {
        Serial.flush();
    }
    else
    {
        /*do nothing*/
    }
}


void awsloop()
{
  long mem = ESP.getFreeHeap();
  Serial.print("freeMemory()=");
    Serial.println(mem);


  // strcpy(shadow, ("{\"state\":{\"reported\":{\"test_value1\":66, \"test_value2\":666}}, \"id_\": \""+ clientID + "\" }").c_str());
  strcpy(shadow, ("{\"state\":{\"reported\":{\"test_value1\":66, \"test_value2\":666}} }"));

  Serial.println("Trying to send data");
  Serial.print(shadow);

  char* result = iotClient.update_shadow(shadow, actionError);
  Serial.print(result);

  delete[] result;

  delay(10000);
}
