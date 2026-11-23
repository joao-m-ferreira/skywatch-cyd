// WiFi setup mode: the board opens its own access point and serves a settings page (with a captive-portal
// DNS, so phones open it by themselves). The station connection keeps retrying meanwhile.
#pragma once
#include "config.h"

#include "net_compat.h"

namespace jf {

// Registers the settings page and its save handler on `server`: "/settings" and "/save" (and "/" too when
// `setup_mode`, where the page is the only thing the board serves). `guard`, if given, runs first and returns
// false (after answering the request itself) when the caller may not see the page. `cfg` is edited when the
// form is saved (all values are validated first; nothing changes if one is wrong); then `on_saved` is called.
void settingsRegister(WebServer* server, Config* cfg, void (*on_saved)(), bool (*guard)(), bool setup_mode);

// Starts the access point `ap_ssid` / `ap_pass`, the DNS and the web server. `cfg` is edited when the form
// is saved (all values are validated first; nothing changes if one is wrong); `on_saved` is then called.
void portalStart(Config* cfg, const char* ap_ssid, const char* ap_pass, void (*on_saved)());
void portalLoop();  // call often: answers DNS and HTTP requests
void portalStop();
bool portalActive();

}  // namespace jf
