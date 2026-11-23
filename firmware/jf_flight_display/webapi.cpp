#include "webapi.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef ESP8266
#include <mdns.h>  // the IDF call that removes one mDNS service
#endif
#ifndef ESP8266
#include "model3d.h"  // the 3D carousel is for the ESP32 builds
#endif
#include "net_compat.h"
#include "portal.h"
#include "portal_util.h"

namespace {
WebServer* gServer = nullptr;
jf::Config* gCfg = nullptr;
jf::McpSnapshot gSnapshot = nullptr;
int gFails = 0;
uint32_t gLockedUntil = 0;  // after 5 wrong tokens in a row, every request is refused for a minute
bool gLocked = false;
bool gMdnsOn = false;  // whether the mDNS records are published right now
bool gMdnsMcp = false;
bool gMdnsAuth = false;  // the auth flag the published TXT record shows

const char kDashboard[] PROGMEM =
    "<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>skywatch</title><style>"
    "body{font:16px system-ui,sans-serif;max-width:40rem;margin:0 auto;padding:16px;background:#f4ead0;color:#2c2016}"
    "h1{font-size:1.4rem}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:.4em .5em;border-bottom:1px solid #d8c9a0}"
    "th{font-size:.8rem;color:#6b5a44;text-transform:uppercase}.sel{background:#efe0b8}small{color:#6b5a44}a{color:#962820}"
    "#car{margin:0 0 1rem;text-align:center}#cv{width:100%;max-width:640px;height:auto;background:#fbf5e4;border:1px solid #d8c9a0;border-radius:8px}"
    "#cap{margin:.4rem 0;font-weight:600}#ctl button{border:0;background:none;font-size:1.6rem;color:#962820;cursor:pointer;padding:0 .5rem}"
    "#dots button{width:.7rem;height:.7rem;border-radius:50%;padding:0;margin:0 .25rem;background:#d8c9a0;font-size:0}#dots button.on{background:#962820}"
    "</style><body><h1>skywatch</h1>"
#ifndef ESP8266
    // The 3D carousel (ESP32 builds): one plane at a time, turning, painted from /api/model (the same mesh and livery
    // colours as the display). The table below stays for the numbers.
    "<div id=car><canvas id=cv width=640 height=360></canvas><div id=cap></div>"
    "<div id=ctl><button id=pv aria-label=Previous>&lsaquo;</button><span id=dots></span><button id=nx aria-label=Next>&rsaquo;</button></div></div>"
    "<script>"
    R"JS(
(function(){
const cv=document.getElementById('cv'),cx=cv.getContext('2d'),cap=document.getElementById('cap'),dots=document.getElementById('dots');
let planes=[],cur=0,pausedUntil=0,mesh=null,meshKey='',loading='',spin=0,last=0,nextAt=0;
const cache=new Map();
function parse(b){const d=new DataView(b),nv=d.getUint16(0,true),nt=d.getUint16(2,true),len=d.getFloat32(4,true);
let o=8;const V=new Float32Array(nv*3);for(let i=0;i<nv*3;i++,o+=2)V[i]=d.getInt16(o,true)/100;
const T=[];for(let i=0;i<nt;i++,o+=12)T.push({a:d.getUint16(o,true),b:d.getUint16(o+2,true),c:d.getUint16(o+4,true),
col:[d.getUint8(o+6),d.getUint8(o+7),d.getUint8(o+8)],ox:d.getInt8(o+9),oy:d.getInt8(o+10),oz:d.getInt8(o+11)});
return{V,T,len};}
async function need(p){const k=p.callsign+'|'+p.aircraft_type+'|'+(p.altitude_ft<1500&&p.speed_kt<200);
if(cache.has(k)){mesh=cache.get(k);meshKey=k;return;}if(loading===k)return;loading=k;
try{const r=await fetch('/api/model?c='+encodeURIComponent(p.callsign));if(r.ok){const m=parse(await r.arrayBuffer());
if(cache.size>5)cache.delete(cache.keys().next().value);cache.set(k,m);if(planes[cur]&&planes[cur].callsign===p.callsign){mesh=m;meshKey=k;}}}
catch(e){}loading='';}
const rad=x=>x*Math.PI/180;
function draw(){const W=cv.width,H=cv.height;cx.clearRect(0,0,W,H);if(!mesh)return;
const m=mesh,p=planes[cur],pitch=rad(24),cp=Math.cos(pitch),sp=Math.sin(pitch),ca=Math.cos(spin),sa=Math.sin(spin);
const ext=Math.max(m.len,1),dist=3*ext,n=m.V.length/3,X=new Float32Array(n),Y=new Float32Array(n),D=new Float32Array(n);
for(let i=0;i<n;i++){const x=m.V[3*i],y=m.V[3*i+1],z=m.V[3*i+2],x1=x*ca-y*sa,y1=x*sa+y*ca,U=z*cp+y1*sp,d=y1*cp-z*sp,s=dist/(dist+d);
D[i]=d;X[i]=x1*s;Y[i]=U*s;}
const sc=W*1.0/ext*(0.55+0.45*Math.min(1,ext/70)),ox=W/2,oy=H*0.55,L=Math.hypot(-0.45,0.8,-0.4);
const out=[];
for(const pass of[0,1])for(const t of m.T){const ov=t.ox||t.oy||t.oz;if((pass===1)!==!!ov)continue;
if(ov){const y1=t.ox*sa+t.oy*ca;if(y1*cp-t.oz*sp>=0)continue;}
const wx=i=>X[i]*(dist+D[i])/dist,wy=i=>Y[i]*(dist+D[i])/dist;
const ax=wx(t.b)-wx(t.a),ay=wy(t.b)-wy(t.a),ad=D[t.b]-D[t.a],bx=wx(t.c)-wx(t.a),by=wy(t.c)-wy(t.a),bd=D[t.c]-D[t.a];
let nx=ay*bd-ad*by,ny=ad*bx-ax*bd,nd=ax*by-ay*bx;const nl=Math.hypot(nx,ny,nd);if(nl<1e-6)continue;
if(nd>0){nx=-nx;ny=-ny;nd=-nd;}
const lam=Math.max(0,(nx*-0.45+ny*0.8+nd*-0.4)/(nl*L)),sh=0.62+0.45*lam;
out.push({t,pass,z:(D[t.a]+D[t.b]+D[t.c])/3,sh});}
out.sort((a,b)=>a.pass-b.pass||b.z-a.z);
for(const o of out){const t=o.t,c=t.col.map(v=>Math.min(255,v*o.sh|0)),col='rgb('+c+')';cx.fillStyle=col;cx.strokeStyle=col;cx.lineWidth=0.8;
cx.beginPath();cx.moveTo(ox+X[t.a]*sc,oy-Y[t.a]*sc);cx.lineTo(ox+X[t.b]*sc,oy-Y[t.b]*sc);cx.lineTo(ox+X[t.c]*sc,oy-Y[t.c]*sc);cx.closePath();cx.fill();cx.stroke();}}
function show(){const p=planes[cur];dots.textContent='';
planes.forEach((q,i)=>{const b=document.createElement('button');b.className=i===cur?'on':'';b.setAttribute('aria-label',q.callsign);b.onclick=()=>go(i,true);dots.appendChild(b);});
if(!p){cap.textContent='No aircraft in range';mesh=null;return;}
cap.textContent=p.callsign+(p.airline?' – '+p.airline:'')+' · '+p.aircraft_type+' · '+p.altitude_ft+' ft · '+p.speed_kt+' kt · '+p.distance_km+' km '+p.direction;
mesh=null;need(p);}
function go(i,manual){if(!planes.length)return;cur=(i+planes.length)%planes.length;if(manual)pausedUntil=Date.now()+20000;nextAt=Date.now()+8000;show();}
document.getElementById('pv').onclick=()=>go(cur-1,true);document.getElementById('nx').onclick=()=>go(cur+1,true);
async function poll(){try{const f=await(await fetch('/api/flights')).json(),was=planes[cur]&&planes[cur].callsign;planes=f;
const i=f.findIndex(q=>q.callsign===was);cur=i>=0?i:0;if(i<0||!mesh)show();else{dots.children.length!==f.length&&show();}}catch(e){}}
function frame(t){const dt=last?(t-last)/1000:0;last=t;spin+=dt*0.5;
if(planes.length>1&&Date.now()>pausedUntil&&Date.now()>nextAt)go(cur+1,false);draw();requestAnimationFrame(frame);}
poll();setInterval(poll,10000);requestAnimationFrame(frame);
})();
)JS"
    "</script>"
