### Sonoff SC


## PlatformIO Dependencies
* Paho
* WebSocketsClient
* ArduinoJson

## Modify config

```
//AWS IOT config, change these:
char wifi_ssid[]       = "XXXX";
char wifi_password[]   = "XXXX";
char aws_endpoint[]    = "XXXXXX.iot.XXXXX.amazonaws.com";
char aws_key[]         = "XXXX";
char aws_secret[]      = "XXXX";
char aws_region[]      = "XXXXXX";
const char* aws_topic  = "$aws/things/XXXXX/shadow/update";
```

# Flashing ESP connections
![alt text][s1]





[s1]: https://raw.githubusercontent.com/charlielito/sonoffsc-tinker-original-mix/master/sample.jpeg "S"
