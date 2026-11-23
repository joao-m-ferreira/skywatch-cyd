# Network interfaces

What the board offers on your network, how to find it, and how to talk to it. This page covers every build. The **ESP8266** build serves the same web page, JSON API, MCP server and mDNS names
with a few limits, listed in [ESP8266](#esp8266); only the WiFi setup access point is missing there. All of it is
tested in the esp32sim emulator (`scripts/emu-web.sh`) and by host tests (`scripts/test.sh`); on real boards it has
not been used from another computer yet (the ESP8266 starts the server, announces itself and keeps fetching). The example payloads below were generated from the firmware's own code.

| Interface | Address | Needs | Switch (default) |
|---|---|---|---|
| [Web page](#web-interface) | `http://skywatch.local/` | access token (login `skywatch`) | `set web on\|off` (on) |
| [JSON API](#json-api) | `/api/flights`, `/api/status` | access token | follows the web page |
| [MCP server](#mcp-server) | `POST /mcp` | token only if header authentication is on | `set mcp on\|off` (on), `set mcpauth on\|off` (off) |
| [Discovery](#discovery) | mDNS `skywatch.local`, `_mcp._tcp`; `/mcp/server-card`; `/.well-known/ai-catalog.json` | nothing (public, no secrets) | `set advertise on\|off` (on) |
| [Setup access point](#setup-access-point) | `http://192.168.4.1/` on the WiFi `skywatch-XXXX` | the one-time password on the screen | opens by itself or `setup` |

All of it is plain **HTTP on port 80**, on your local network. Treat the board as a LAN service and never forward its port to
the internet. Switch off what you do not use: with `web` and `mcp` both off the board opens no port.

## Discovery

### Names
- **`skywatch.local`** (mDNS, also called Bonjour or Avahi): the board's name on the network. Use it instead of an IP address.
- **`_http._tcp`** service on port 80: shows up in browsers that list local web services.
- **`_mcp._tcp`** service on port 80, only while MCP is on, with this TXT record (nothing secret in it):

  | Key | Value |
  |---|---|
  | `path` | `/mcp` |
  | `card` | `/mcp/server-card` |
  | `auth` | `bearer` when header authentication is on, `none` when it is off |
  | `version` | the firmware version, for example `0.5.0` |

  The `_mcp._tcp` name is a convention, not a standard.

### Finding the board
```sh
# macOS
dns-sd -B _mcp._tcp                 # browse for MCP servers
dns-sd -L skywatch _mcp._tcp local  # host, port and TXT record of one
dns-sd -B _http._tcp                # web servers, which includes the board
ping skywatch.local

# Linux (avahi)
avahi-browse -rt _mcp._tcp
avahi-resolve -n skywatch.local

# Windows (recent Windows 10 and 11 resolve .local names themselves)
ping skywatch.local
```
If the name does not resolve, use the IP address: it is printed on the serial console when the web server starts
(`web: http://192.168.1.50/ ...`) and shown by `status`; your router's list of connected devices has it too.

### Discovery documents
With **MCP and advertising both on**, two public documents describe the endpoint. They hold no secret: never the token,
never the WiFi or your position.

- **MCP Server Card** at `GET /mcp/server-card`, media type `application/mcp-server-card+json`. Name, version, the endpoint, the
  header it needs (only when header authentication is on) and the protocol version:

  ```json
  {
    "$schema": "https://static.modelcontextprotocol.io/schemas/v1/server-card.schema.json",
    "name": "io.github.joao-m-ferreira/skywatch-cyd",
    "version": "0.5.0",
    "description": "Read-only view of the aircraft shown on a skywatch-cyd display",
    "title": "skywatch-cyd flight display",
    "websiteUrl": "https://joao-m-ferreira.github.io/skywatch-cyd/",
    "repository": { "url": "https://github.com/joao-m-ferreira/skywatch-cyd", "source": "github" },
    "remotes": [{
      "type": "streamable-http",
      "url": "http://{host}/mcp",
      "variables": { "host": { "description": "The display's address on your network", "default": "skywatch.local", "isRequired": true } },
      "supportedProtocolVersions": ["2025-03-26"]
    }]
  }
  ```
  With `set mcpauth on` the remote also lists an `Authorization: Bearer {token}` header (required) and an `X-API-Key: {token}` header
  (optional alternative), both marked secret.

- **AI Catalog** at `GET /.well-known/ai-catalog.json`, media type `application/ai-catalog+json`, pointing at the card:

  ```json
  { "specVersion": "1.0",
    "entries": [{ "identifier": "urn:air:skywatch.local:mcp:skywatch-cyd",
                  "type": "application/mcp-server-card+json",
                  "url": "http://skywatch.local/mcp/server-card" }] }
  ```

### What discovers it
Nothing finds it by itself yet: current MCP clients (desktop assistants, editors) do not browse the network for servers, and the
protocol has no discovery of its own. The pieces above are for tools you write or configure: browse `_mcp._tcp`, read the card it
points to, then connect. A script that does that and writes your client's configuration is the usual way.

### Hiding the board
`set advertise off` stops the announcement and the public documents (404). Turning it off restarts the board once (the name cannot
be withdrawn while it runs); afterwards it answers only on its IP address.

## Access token
The first time the web page or MCP is on, the board makes a **24-character token** (120 bits, no look-alike characters) and prints it.

| Command (USB serial) | Does |
|---|---|
| `token` | prints the token |
| `token new` | replaces it (everything using the old one stops working) |
| `set token <16-32 letters and digits>` | sets your own |
| `show` | never prints it (it says `token set`) |

The setup page also shows it once, over the private setup WiFi, after you save.

Wrong tokens are rate limited: **five wrong tokens in a row lock every request out for a minute** (`429`, `Retry-After: 60`). A request
with no token at all (a browser's first, unanswered request) does not count.

## Web interface
Open `http://skywatch.local/` in a browser. It asks for a login: user **`skywatch`**, the token as the password (HTTP Basic).

| Route | What |
|---|---|
| `GET /` | the planes, refreshing every 10 s, the plane on screen highlighted; a link to the settings |
| `GET /settings` | the settings form (the same as the setup page) |
| `POST /save` | applies the form |

The form has the WiFi name and password, location, radius, seconds per plane, night mode, the photo contact, and the switches for the web
page, MCP, header authentication and advertising. Every value goes through the same parser as the serial commands, on a copy of the
settings: one wrong value changes nothing and the page says why. A field that is not in the post is left unchanged. The password field is
never filled in: left empty it keeps the saved password, unless you change the WiFi name, where empty means an open network. The token is never shown here.

Turning the web page off makes these routes answer `404`. With the web page off you can still change settings over USB serial or in setup mode.

## JSON API
Same login as the web page (`curl -u skywatch:TOKEN`), and it follows the web-page switch.

```sh
curl -u skywatch:TOKEN http://skywatch.local/api/flights
curl -u skywatch:TOKEN http://skywatch.local/api/status
```

`GET /api/flights`: the planes the display holds, nearest first (at most `set max`, default 10):
```json
[{"callsign":"TAP443","aircraft_type":"A21N","registration":"CS-TJA","airline":"TAP Portugal","altitude_ft":6800,
  "speed_kt":259,"track_deg":271,"distance_km":22.2,"bearing_deg":180,"direction":"S"},
 {"callsign":"G-ABCD","aircraft_type":"C172","registration":"G-ABCD","altitude_ft":1500,
  "speed_kt":95,"track_deg":90,"distance_km":9.4,"bearing_deg":45,"direction":"NE"}]
```
`airline` appears only when the operator is known (not for registrations such as `G-ABCD`). `bearing_deg` and `direction` say where the plane is
from the display; `track_deg` is where it is heading.

`GET /api/status`:
```json
{"firmware":"0.5.0","board":"ST7735S M5StickC","uptime_s":3600,"free_heap_bytes":98000,"wifi_connected":true,
 "wifi_rssi_dbm":-52,"last_fetch_s_ago":7,"radius_km":40,"max_planes":10,"planes_found":2,"on_screen":"TAP443"}
```
It never contains the WiFi name or password, the token or your position. `last_fetch_s_ago` is absent before the first good fetch; the data
is refreshed every 20 s.

## MCP server
A read-only [Model Context Protocol](https://modelcontextprotocol.io/) server, so an AI assistant can ask what is flying overhead.

- **Endpoint:** `POST http://skywatch.local/mcp`, one JSON-RPC 2.0 request in the body, one response back (`Content-Type: application/json`).
  Stateless: no sessions, no streaming. Protocol version `2025-03-26`.
- **Authentication:** off by default. With `set mcpauth on`, send the token as `Authorization: Bearer <token>` or `X-API-Key: <token>`.
  Off, anyone on your network can read the planes and the status (all the tools are read-only). The web page always needs the token.
- **Browsers are refused:** a request with an `Origin` header gets `403`, so a web page open in your browser cannot talk to the board.
- **Methods:** `initialize`, `ping`, `tools/list`, `tools/call`. A notification (no `id`, such as `notifications/initialized`) gets `202` and no body.

### Tools
| Tool | Input | Returns |
|---|---|---|
| `get_nearby_flights` | `limit` (integer 1-20, optional) | the planes, nearest first, as in `/api/flights` |
| `get_current_plane` | none | the plane on the display (or the text `No aircraft in range right now.`) |
| `get_status` | none | the board status, as in `/api/status` |

Nothing can change a setting through MCP.

### Examples
```sh
URL=http://skywatch.local/mcp
H='Content-Type: application/json'                         # add -H "Authorization: Bearer TOKEN" with mcpauth on

curl -s $URL -H "$H" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"demo","version":"1"}}}'
curl -s $URL -H "$H" -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'
curl -s $URL -H "$H" -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_nearby_flights","arguments":{"limit":2}}}'
```

`initialize` answers:
```json
{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2025-03-26","capabilities":{"tools":{}},
 "serverInfo":{"name":"skywatch-cyd","version":"0.5.0"},
 "instructions":"Read-only access to the aircraft shown on a skywatch-cyd display: the planes near it, the one on screen, and the board's status."}}
```

A `tools/call` result carries the data as JSON **text** in the MCP content format:
```json
{"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"[{\"callsign\":\"TAP443\",\"aircraft_type\":\"A21N\",...}]"}],"isError":false}}
```

### Errors
JSON-RPC errors come back with HTTP `200`:

| Code | When |
|---|---|
| `-32700` | the body is not valid JSON (`id` is `null`) |
| `-32601` | unknown method |
| `-32602` | unknown tool, or `limit` out of range |
| `-32603` | the answer would not fit the reply buffer |

HTTP-level answers: `401` token missing or wrong (with a `WWW-Authenticate` challenge), `403` request from a browser, `404` MCP is off,
`405` anything but `POST` (`Allow: POST`), `400` empty body, `413` body over 8 KB, `429` locked out, `503` out of memory.

### Configuring a client
A client that takes a JSON configuration looks roughly like this (the exact keys differ between clients). Leave `headers` out while
header authentication is off:
```json
{ "mcpServers": { "skywatch": { "url": "http://skywatch.local/mcp", "headers": { "Authorization": "Bearer <token>" } } } }
```

## Setup access point
A board with no WiFi saved, or that cannot reach its WiFi for 3 minutes, opens its own WiFi **`skywatch-XXXX`** with a random one-time
password. The screen shows a QR code that joins it, the name, the password and `192.168.4.1`. A phone opens the settings page by itself
(captive portal); otherwise browse to `http://192.168.4.1/`. It also starts with `setup` over serial or by holding the board's button for 3
seconds (T-Display, M5StickC and the OLED boards). The page is the same form as `/settings`; setup mode ends by itself once the board is
connected. The web page, API and MCP are not served while it is open.

## Settings at a glance
| Command | What |
|---|---|
| `set web on\|off` | web page and JSON API |
| `set mcp on\|off` | MCP endpoint |
| `set mcpauth on\|off` | require the token in a header for `/mcp` (default off) |
| `set advertise on\|off` | mDNS name, `_mcp._tcp`, Server Card, AI Catalog (restarts once when turned off) |
| `token`, `token new`, `set token <...>` | the access token |
| `set max <1-20>` | most planes listed |
| `setup` | open the setup access point |
| `status`, `show`, `version`, `update` | state, settings, firmware version, check for a newer release |

## ESP8266
The same code serves the ESP8266 build (`esp8266-oled`), within an 80 KB RAM budget:
- Request bodies over **2 KB** are refused (`413`; the ESP32 allows 8 KB), and a tool result that would not fit a 4 KB buffer answers
  `-32603 Response too large`: lower `set max`.
- **No setup access point**: set the WiFi over USB serial or with the portal's browser setup form.
- mDNS changes apply live (no restart for `set advertise off`).
- While the board fetches planes or looks up a route (a few seconds) it does not answer requests; a client simply waits.
- Memory is tight: about 24 KB free when idle and 10 KB while fetching. A fetch that finds too little says "Not enough memory for TLS"
  instead of crashing; `set web off` and `set mcp off` give the memory back.

## Troubleshooting
| Symptom | Try |
|---|---|
| `skywatch.local` does not resolve | use the IP address (serial `status`, or your router); older Windows and some Android browsers do not resolve `.local` names; mDNS only crosses a single network segment, not VLANs or guest networks |
| `401` in the browser | the user is `skywatch` and the password is the token (`token` over serial) |
| `429 Too many wrong tokens` | wait a minute; check the token with `token` |
| `404` | the interface is switched off (`show` tells you), or advertising is off for the card and catalog |
| `403` on `/mcp` | the request came from a browser page; use a normal MCP client or `curl` |
| `dns-sd -B _mcp._tcp` finds nothing | MCP and advertising must both be on (`show`), and you must be on the same network segment |
| nothing answers at all | `web` and `mcp` both off closes the port; `set web on` |

## Testing
- `scripts/test.sh` runs the MCP handler, the discovery documents, the token helpers and the settings parser as host tests.
- `scripts/emu-web.sh` boots the real firmware in the esp32sim emulator and checks the web page, the API, MCP (including authentication,
  the lock-out, browser refusal and every error), the Server Card and catalog, saving settings, and each switch turning its interface off.
