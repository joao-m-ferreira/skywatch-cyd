#include "portal.h"

#include <string.h>

#include "net_compat.h"
#include "portal_util.h"

namespace {
jf::Config* gCfg = nullptr;
void (*gSaved)() = nullptr;
WebServer* gServer = nullptr;
#ifndef ESP8266
DNSServer* gDns = nullptr;  // setup mode only: the ESP8266 build has no setup access point
#endif
bool (*gGuard)() = nullptr;  // runs before every settings request (access token check on a normal network)
bool gSetup = false;         // true: the board's own setup WiFi, false: the board's address on your network

const char kHead[] PROGMEM =
    "<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>skywatch setup</title><style>"
    "body{font:16px system-ui,sans-serif;max-width:30rem;margin:0 auto;padding:16px;background:#f4ead0;color:#2c2016}"
    "h1{font-size:1.4rem;margin:.2em 0}label{display:block;margin:.9em 0 .2em;font-weight:600}"
    "input,select{width:100%;box-sizing:border-box;padding:.6em;font-size:1rem;border:1px solid #b0a082;border-radius:6px;background:#fffdf6}"
    ".row{display:flex;gap:.6em}.row>*{flex:1}small{color:#6b5a44}"
    "button{margin-top:1.4em;width:100%;padding:.8em;font-size:1.05rem;border:0;border-radius:6px;background:#962820;color:#fff}"
    ".err{background:#fbe3df;border:1px solid #962820;padding:.6em;border-radius:6px}</style><body>";

// Appends text to the page, HTML-escaped (SSIDs and contacts are untrusted input).
void appendEscaped(String& out, const char* text) {
  char buf[140];
  jf::htmlEscape(text, buf, sizeof buf);
  out += buf;
}

// Appends a labelled text input holding its current value.
void appendInput(String& out, const char* label, const char* name, const char* value, const char* extra = "") {
  out += "<label>";
  out += label;
  out += "</label><input name=";
  out += name;
  out += " value=\"";
  appendEscaped(out, value);
  out += "\" ";
  out += extra;
  out += ">";
}

// Serves the settings form, with an error message above it when there is one.
void sendForm(const char* error) {
  String h = FPSTR(kHead);
  h += "<h1>skywatch setup</h1><p>Choose your WiFi and where you are. The display then joins your WiFi on its own.</p>";
  if (error != nullptr && error[0] != '\0') {
    h += "<p class=err>";
    appendEscaped(h, error);
    h += "</p>";
  }
  h += "<form method=post action=/save>";
  h += "<label>WiFi network</label><input name=ssid list=nets autocomplete=off required maxlength=32 value=\"";
  appendEscaped(h, gCfg->ssid);
  h += "\"><datalist id=nets>";
  const int found = WiFi.scanComplete();
  for (int i = 0; i < found && i < 20; ++i) {
    h += "<option value=\"";
    appendEscaped(h, WiFi.SSID(i).c_str());
    h += "\">";
  }
  if (found >= 0) WiFi.scanDelete();
  h += "</datalist>";
  h += "<label>WiFi password</label><input name=pass type=password autocomplete=off maxlength=64 placeholder=\"";
  h += gCfg->pass[0] ? "(unchanged)" : "(none for an open network)";
  h += "\">";
  h += "<label>Location</label><select name=loc><option value=auto";
  h += gCfg->auto_loc ? " selected" : "";
  h += ">Work it out from my internet address</option><option value=static";
  h += gCfg->auto_loc ? "" : " selected";
  h += ">Use the coordinates below</option></select>";
  char num[24];
  h += "<div class=row><div>";
  snprintf(num, sizeof num, "%.4f", gCfg->lat);
  appendInput(h, "Latitude", "lat", num, "inputmode=decimal");
  h += "</div><div>";
  snprintf(num, sizeof num, "%.4f", gCfg->lon);
  appendInput(h, "Longitude", "lon", num, "inputmode=decimal");
  h += "</div></div>";
  h += "<div class=row><div>";
  snprintf(num, sizeof num, "%d", gCfg->radius_km);
  appendInput(h, "Radius (km)", "radius", num, "type=number min=1 max=250");
  h += "</div><div>";
  snprintf(num, sizeof num, "%d", gCfg->plane_sec);
  appendInput(h, "Seconds per plane", "plane", num, "type=number min=3 max=120");
  h += "</div></div>";
  h += "<label>Night mode (screen off)</label><div class=row><select name=night><option value=on";
  h += gCfg->night_on ? " selected" : "";
  h += ">On</option><option value=off";
  h += gCfg->night_on ? "" : " selected";
  h += ">Off</option></select>";
  snprintf(num, sizeof num, "%d", gCfg->night_start);
  h += "<input name=from type=number min=0 max=23 title='from hour' value=";
  h += num;
  snprintf(num, sizeof num, "%d", gCfg->night_end);
  h += "><input name=to type=number min=0 max=23 title='to hour' value=";
  h += num;
  h += "></div><small>From and to, as hours 0-23.</small>";
  appendInput(h, "Contact for plane photos (optional)", "contact", gCfg->contact, "type=email placeholder=you@example.com");
  h += "<small>Sent to planespotters.net with photo requests, as they ask.</small>";
  h += "<label>Web page on the board</label><select name=web><option value=off";
  h += gCfg->web_on ? "" : " selected";
  h += ">Off</option><option value=on";
  h += gCfg->web_on ? " selected" : "";
  h += ">On</option></select><small>The planes and these settings at the board's own address on your network, behind an access token.</small>";
  h += "<label>MCP needs the access token in a header</label><select name=mcpauth><option value=off";
  h += gCfg->mcp_auth ? "" : " selected";
  h += ">Off: anyone on the network can read the planes</option><option value=on";
  h += gCfg->mcp_auth ? " selected" : "";
  h += ">On: Authorization: Bearer or X-API-Key</option></select><small>The MCP tools only read plane data and the board's status; they cannot change anything. This page always needs the token.</small>";
  h += "<label>Announce on the network</label><select name=adv><option value=off";
  h += gCfg->adv_on ? "" : " selected";
  h += ">Off</option><option value=on";
  h += gCfg->adv_on ? " selected" : "";
  h += ">On</option></select><small>The name skywatch.local, the _mcp._tcp service and the public Server Card. Off hides the board; use its IP address.</small>";
  h += "<label>MCP server (for AI assistants)</label><select name=mcp><option value=off";
  h += gCfg->mcp_on ? "" : " selected";
  h += ">Off</option><option value=on";
  h += gCfg->mcp_on ? " selected" : "";
  h += ">On</option></select><small>Read-only tools at /mcp: the planes near the display, the one on screen, its status. Needs the access token, made when you switch it on.</small>";
  h += "<button>Save and connect</button></form></body></html>";
  gServer->send(200, "text/html", h);
}

// Runs one settings command on `tmp`; false (and the reason in `why`) if the parser rejected it.
bool run(jf::Config* tmp, const String& cmd, String* why) {
  char reply[200];
  jf::configCommand(tmp, cmd.c_str(), reply, sizeof reply);
  if (strncmp(reply, "error", 5) == 0 || strncmp(reply, "unknown", 7) == 0 || strncmp(reply, "use:", 4) == 0) {
    *why = reply;
    return false;
  }
  return true;
}

// Applies a posted form. Every value goes through the same parser as the serial commands, on a copy of the settings:
// one wrong value changes nothing.
void handleSave() {
  if (gGuard != nullptr && !gGuard()) return;
  if (gServer->args() > 24) {  // the form has about a dozen fields
    sendForm("Too many fields.");
    return;
  }
  for (int i = 0; i < gServer->args(); ++i) {
    if (!jf::formValueOk(gServer->arg(i).c_str())) {
      sendForm("A value had a line break or control character.");
      return;
    }
  }
  jf::Config tmp = *gCfg;  // all or nothing
  String why;
  const String ssid = gServer->arg("ssid");
  const String pass = gServer->arg("pass");
  bool ok = run(&tmp, "set ssid " + ssid, &why);
  if (ok && (pass.length() > 0 || ssid != gCfg->ssid)) ok = run(&tmp, "set pass " + (pass.length() ? pass : String("-")), &why);
  if (ok) {
    if (gServer->arg("loc") == "static") ok = run(&tmp, "set location static " + gServer->arg("lat") + " " + gServer->arg("lon"), &why);
    else ok = run(&tmp, "set location auto", &why);
  }
  if (ok && gServer->arg("radius").length()) ok = run(&tmp, "set radius " + gServer->arg("radius"), &why);
  if (ok && gServer->arg("plane").length()) ok = run(&tmp, "set plane " + gServer->arg("plane"), &why);
  if (ok) {
    if (gServer->arg("night") == "off") ok = run(&tmp, "set night off", &why);
    else ok = run(&tmp, "set night " + gServer->arg("from") + " " + gServer->arg("to"), &why);
  }
  if (ok && gServer->arg("contact").length()) ok = run(&tmp, "set contact " + gServer->arg("contact"), &why);
  if (ok && gServer->hasArg("web")) ok = run(&tmp, String("set web ") + (gServer->arg("web") == "on" ? "on" : "off"), &why);
  if (ok && gServer->hasArg("mcp")) ok = run(&tmp, String("set mcp ") + (gServer->arg("mcp") == "on" ? "on" : "off"), &why);
  if (ok && gServer->hasArg("mcpauth")) ok = run(&tmp, String("set mcpauth ") + (gServer->arg("mcpauth") == "on" ? "on" : "off"), &why);
  if (ok && gServer->hasArg("adv")) ok = run(&tmp, String("set advertise ") + (gServer->arg("adv") == "on" ? "on" : "off"), &why);
  if (ok && (tmp.web_on || tmp.mcp_on) && tmp.token[0] == '\0') {  // first time either is on: make the access token
    uint32_t words[4];
    for (uint32_t& w : words) w = platformRandom();
    char tok[25];
    jf::randomToken(words, tok);
    ok = run(&tmp, String("set token ") + tok, &why);
  }
  if (!ok) {
    sendForm(why.c_str());
    return;
  }
  *gCfg = tmp;
  String h = FPSTR(kHead);
  if (gSetup) {
    h += "<h1>Saved</h1><p>The display is joining <b>";
    appendEscaped(h, gCfg->ssid);
    h += "</b> now. When that works, this setup WiFi disappears and the display shows planes.</p>"
         "<p>If it is still showing the setup screen after a minute, the password was probably wrong: "
         "reconnect to the setup WiFi and try again.</p>";
    if (gCfg->web_on || gCfg->mcp_on) {  // shown once, over the private setup WiFi
      h += "<p><b>Access token</b> for the web page and the MCP server (keep it private):</p><p><code style='font-size:1.2rem'>";
      appendEscaped(h, gCfg->token);
      h += "</code></p><p>Later, open <b>http://skywatch.local/</b> (user <code>skywatch</code>, the token as password)";
      if (gCfg->mcp_on) h += ", and point an MCP client at <b>http://skywatch.local/mcp</b> with the header <code>Authorization: Bearer &lt;token&gt;</code>";
      h += ". The <code>token</code> command over USB serial shows it again.</p>";
    }
    h += "</body></html>";
  } else {
    h += "<h1>Saved</h1><p>The settings are saved; the display applies them in a moment.</p><p><a href=/>Back to the planes</a></p></body></html>";
  }
  gServer->send(200, "text/html", h);
  if (gSaved != nullptr) gSaved();
}

// GET handler for the settings page (behind the guard, when there is one).
void handleForm() {
  if (gGuard != nullptr && !gGuard()) return;
  sendForm(nullptr);
}

#ifndef ESP8266
void redirectToPortal() {  // phones probe a few well-known addresses; sending them here opens the page
  gServer->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  gServer->send(302, "text/plain", "");
}
#endif  // !ESP8266
}  // namespace

