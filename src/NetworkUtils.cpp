#include "NetworkUtils.h"
#include <WiFi.h>

HttpResult httpGet(String url) {
    HTTPClient http;
    HttpResult res;
    res.code = -1;
    res.payload = "";
    http.begin(url);
    http.setTimeout(2000);
    int httpCode = http.GET();
    res.code = httpCode;
    if (httpCode > 0) {
        res.payload = http.getString();
    }
    http.end();
    return res;
}

HttpResult httpPost(String url, String body) {
    HTTPClient http;
    HttpResult res;
    res.code = -1;
    res.payload = "";
    http.begin(url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);
    res.code = httpCode;
    if (httpCode > 0) {
        res.payload = http.getString();
    }
    http.end();
    return res;
}

// Set WiFi SDIO pins (useful for M5Tab5)
void setWifiPins(int clk, int cmd, int d0, int d1, int d2, int d3, int rst) {
    WiFi.setPins(clk, cmd, d0, d1, d2, d3, rst);
}
