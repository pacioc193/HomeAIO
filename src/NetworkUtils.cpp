#include "NetworkUtils.h"

String httpGet(String url) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);
    int httpCode = http.GET();
    String payload = "{}";
    if (httpCode > 0) {
        payload = http.getString();
    }
    http.end();
    return payload;
}

String httpPost(String url, String body) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);
    String payload = "{}";
    if (httpCode > 0) {
        payload = http.getString();
    }
    http.end();
    return payload;
}
