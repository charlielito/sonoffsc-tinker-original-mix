#include <Arduino.h>
#include <Stream.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h> //

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h> // paho
#include <Countdown.h>
#include <MQTTClient.h>

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

#include <EEPROM.h> //library to handle with permament data after turning off Device

#include <ArduinoJson.h>

#define BUTTON_PIN 0
#define LED_PIN 13


//  --------- Config ---------- //
//AWS IOT config, change these:
char wifi_ssid[]       = "kiwibot1";
char wifi_password[]   = "kiwicampus1";
char aws_endpoint[]    = "a1ud2nqx6i62qf.iot.us-east-1.amazonaws.com";
char aws_key[]         = "XXXXXXXXXXXXXXXXX";
char aws_secret[]      = "XXXXXXXXXXXXXXXXX";
char aws_region[]      = "us-east-1";
const char* aws_topic  = "$aws/things/Sensor1/shadow/update";
int port = 443;

//MQTT config
const int maxMQTTpackageSize = 1024;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;

char shadow[700];

String chip_id;



String read_eeprom(int index, int len){
  String str = "";
  for (int i = index; i < len + index; ++i)
    str.concat( char(EEPROM.read(i)) );
  return str;
}


void write_eeprom(uint32_t address, String str){
  for (int i = 0; i < str.length(); i++)
      EEPROM.write(address+i, str.charAt(i));
  EEPROM.commit();
}


void readUart(void)
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
    Serial.println(rec_string);
    if(-1 != rec_string.indexOf(0x1B))
    {
      delay(100);
      // DEBUG_MSG("Read: %s", rec_string.c_str());
        if(-1 != (index1 = rec_string.indexOf("AT+STATUS?")))
        {
          delay(1000);
          Serial.write("AT+STATUS=4");
        // DEBUG_MSG("Send status=4");
        }
        else if (-1 != (index1 = rec_string.indexOf("AT+UPDATE")))
        {
          delay(1000);
          Serial.write("AT+SEND=ok");

          String check_str = rec_string.substring(15,rec_string.length()-1);
          int index = rec_string.lastIndexOf("AT+UPDATE");
          // if (-1 != index) index = rec_string.indexOf("AT+UPDATE");
          // Serial.println(index);


          String msg = "{"+rec_string.substring(index+10,rec_string.length()-1)+"}";
          JsonObject& root = jsonBuffer.parseObject(msg.c_str());
          int humidity = root["humidity"];
          double temp  = 1.8*double(root["temperature"])+32;
          int noise    = root["noise"];
          int dust   = root["dusty"];
          int light   = root["light"];

          // Serial.println("UPDATE! " + msg);
          // DEBUG_MSG("Data: %d %d %d %d %d", humidity, temp, noise, dust, light);

          String config_msg =  "\"config\": {\"bemo_type\": \"bemo-test\", \"version\": \"0.0.1\", \"iotname\": \"esp8266_"+chip_id+"\" }";

          String light_msg = "\"light_sensor\": {\"light_value\":" + String(light) + ", \"light_value_old\":" + String(light) + ", \"antiglare\": 0, \"reliability\": 1}";
          String temp_msg = "\"temperature_sensor\": {\"temperature_value\":" + String(temp) + ", \"temperature_value_old\":" + String(temp) + ", \"reliability\": 1}";
          String humidity_msg = "\"humidity_sensor\": {\"humidity_value\":" + String(humidity) + ", \"humidity_value_old\":" + String(humidity) + ", \"reliability\": 1}";
          String noise_msg = "\"noise_sensor\": {\"noise_value\":" + String(noise) + ", \"noise_value_old\":" + String(noise) + ", \"reliability\": 1}";
          String dust_msg = "\"dust_sensor\": {\"dust_value\":" + String(dust) + ", \"dust_value_old\":" + String(dust) + ", \"reliability\": 1}";

          String total_msg = config_msg + "," + light_msg + "," + temp_msg + "," + humidity_msg + "," + noise_msg + "," + dust_msg;

          long mem = ESP.getFreeHeap();

          // msg = "{\"state\":{\"reported\":" + msg + "}}";
          msg = "{\"state\":{\"reported\":{" + total_msg + "} }}";

          strcpy(shadow, msg.c_str());



          //send a message
          MQTT::Message message;
          // char buf[100];
          // strcpy(buf, "{\"state\":{\"reported\":{\"on\": false}, \"desired\":{\"on\": false}}}");
          message.qos = MQTT::QOS0;
          message.retained = false;
          message.dup = false;
          message.payload = (void*)shadow;
          message.payloadlen = strlen(shadow)+1;
          int rc = client->publish(aws_topic, message);

          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);
          delay(200);
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);

          // delay(5000);
          // DEBUG_MSG("Message: %s", result);

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




//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;
  //
  // Serial.print("Message ");
  // Serial.print(++arrivedcount);
  // Serial.print(" arrived: qos ");
  // Serial.print(message.qos);
  // Serial.print(", retained ");
  // Serial.print(message.retained);
  // Serial.print(", dup ");
  // Serial.print(message.dup);
  // Serial.print(", packetid ");
  // Serial.println(message.id);
  // Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  // Serial.println(msg);
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect () {

    if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    } else {

      if (client->isConnected ()) {
        client->disconnect ();
      }
      delete client;
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    }


    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");




   int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return false;
    } else {
      Serial.println("websocket layer connected");
    }


    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    char* clientID = generateClientID ();
    data.clientID.cstring = clientID;
    rc = client->connect(data);
    delete[] clientID;
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
      return false;
    }
    Serial.println("MQTT connected");
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscript to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

//send a message to a mqtt topic
void sendmessage () {
    //send a message
    MQTT::Message message;
    char buf[100];
    strcpy(buf, "{\"state\":{\"reported\":{\"on\": false}, \"desired\":{\"on\": false}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(aws_topic, message);
}


void setup() {
    Serial.begin (19200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    delay (2000);
    Serial.setDebugOutput(1);

    //fill with ssid and wifi password
    WiFiMulti.addAP(wifi_ssid, wifi_password);
      Serial.println ("connecting to wifi");
    while(WiFiMulti.run() != WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        Serial.print (".");
    }
    Serial.println ("\nconnected");

    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");


    long ID = ESP.getChipId();
    chip_id = String(ID, HEX);
    Serial.println(chip_id.c_str());

    //fill AWS parameters
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);

    if (connect ()){
      subscribe ();
      // sendmessage ();
    }

}

void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
      client->yield();
      // sendmessage();
      readUart();
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();
    }
  }

  delay(1000);

}
