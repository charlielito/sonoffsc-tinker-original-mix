/*

WIFI MODULE

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

#include "JustWifi.h"
#include <DNSServer.h>

DNSServer dnsServer;

// -----------------------------------------------------------------------------
// WIFI
// -----------------------------------------------------------------------------

String getIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

String getNetwork() {
    if (WiFi.getMode() == WIFI_AP) {
        return jw.getAPSSID();
    }
    return WiFi.SSID();
}

void wifiDisconnect() {
    jw.disconnect();
}

void resetConnectionTimeout() {
    jw.resetReconnectTimeout();
}

bool wifiConnected() {
    return jw.connected();
}

bool createAP() {
    return jw.createAP();
}

void wifiConfigure() {
    jw.scanNetworks(true);
    jw.setHostname(getSetting("hostname", HOSTNAME).c_str());
    jw.setSoftAP(getSetting("hostname", HOSTNAME).c_str(), getSetting("adminPass", ADMIN_PASS).c_str());
    jw.setAPMode(AP_MODE_ALONE);
    jw.cleanNetworks();
    for (int i = 0; i< WIFI_MAX_NETWORKS; i++) {
        if (getSetting("ssid" + String(i)).length() == 0) break;
        if (getSetting("ip" + String(i)).length() == 0) {
            jw.addNetwork(
                getSetting("ssid" + String(i)).c_str(),
                getSetting("pass" + String(i)).c_str()
            );
        } else {
            jw.addNetwork(
                getSetting("ssid" + String(i)).c_str(),
                getSetting("pass" + String(i)).c_str(),
                getSetting("ip" + String(i)).c_str(),
                getSetting("gw" + String(i)).c_str(),
                getSetting("mask" + String(i)).c_str(),
                getSetting("dns" + String(i)).c_str()
            );
        }
    }
}

void wifiSetup() {
  DEBUG_MSG("Wifi setup initited");
  jw.scanNetworks(true);
  jw.cleanNetworks();
  jw.setAPMode(AP_MODE_OFF);
  jw.addNetwork("Time_machine", "P66988165r");
  delay(1000);
  DEBUG_MSG("Connectetd: %d",wifiConnected());

}

void wifiLoop() {
    jw.loop();

    if (WiFi.getMode() == WIFI_AP) {
        dnsServer.processNextRequest();
    }
}