#endif
    "<p id=st><small>loading...</small></p>"
    "<table><thead><tr><th>Flight<th>Type<th>Alt (ft)<th>Speed (kt)<th>Distance</tr></thead><tbody id=rows></tbody></table>"
    "<p><a href=/settings>Settings</a></p><script>"
    "async function tick(){try{const f=await (await fetch('/api/flights')).json(),s=await (await fetch('/api/status')).json();"
    "const b=document.getElementById('rows');b.textContent='';"
    "for(const p of f){const r=b.insertRow();if(p.callsign===s.on_screen)r.className='sel';"
    "for(const v of [p.callsign+(p.airline?' - '+p.airline:''),p.aircraft_type,p.altitude_ft,p.speed_kt,p.distance_km+' km '+p.direction])r.insertCell().textContent=v;}"
    "if(!f.length){const c=b.insertRow().insertCell();c.colSpan=5;c.textContent='No aircraft in range';}"
    "document.getElementById('st').textContent='Firmware '+s.firmware+', up '+Math.round(s.uptime_s/60)+' min, '+(s.last_fetch_s_ago!==undefined?'updated '+s.last_fetch_s_ago+' s ago':'no data yet');"
    "}catch(e){document.getElementById('st').textContent='Could not load: '+e;}}tick();setInterval(tick,10000);"
    "</script></html>";

