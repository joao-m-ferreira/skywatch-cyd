// The web page and MCP server on the board's own address (both on by default; `set web off`, `set mcp off` and
// `set advertise off` switch them and the mDNS announcement off). The web page needs the access token; /mcp
// needs it only with `set mcpauth on` (default off). The MCP tools are read-only.
//   /            the planes and a link to the settings          (web page on)
//   /api/flights /api/status   the same data as JSON            (web page on)
//   /settings    the settings page, as in setup mode            (web page on)
//   /mcp         MCP over HTTP POST (JSON-RPC 2.0)              (MCP on)
#pragma once
#include "config.h"
#include "mcp.h"

namespace jf {

using McpSnapshot = McpContext (*)();  // what the board knows right now; called per request

// Starts the server on port 80 (and the name skywatch.local). WiFi must be connected.
void webStart(Config* cfg, McpSnapshot snapshot, void (*on_saved)());
void webLoop();  // call often
void webStop();
bool webActive();
bool webWantsRestart();  // advertising was switched off while running: the name is withdrawn by restarting

}  // namespace jf
