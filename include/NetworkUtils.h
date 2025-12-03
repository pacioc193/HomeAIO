#pragma once
#include <Arduino.h>
#include <HTTPClient.h>

String httpGet(String url);
String httpPost(String url, String body);