// The token the client presented: "Bearer <token>" or Basic auth with the token as the password.
bool presentedToken(char* out, size_t n, bool allow_api_key = false) {
  out[0] = '\0';
  if (allow_api_key) {
    const String key = gServer->header("X-API-Key");
    if (key.length() > 0 && key.length() < n) {
      snprintf(out, n, "%s", key.c_str());
      return true;
    }
  }
  const String h = gServer->header("Authorization");
  if (h.length() == 0 || h.length() > 200) return false;
  const char* tok = jf::bearerToken(h.c_str());
  if (tok[0] != '\0') {
    snprintf(out, n, "%s", tok);
    return true;
  }
  if (!h.startsWith("Basic ")) return false;
  unsigned char dec[160];
  const int len = jf::base64Decode(h.c_str() + 6, h.length() - 6, dec, sizeof dec - 1);
  if (len < 0) return false;
  dec[len] = '\0';
  const char* colon = strchr(reinterpret_cast<const char*>(dec), ':');
  if (colon == nullptr) return false;
  snprintf(out, n, "%s", colon + 1);
  return true;
}

// True if the request carries the right token; otherwise answers 401 (or 429 while locked) and returns false.
bool authorised(bool basic_challenge, bool allow_api_key = false) {
  if (gLocked && static_cast<int32_t>(millis() - gLockedUntil) < 0) {
    gServer->sendHeader("Retry-After", "60");
    gServer->send(429, "text/plain", "Too many wrong tokens; try again in a minute.\n");
    return false;
  }
  gLocked = false;
  char tok[64];
  const bool presented = presentedToken(tok, sizeof tok, allow_api_key);  // a request with no credentials is just the first, unanswered one
  if (presented && gCfg->token[0] != '\0' && jf::secretEquals(tok, gCfg->token)) {
    gFails = 0;
    return true;
  }
  if (presented && ++gFails >= 5) {  // only wrong tokens count towards the lock-out
    gFails = 0;
    gLocked = true;
    gLockedUntil = millis() + 60000;
  }
  gServer->sendHeader("WWW-Authenticate", basic_challenge ? "Basic realm=\"skywatch\", charset=\"UTF-8\"" : "Bearer realm=\"skywatch\"");
  gServer->send(401, "text/plain", "Access token required.\n");
  return false;
}

// Guards the read-only routes (the plane list and /api/*): 404 when the web page is off; the login is required unless
// `set webauth off` opened them. The settings routes use pageGuard() below and are never open.
bool viewGuard() {
  if (!gCfg->web_on) {
    gServer->send(404, "text/plain", "Not found\n");
    return false;
  }
  return gCfg->web_open ? true : authorised(true);
}

// Guards the settings routes: 404 when the web page is off. With `set webauth on` the token is required; with it off
// (the default) there is no login anywhere on the web page, settings included.
bool pageGuard() {
  if (!gCfg->web_on) {
    gServer->send(404, "text/plain", "Not found\n");
    return false;
  }
  return gCfg->web_open ? true : authorised(true);
}

