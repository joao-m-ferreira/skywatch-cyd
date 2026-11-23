// The network libraries differ between the ESP32 and the ESP8266; this is the one place that knows how. The web page,
// settings form, JSON API and MCP server code (portal.cpp, webapi.cpp) is written once against these names.
#pragma once
#include <stdint.h>

#ifdef ESP8266
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
using WebServer = ESP8266WebServer;  // same calls, different class name
// A 32-bit random number from the chip's hardware generator.
inline uint32_t platformRandom() { return ESP.random(); }
#else
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
// A 32-bit random number from the chip's hardware generator.
inline uint32_t platformRandom() { return esp_random(); }
#endif
