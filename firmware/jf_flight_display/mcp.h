// A small Model Context Protocol (MCP) server core: JSON-RPC 2.0 requests in, responses out, with read-only
// tools that describe the planes the board has found. Written against the open MCP specification (the
// "initialize", "tools/list" and "tools/call" methods over HTTP POST); no Arduino headers, so it is host-tested.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "flights.h"

namespace jf {

constexpr const char* kMcpProtocol = "2025-03-26";

// A snapshot of what the board knows; the sketch fills it in for each request.
struct McpContext {
  const Flight* flights = nullptr;  // nearest first
  int count = 0;
  int selected = 0;                 // the plane on screen
  const char* version = "dev";
  uint32_t uptime_s = 0;
  uint32_t free_heap = 0;
  bool wifi = false;
  int rssi = 0;
  int last_fetch_age_s = -1;        // seconds since the last good fetch, -1 if none yet
  int radius_km = 0;
  int max_planes = 0;
  const char* panel = "";
};

// The planes as a JSON array, at most `limit` of them (0 = all). Returns the bytes written (0 if `n` is too small).
size_t flightsJson(const McpContext& c, int limit, char* out, size_t n);
// The board's status as a JSON object (version, uptime, WiFi, last fetch; no WiFi password, token or position).
size_t statusJson(const McpContext& c, char* out, size_t n);

// Discovery. The Server Card (media type application/mcp-server-card+json, served at /mcp/server-card) describes
// the endpoint and the header it needs, never the token itself: cards are public. The AI Catalog (media type
// application/ai-catalog+json, at /.well-known/ai-catalog.json) points to the card. `host` is the board's name on
// the network ("skywatch.local").
constexpr const char* kCardMediaType = "application/mcp-server-card+json";
constexpr const char* kCatalogMediaType = "application/ai-catalog+json";
size_t serverCardJson(const char* host, const char* version, bool auth_required, char* out, size_t n);
size_t aiCatalogJson(const char* host, char* out, size_t n);

// Handles one JSON-RPC request body and writes the response JSON to `out`. Returns the bytes written, or 0
// when nothing is to be sent (a notification such as "notifications/initialized"). Never writes past `n`.
size_t mcpHandle(const char* body, size_t len, const McpContext& c, char* out, size_t n);

}  // namespace jf
