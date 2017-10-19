#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <AmazonIOTClient.h>
#include <ESP8266AWSImplementations.h>

#include <EEPROM.h> //library to handle with permament data after turning off Device

#include <ArduinoJson.h>

Esp8266HttpClient httpClient;
Esp8266DateTimeProvider dateTimeProvider;

AmazonIOTClient iotClient;
ActionError actionError;

// Your WIFI Setttings
const char* ssid = "Time_machine";
const char* password = "P66988165r";

// Your IAM credentials
const char* AWSKeyID = "AKIAISLB53XWR6LH266Q";
const char* AWSSecretKey = "N12kNqd9QFX0AqopLlJEogHXiz+17o5Eqm2UfV7p";

const char* AWSPath = "/things/iotBemo/shadow";
const char* AWSDomain = "a1ud2nqx6i62qf.iot.us-east-2.amazonaws.com";
const char* AWSRegion = "us-east-2";


char shadow[800];


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
          Serial.write("AT+STATUS=4");
        // DEBUG_MSG("Send status=4");
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

          Serial.println("UPDATE! " + msg);
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
          //strcpy(shadow, ("{\"state\":{\"reported\":{\"test_value1\":6, \"test_value2\":1}} }"));
          char* result = iotClient.update_shadow(shadow, actionError);
          // Serial.print(result);
          delay(6000);
          ESP.getFreeHeap();
          delete[] result;
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



void initWLAN()
{
  WiFi.begin(ssid, password);
  Serial.print("Trying to connect to wifi ");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(". ");
  }
  Serial.println("Connected! ");
}

void initAWS()
{
  iotClient.setAWSRegion(AWSRegion);
  iotClient.setAWSEndpoint("amazonaws.com");
  iotClient.setAWSDomain(AWSDomain);
  iotClient.setAWSPath(AWSPath );
  iotClient.setAWSKeyID(AWSKeyID);
  iotClient.setAWSSecretKey(AWSSecretKey);
  iotClient.setHttpClient(&httpClient);
  iotClient.setDateTimeProvider(&dateTimeProvider);
}

void setup() {
  Serial.begin(19200);
  delay(10);
  Serial.println("begin");
  initWLAN();
  Serial.println("wlan initialized");
  initAWS();
  Serial.println("iot initialized");

  // clientID = get_clientId();
  long ID = ESP.getChipId();
  chip_id = String(ID, HEX);
  Serial.println(chip_id.c_str());

}

void loop()
{

  long mem = ESP.getFreeHeap();
  // Serial.print("freeMemory()=");
  //   Serial.println(mem);

  readUart();

  // Serial.printf ("ESP8266 Chip id =%08X \n", ESP.getChipId ());
  // Serial.printf ("ESP8266 Chip id =%s \n", id.c_str());

  delay(1000);
}
