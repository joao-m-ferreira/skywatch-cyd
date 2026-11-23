#include "mcp.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airline.h"

namespace jf {
namespace {

// A scratch buffer for the text of one tool result, held only while a request is handled (an ESP8266 cannot spare a
// permanent 4-6 KB). It is released when mcpHandle returns, after the response has been serialised.
#ifdef ESP8266
constexpr size_t kToolBuffer = 4096;
#else
constexpr size_t kToolBuffer = 6144;
#endif
struct Scratch {
  char* p = nullptr;
  char* take(size_t n) { return p = static_cast<char*>(malloc(n)); }
  ~Scratch() { free(p); }
};

// One plane as a JSON object: the fields an assistant needs, never the raw ADS-B record.
void addFlight(JsonObject o, const Flight& f) {
  o["callsign"] = f.callsign;
  o["aircraft_type"] = f.type;
  o["registration"] = f.reg;
  const Airline* al = airlineFor(f.callsign);
  if (al != nullptr) o["airline"] = al->name;
  o["altitude_ft"] = f.alt_ft;
  o["speed_kt"] = f.speed_kt;
  o["track_deg"] = f.track;
  o["distance_km"] = static_cast<int>(f.dist_km * 10.0f + 0.5f) / 10.0;
  o["bearing_deg"] = f.bearing;
  o["direction"] = compass(f.bearing);
}

// Serialises `doc` into `out`; returns 0 when it would not fit (nothing partial is ever written).
size_t finish(JsonDocument& doc, char* out, size_t n) {
  if (n == 0 || measureJson(doc) + 1 > n) return 0;
  return serializeJson(doc, out, n);
}

// Turns the response into a JSON-RPC error.
void setError(JsonDocument& res, int code, const char* message) {
  res.remove("result");
  res["error"]["code"] = code;
  res["error"]["message"] = message;
}

// One tool's text result in MCP's content format.
void setText(JsonDocument& res, const char* text, bool is_error) {
  JsonObject r = res["result"].to<JsonObject>();
  JsonObject item = r["content"].add<JsonObject>();
  item["type"] = "text";
  item["text"] = text;
  r["isError"] = is_error;
}

// Describes one tool (name, description and a JSON Schema for its input) for tools/list.
void addTool(JsonArray tools, const char* name, const char* description, bool with_limit) {
  JsonObject t = tools.add<JsonObject>();
  t["name"] = name;
  t["description"] = description;
  JsonObject schema = t["inputSchema"].to<JsonObject>();
  schema["type"] = "object";
  JsonObject props = schema["properties"].to<JsonObject>();
  if (with_limit) {
    JsonObject lim = props["limit"].to<JsonObject>();
    lim["type"] = "integer";
    lim["minimum"] = 1;
    lim["maximum"] = static_cast<int>(kMaxFlights);
    lim["description"] = "Most planes to return, nearest first (default: all that are shown)";
  }
}

}  // namespace

// See mcp.h.
size_t flightsJson(const McpContext& c, int limit, char* out, size_t n) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  const int cap = (limit > 0 && limit < c.count) ? limit : c.count;
  for (int i = 0; i < cap; ++i) addFlight(arr.add<JsonObject>(), c.flights[i]);
  return finish(doc, out, n);
}

// See mcp.h.
size_t statusJson(const McpContext& c, char* out, size_t n) {
  JsonDocument doc;
  doc["firmware"] = c.version;
  doc["board"] = c.panel;
  doc["uptime_s"] = c.uptime_s;
  doc["free_heap_bytes"] = c.free_heap;
  doc["wifi_connected"] = c.wifi;
  if (c.wifi) doc["wifi_rssi_dbm"] = c.rssi;
  if (c.last_fetch_age_s >= 0) doc["last_fetch_s_ago"] = c.last_fetch_age_s;
  doc["radius_km"] = c.radius_km;
  doc["max_planes"] = c.max_planes;
  doc["planes_found"] = c.count;
  if (c.count > 0 && c.selected >= 0 && c.selected < c.count) doc["on_screen"] = c.flights[c.selected].callsign;
  return finish(doc, out, n);
}

// See mcp.h.
size_t serverCardJson(const char* host, const char* version, bool auth_required, char* out, size_t n) {
  JsonDocument doc;
  doc["$schema"] = "https://static.modelcontextprotocol.io/schemas/v1/server-card.schema.json";
  doc["name"] = "io.github.joao-m-ferreira/skywatch-cyd";
  doc["version"] = version;
  doc["description"] = "Read-only view of the aircraft shown on a skywatch-cyd display";
  doc["title"] = "skywatch-cyd flight display";
  doc["websiteUrl"] = "https://joao-m-ferreira.github.io/skywatch-cyd/";
  doc["repository"]["url"] = "https://github.com/joao-m-ferreira/skywatch-cyd";
  doc["repository"]["source"] = "github";
  JsonObject remote = doc["remotes"].add<JsonObject>();
  remote["type"] = "streamable-http";
  remote["url"] = "http://{host}/mcp";
  JsonObject hv = remote["variables"]["host"].to<JsonObject>();
  hv["description"] = "The display's address on your network";
  hv["default"] = host;
  hv["isRequired"] = true;
  if (auth_required) {  // only when 'set mcpauth on': otherwise the endpoint takes requests without a header
    JsonObject header = remote["headers"].add<JsonObject>();
    header["name"] = "Authorization";
    header["description"] = "Access token of this display: the 'token' command over USB serial, or the settings page";
    header["isRequired"] = true;
    header["isSecret"] = true;
    header["value"] = "Bearer {token}";
    JsonObject tv = header["variables"]["token"].to<JsonObject>();
    tv["description"] = "The display's access token";
    tv["isRequired"] = true;
    tv["isSecret"] = true;
    JsonObject alt = remote["headers"].add<JsonObject>();  // accepted instead of Authorization
    alt["name"] = "X-API-Key";
    alt["description"] = "The same access token, for clients that cannot send an Authorization header";
    alt["isRequired"] = false;
    alt["isSecret"] = true;
    alt["value"] = "{token}";
  }
  remote["supportedProtocolVersions"].add(kMcpProtocol);
  return finish(doc, out, n);
}