// GET /: the planes page, a small page that polls /api/flights.
void handleDashboard() {
  if (!viewGuard()) return;
  gServer->send_P(200, "text/html", kDashboard);
}

// Builds a JSON document into a temporary buffer and sends it (503 when memory is short).
void sendJson(size_t (*build)(const jf::McpContext&, int, char*, size_t), int limit) {
#ifdef ESP8266
  const size_t cap = 3072;  // about 8 planes
#else
  const size_t cap = 6144;
#endif
  char* buf = static_cast<char*>(malloc(cap));
  if (buf == nullptr) {
    gServer->send(503, "text/plain", "Out of memory\n");
    return;
  }
  const size_t w = build(gSnapshot(), limit, buf, cap);
  if (w == 0) gServer->send(500, "text/plain", "Response too large\n");
  else gServer->send(200, "application/json", buf);
  free(buf);
}

size_t statusAdapter(const jf::McpContext& c, int, char* out, size_t n) { return jf::statusJson(c, out, n); }

// GET /api/flights: the planes as JSON.
void handleFlights() {
  if (!viewGuard()) return;
#ifdef ESP8266
  sendJson(jf::flightsJson, 8);  // as many planes as fit the buffer
#else
  sendJson(jf::flightsJson, 0);
#endif
}

// GET /api/status: the board's status as JSON (nothing secret in it).
void handleStatus() {
  if (!viewGuard()) return;
  sendJson(statusAdapter, 0);
}

#ifndef ESP8266
// GET /api/model?c=<callsign>: the plane's 3D model for the web page's carousel, in the livery the display uses.
// Binary, little-endian: u16 vertices, u16 triangles, f32 length (m); then the vertices (3 x i16, centimetres: x forward,
// y right, z up); then the triangles (3 x u16 vertex index, 3 x u8 colour, 3 x i8 overlay direction). The same mesh
// the screen draws; at most ~19 KB, streamed in small pieces so no big buffer is needed.
void handleModel() {
  if (!viewGuard()) return;
  const jf::McpContext& c = gSnapshot();
  const jf::Flight* f = nullptr;
  const String want = gServer->arg("c");
  for (int i = 0; i < c.count && want.length() > 0; ++i) {
    if (want == c.flights[i].callsign) f = &c.flights[i];
  }
  if (f == nullptr) {
    gServer->send(404, "text/plain", "No such aircraft\n");
    return;
  }
  const jf::Palette pal = jf::paletteFor(f->callsign, f->alt_ft, f->speed_kt);
  const jf::Mesh& m = jf::buildAircraft(f->type, pal);
  const size_t total = 8 + m.nv * 6 + m.nt * 12;
  gServer->sendHeader("Cache-Control", "max-age=60");
  gServer->setContentLength(total);
  gServer->send(200, "application/octet-stream", "");
  uint8_t buf[768];
  size_t used = 0;
  auto flush = [&] {
    if (used > 0) gServer->sendContent(reinterpret_cast<const char*>(buf), used);
    used = 0;
  };
  auto put = [&](const void* p, size_t n) {
    if (used + n > sizeof buf) flush();
    memcpy(buf + used, p, n);
    used += n;
  };
  const uint16_t nv = static_cast<uint16_t>(m.nv), nt = static_cast<uint16_t>(m.nt);
  const float len = m.length;
  put(&nv, 2);
  put(&nt, 2);
  put(&len, 4);
  for (size_t i = 0; i < m.nv; ++i) {
    const int16_t q[3] = {static_cast<int16_t>(lroundf(m.v[i].x * 100.0f)), static_cast<int16_t>(lroundf(m.v[i].y * 100.0f)),
                          static_cast<int16_t>(lroundf(m.v[i].z * 100.0f))};
    put(q, 6);
  }
  for (size_t i = 0; i < m.nt; ++i) {
    const jf::Tri& t = m.t[i];
    const uint16_t idx[3] = {t.a, t.b, t.c};
    const int8_t tail[6] = {static_cast<int8_t>(t.color.r), static_cast<int8_t>(t.color.g), static_cast<int8_t>(t.color.b), t.ox, t.oy, t.oz};
    put(idx, 6);
    put(tail, 6);
  }
  flush();
}
#endif

