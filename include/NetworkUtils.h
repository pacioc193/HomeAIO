#pragma once

#include <Arduino.h>
#include <HTTPClient.h>

struct HttpResult {
	int code; // HTTP status code (<=0 for network errors)
	String payload; // response body
};

HttpResult httpGet(String url);
HttpResult httpPost(String url, String body);
void setWifiPins(int clk, int cmd, int d0, int d1, int d2, int d3, int rst);