// See mcp.h.
size_t aiCatalogJson(const char* host, char* out, size_t n) {
  JsonDocument doc;
  doc["specVersion"] = "1.0";
  JsonObject e = doc["entries"].add<JsonObject>();
  char buf[96];
  snprintf(buf, sizeof buf, "urn:air:%s:mcp:skywatch-cyd", host);
  e["identifier"] = buf;
  e["type"] = kCardMediaType;
  snprintf(buf, sizeof buf, "http://%s/mcp/server-card", host);
  e["url"] = buf;
  return finish(doc, out, n);
}

// See mcp.h: one JSON-RPC 2.0 request in, one response out (nothing for a notification).
size_t mcpHandle(const char* body, size_t len, const McpContext& c, char* out, size_t n) {
  Scratch scratch;  // declared here so it outlives the serialisation at the end
  JsonDocument req;
  JsonDocument res;
  res["jsonrpc"] = "2.0";
  if (deserializeJson(req, body, len) || !req.is<JsonObject>()) {
    res["id"] = nullptr;
    setError(res, -32700, "Parse error");
    return finish(res, out, n);
  }
  if (req["id"].isNull()) return 0;  // a notification: no response (e.g. notifications/initialized)
  res["id"] = req["id"];
  const char* method = req["method"] | "";

  if (strcmp(method, "initialize") == 0) {
    JsonObject r = res["result"].to<JsonObject>();
    r["protocolVersion"] = kMcpProtocol;
    r["capabilities"]["tools"].to<JsonObject>();
    r["serverInfo"]["name"] = "skywatch-cyd";
    r["serverInfo"]["version"] = c.version;
    r["instructions"] = "Read-only access to the aircraft shown on a skywatch-cyd display: the planes near it, "
                        "the one on screen, and the board's status.";
  } else if (strcmp(method, "ping") == 0) {
    res["result"].to<JsonObject>();
  } else if (strcmp(method, "tools/list") == 0) {
    JsonArray tools = res["result"]["tools"].to<JsonArray>();
    addTool(tools, "get_nearby_flights", "The aircraft near the display, nearest first: callsign, type, airline, altitude, speed, track, distance and bearing.", true);
    addTool(tools, "get_current_plane", "The aircraft currently shown on the display.", false);
    addTool(tools, "get_status", "The display's status: firmware version, uptime, WiFi, last data fetch, search radius.", false);
  } else if (strcmp(method, "tools/call") == 0) {
    const char* name = req["params"]["name"] | "";
    char* const buf = scratch.take(kToolBuffer);
    if (buf == nullptr) {
      setError(res, -32603, "Out of memory");
    } else if (strcmp(name, "get_nearby_flights") == 0) {
      const int limit = req["params"]["arguments"]["limit"] | 0;
      if (limit < 0 || limit > static_cast<int>(kMaxFlights)) {
        setError(res, -32602, "limit must be 1 to the number of planes shown");
      } else if (flightsJson(c, limit, buf, kToolBuffer) == 0) {
        setError(res, -32603, "Response too large");
      } else {
        setText(res, buf, false);
      }
    } else if (strcmp(name, "get_current_plane") == 0) {
      if (c.count == 0 || c.selected < 0 || c.selected >= c.count) {
        setText(res, "No aircraft in range right now.", false);
      } else {
        McpContext one = c;
        one.flights = c.flights + c.selected;
        one.count = 1;
        if (flightsJson(one, 1, buf, kToolBuffer) == 0) setError(res, -32603, "Response too large");
        else setText(res, buf, false);
      }
    } else if (strcmp(name, "get_status") == 0) {
      if (statusJson(c, buf, kToolBuffer) == 0) setError(res, -32603, "Response too large");
      else setText(res, buf, false);
    } else {
      setError(res, -32602, "Unknown tool");
    }
  } else {
    setError(res, -32601, "Method not found");
  }
  const size_t w = finish(res, out, n);
  if (w != 0) return w;
  JsonDocument small;  // the answer did not fit: say so rather than send half of it
  small["jsonrpc"] = "2.0";
  small["id"] = req["id"];
  setError(small, -32603, "Response too large");
  return finish(small, out, n);
}

}  // namespace jf