// Discovery documents: public by design (they hold no secret), and only offered while the MCP server is on.
void handleCard() {
  if (!gCfg->mcp_on || !gCfg->adv_on) {
    gServer->send(404, "text/plain", "Not found\n");
    return;
  }
  char buf[1536];
  if (jf::serverCardJson("skywatch.local", gSnapshot().version, gCfg->mcp_auth, buf, sizeof buf) == 0) gServer->send(500, "text/plain", "Too large\n");
  else gServer->send(200, jf::kCardMediaType, buf);
}

// GET /.well-known/ai-catalog.json: points to the Server Card. Public by design; only while MCP and advertising are on.
void handleCatalog() {
  if (!gCfg->mcp_on || !gCfg->adv_on) {
    gServer->send(404, "text/plain", "Not found\n");
    return;
  }
  char buf[512];
  if (jf::aiCatalogJson("skywatch.local", buf, sizeof buf) == 0) gServer->send(500, "text/plain", "Too large\n");
  else gServer->send(200, jf::kCatalogMediaType, buf);
}

// POST /mcp: one JSON-RPC request in, one response out. Requests from a browser (with an Origin header) are
// refused, and the token is required when header authentication is on.
void handleMcp() {
  if (!gCfg->mcp_on) {
    gServer->send(404, "text/plain", "Not found\n");
    return;
  }
  if (gServer->method() != HTTP_POST) {
    gServer->sendHeader("Allow", "POST");
    gServer->send(405, "text/plain", "POST JSON-RPC requests here.\n");
    return;
  }
  // Browsers always send an Origin; MCP clients that are not web pages do not. Refusing them stops a web page
  // you happen to have open from talking to the board.
  if (gServer->header("Origin").length() > 0) {
    gServer->send(403, "text/plain", "Browser requests are not accepted.\n");
    return;
  }
  if (gCfg->mcp_auth && !authorised(false, true)) return;  // header authentication is a switch, off by default
  const String body = gServer->arg("plain");
  if (body.length() == 0) {
    gServer->send(400, "text/plain", "Empty request.\n");
    return;
  }
#ifdef ESP8266
  constexpr size_t kMaxBody = 2048;  // a JSON-RPC request is a few hundred bytes; this chip has no room for more
#else
  constexpr size_t kMaxBody = 8192;
#endif
  if (body.length() > kMaxBody) {  // a JSON-RPC request here is a few hundred bytes; never parse something huge on this heap
    gServer->send(413, "text/plain", "Request too large.\n");
    return;
  }
#ifdef ESP8266
  const size_t cap = 3072;
#else
  const size_t cap = 8192;
#endif
  char* out = static_cast<char*>(malloc(cap));
  if (out == nullptr) {
    gServer->send(503, "text/plain", "Out of memory\n");
    return;
  }
  const size_t w = jf::mcpHandle(body.c_str(), body.length(), gSnapshot(), out, cap);
  if (w == 0) gServer->send(202, "text/plain", "");  // a notification: accepted, nothing to say
  else gServer->send(200, "application/json", out);
  free(out);
}
}  // namespace