namespace jf {

// Registers the settings routes on a server; see portal.h.
void settingsRegister(WebServer* server, Config* cfg, void (*on_saved)(), bool (*guard)(), bool setup_mode) {
  gServer = server;
  gCfg = cfg;
  gSaved = on_saved;
  gGuard = guard;
  gSetup = setup_mode;
  server->on("/settings", HTTP_GET, handleForm);
  server->on("/save", HTTP_POST, handleSave);
  if (setup_mode) server->on("/", HTTP_GET, handleForm);
}

#ifndef ESP8266
// Starts setup mode: the access point, the captive-portal DNS and the settings page.
void portalStart(Config* cfg, const char* ap_ssid, const char* ap_pass, void (*on_saved)()) {
  if (gServer != nullptr) portalStop();
  WiFi.mode(WIFI_AP_STA);  // the station side keeps trying to join the saved network
  WiFi.softAP(ap_ssid, ap_pass);
  delay(100);
  gDns = new DNSServer();
  gDns->start(53, "*", WiFi.softAPIP());
  gServer = new WebServer(80);
  settingsRegister(gServer, cfg, on_saved, nullptr, true);
  gServer->onNotFound(redirectToPortal);
  gServer->begin();
  WiFi.scanNetworks(true);  // async: the list is ready by the time the page is opened
}

// Serves pending DNS and HTTP requests.
void portalLoop() {
  if (gServer == nullptr) return;
  gDns->processNextRequest();
  gServer->handleClient();
}

// Closes the setup page and the access point.
void portalStop() {
  if (gServer != nullptr) {
    gServer->stop();
    delete gServer;
    gServer = nullptr;
  }
  gGuard = nullptr;
  gSetup = false;
  if (gDns != nullptr) {
    gDns->stop();
    delete gDns;
    gDns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

bool portalActive() { return gDns != nullptr; }
#endif  // !ESP8266

}  // namespace jf