namespace jf {

namespace {
bool gMdnsHttp = false;

#ifdef ESP8266
// No mDNS on the ESP8266 build: its responder takes about 4.7 KB of the ~26 KB that are free, which the HTTPS fetch needs
// (with it the fetch ran out of memory and the board crashed). The board is reached by its IP address, which the serial
// console prints when the server starts and `status` shows.
void applyAdvertising() {}
#else
bool gRestart = false;  // advertising was switched off: only a restart withdraws the name (see applyAdvertising)

// Keeps the mDNS records in step with the settings. MDNS.end() is never called: on this core it crashes the board
// while the station interface is up (found in the emulator). Records are added, updated in place or removed as
// services instead, and switching advertising off is finished by a restart, as the panel settings are.
void applyAdvertising() {
  if (!gCfg->adv_on) {
    if (gMdnsOn) gRestart = true;  // the name cannot be withdrawn without MDNS.end()
    return;
  }
  if (!gMdnsOn) {
    if (!MDNS.begin("skywatch")) return;
    gMdnsOn = true;
  }
  if (!gMdnsHttp) {
    MDNS.addService("http", "tcp", 80);
    gMdnsHttp = true;
  }
  if (gCfg->mcp_on && !gMdnsMcp) {  // DNS-SD: browse for _mcp._tcp (dns-sd -B _mcp._tcp); nothing secret in the record
    MDNS.addService("mcp", "tcp", 80);
    MDNS.addServiceTxt("mcp", "tcp", "path", "/mcp");
    MDNS.addServiceTxt("mcp", "tcp", "card", "/mcp/server-card");
    MDNS.addServiceTxt("mcp", "tcp", "auth", gCfg->mcp_auth ? "bearer" : "none");
    MDNS.addServiceTxt("mcp", "tcp", "version", gSnapshot().version);
    gMdnsMcp = true;
    gMdnsAuth = gCfg->mcp_auth;
  } else if (!gCfg->mcp_on && gMdnsMcp) {
    mdns_service_remove("_mcp", "_tcp");
    gMdnsMcp = false;
  } else if (gMdnsMcp && gMdnsAuth != gCfg->mcp_auth) {
    MDNS.addServiceTxt("mcp", "tcp", "auth", gCfg->mcp_auth ? "bearer" : "none");  // updates the item in place
    gMdnsAuth = gCfg->mcp_auth;
  }
}
#endif
}  // namespace

// Starts the web server on port 80, registers the routes and announces it on mDNS.
void webStart(Config* cfg, McpSnapshot snapshot, void (*on_saved)()) {
  if (gServer != nullptr || cfg == nullptr || snapshot == nullptr) return;  // nothing to serve without both
  gCfg = cfg;
  gSnapshot = snapshot;
  gFails = 0;
  gLocked = false;
  gServer = new WebServer(80);
  static const char* kHeaders[] = {"Authorization", "Origin", "X-API-Key"};
  const size_t kHeaderCount = 3;  // (exact types, so the ESP8266's array overload is chosen over its variadic one)
  gServer->collectHeaders(static_cast<const char**>(kHeaders), kHeaderCount);
  gServer->on("/", HTTP_GET, handleDashboard);
  gServer->on("/api/flights", HTTP_GET, handleFlights);
  gServer->on("/api/status", HTTP_GET, handleStatus);
#ifndef ESP8266
  gServer->on("/api/model", HTTP_GET, handleModel);
#endif
  gServer->on("/mcp", HTTP_ANY, handleMcp);
  gServer->on("/mcp/server-card", HTTP_GET, handleCard);
  gServer->on("/.well-known/ai-catalog.json", HTTP_GET, handleCatalog);
  settingsRegister(gServer, cfg, on_saved, pageGuard, false);
  gServer->onNotFound([] { gServer->send(404, "text/plain", "Not found\n"); });
  gServer->begin();
  applyAdvertising();
  webLogAddress();  // only with 'set debug on'
}

// Serves pending requests and keeps the mDNS records in step with the settings.
void webLogAddress() {
  if (gServer == nullptr || !gCfg->debug_log) return;
#ifdef ESP8266
  Serial.printf("web: http://%s/%s%s\n", WiFi.localIP().toString().c_str(),
#else
  Serial.printf("web: http://%s/ (also http://skywatch.local/)%s%s\n", WiFi.localIP().toString().c_str(),
#endif
                gCfg->web_on ? " web page on" : "", gCfg->mcp_on ? ", MCP at /mcp" : "");
}

void webLoop() {
  if (gServer == nullptr) return;
  applyAdvertising();  // 'set mcp off' and 'set mcpauth' take effect at once; 'set advertise off' restarts an ESP32
  gServer->handleClient();
}

// Stops the server and withdraws its mDNS services (the responder itself is never shut down: see applyAdvertising).
void webStop() {
  if (gServer == nullptr) return;
  gServer->stop();
  delete gServer;
  gServer = nullptr;
#ifndef ESP8266
  if (gMdnsMcp) mdns_service_remove("_mcp", "_tcp");
  if (gMdnsHttp) mdns_service_remove("_http", "_tcp");
  gMdnsMcp = gMdnsHttp = false;  // the responder itself keeps running (MDNS.end() is not safe here)
#endif
}

#ifdef ESP8266
bool webWantsRestart() { return false; }  // the ESP8266 responder can be closed live
#else
bool webWantsRestart() { return gRestart; }
#endif

bool webActive() { return gServer != nullptr; }

}  // namespace jf
