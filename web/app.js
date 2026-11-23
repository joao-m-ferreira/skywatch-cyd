// The portal: pick a board and install it (ESP Web Tools), then send the WiFi name and a few settings
// to the board over USB (Web Serial). No framework, no build step.
(async function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const el = (tag, attrs = {}, ...kids) => {
    const n = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
      if (k === "class") n.className = v; else if (k === "text") n.textContent = v; else n.setAttribute(k, v);
    }
    for (const kid of kids) n.append(kid);
    return n;
  };
  const get = async (url) => { const r = await fetch(url, { headers: { Accept: "application/json" } }); if (!r.ok) throw new Error(url); return r.json(); };
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  let cfg = { repo: "", counterUrl: "" };
  try { cfg = await get("config.json"); } catch (e) { /* defaults */ }
  if (cfg.repo) $("repo-link").href = `https://github.com/${cfg.repo}`;
  if (!("serial" in navigator)) $("unsupported").hidden = false;

  // ------------------------------------------------------------------ 1. install
  const CHIP = { verified: "Tested on a real board", untested: "Should work, not tested yet", experimental: "Experimental", simulator: "Emulator only" };
  const WARN = {
    untested: "We haven't been able to try this one on a real board yet. It should work, but the colours might look off.",
    experimental: "Experimental: it uses the usual settings for this kind of screen, but nobody has confirmed them on a real one yet.",
    simulator: "This one has only run in an emulator so far, never on the real board. Install it only if you are comfortable recovering a board.",
  };
  const counted = new Set();
  function countFlash(board, version, mode) {
    const key = `${board}:${version}:${mode}`;
    if (!cfg.counterUrl || counted.has(key)) return;  // once per page load per board
    counted.add(key);
    fetch(`${cfg.counterUrl}/flash`, { method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ board, version, mode }) }).catch(() => {});
  }
  function installButton(manifest, label, asLink, board, version, mode) {
    const btn = el("button", { class: asLink ? "link" : "btn", slot: "activate", type: "button", text: label });
    const wrap = el("esp-web-install-button", { manifest }, btn,
      el("span", { slot: "unsupported", text: "Use Chrome or Edge on a computer." }),
      el("span", { slot: "not-allowed", text: "This needs a secure (https) page." }));
    wrap.addEventListener("state-changed", (ev) => {
      if (ev.detail && ev.detail.state === "finished") {
        countFlash(board, version, mode);
        status("Installed. Now connect it to your WiFi below.", "ok");
        $("setup").scrollIntoView({ behavior: "smooth" });
      }
    });
    return wrap;
  }
  try {
    const data = await get("boards.json");
    const tabs = $("board-tabs"), host = $("board-list");
    const byId = Object.fromEntries(data.boards.map((b) => [b.id, b]));
    function card(b) {
      const c = el("article", { class: "card", id: b.id });
      c.append(el("img", { class: "board-img", src: `img/board-${b.id}.svg`, alt: b.title || b.short }),
        el("h3", { text: b.title || b.short }),
        el("p", { class: "tag", text: b.tagline || "" }),
        el("span", { class: "chip " + b.status, text: CHIP[b.status] }));
      const actions = el("div", { class: "actions" });
      actions.append(
        installButton(`manifests/${b.id}.json`, "Install", false, b.id, b.version, "install"),
        installButton(`manifests/${b.id}-update.json`, "Update (keep my settings)", true, b.id, b.version, "update"),
        el("a", { class: "btn ghost", href: `firmware/${b.version}/skywatch-cyd-${b.version}-${b.id}-merged.bin`, download: "", text: "Download firmware" }));
      if (b.status !== "verified") {
        actions.hidden = true;
        const box = el("input", { type: "checkbox", id: `ack-${b.id}` });
        box.addEventListener("change", () => { actions.hidden = !box.checked; });
        c.append(el("p", { class: "warn", text: WARN[b.status] }),
          el("label", { class: "ack", for: `ack-${b.id}` }, box, el("span", { text: "I understand and want to install it anyway." })));
      }
      c.append(actions);
      return c;
    }
    function show(id, remember) {
      const b = byId[id] || data.boards[0];
      for (const t of tabs.children) t.setAttribute("aria-pressed", String(t.dataset.id === b.id));
      host.replaceChildren(card(b));  // only one board at a time
      if (remember) history.replaceState(null, "", "#" + b.id);
    }
    for (const b of data.boards) {
      const t = el("button", { type: "button", class: "tab", "data-id": b.id, "aria-pressed": "false" },
        el("span", { class: "dot " + b.status }), b.tab || b.short);
      t.addEventListener("click", () => show(b.id, true));
      tabs.append(t);
    }
    const first = data.boards.find((b) => b.status === "verified") || data.boards[0];
    show(location.hash.slice(1) || first.id, false);
  } catch (e) {
    $("board-list").replaceChildren(el("p", { class: "notice", text: "Could not load the list of boards. Please reload the page." }));
  }

  // ------------------------------------------------------------------ 2. WiFi and settings over USB
  function status(text, kind) {
    const s = $("setup-status");
    s.textContent = text;
    s.className = kind || "";
  }
  const noControl = (v) => !/[\u0000-\u001f\u007f]/.test(v);  // no line breaks or control characters in anything we send

  // Turns the form into the board's serial commands; throws a readable Error when something is invalid.
  function buildCommands() {
    const ssid = $("ssid").value, pass = $("pass").value;
    if (!ssid || ssid.length > 32 || !noControl(ssid)) throw new Error("The WiFi name must be 1 to 32 characters.");
    if (!noControl(pass) || pass.length > 64 || (pass && pass.length < 8)) throw new Error("A WiFi password has 8 to 64 characters. Leave it empty for an open network.");
    const cmds = [["set ssid " + ssid, "WiFi name"], ["set pass " + (pass || "-"), "WiFi password"]];
    if ($("where").value === "manual") {
      const lat = Number($("lat").value), lon = Number($("lon").value);
      if (!$("lat").value || !$("lon").value || !(lat >= -90 && lat <= 90) || !(lon >= -180 && lon <= 180)) throw new Error("Enter your latitude (-90 to 90) and longitude (-180 to 180).");
      cmds.push([`set location static ${lat} ${lon}`, "Location"]);
    } else {
      cmds.push(["set location auto", "Location"]);
    }
    const radius = Number($("radius").value);
    if (!Number.isInteger(radius) || radius < 1 || radius > 250) throw new Error("How far to look must be a whole number from 1 to 250 km.");
    cmds.push([`set radius ${radius}`, "Search distance"]);
    if ($("night").checked) {
      const from = Number($("night-from").value), to = Number($("night-to").value);
      if (![from, to].every((h) => Number.isInteger(h) && h >= 0 && h <= 23)) throw new Error("Night hours are whole numbers from 0 to 23.");
      cmds.push([`set night ${from} ${to}`, "Night mode"]);
    } else {
      cmds.push(["set night off", "Night mode"]);
    }
    // Firmware older than 0.5.0 does not know these three settings and refuses them: the third item marks them optional.
    cmds.push([`set web ${$("web-on").checked ? "on" : "off"}`, "Web page", true]);
    cmds.push([`set mcp ${$("mcp-on").checked ? "on" : "off"}`, "MCP server", true]);
    cmds.push([`set advertise ${$("adv-on").checked ? "on" : "off"}`, "Network announcement", true]);
    return cmds;
  }

  // A tiny serial session: opens the port, collects what the board prints, sends one line and waits for its answer.
  async function session() {
    const port = await navigator.serial.requestPort();  // needs the click that submitted the form
    await port.open({ baudRate: 115200 });
    try { await port.setSignals({ dataTerminalReady: false, requestToSend: false }); } catch (e) { /* not all adapters */ }
    const writer = port.writable.getWriter();
    const reader = port.readable.getReader();
    const dec = new TextDecoder();
    let inbox = "", alive = true;
    (async () => {
      try { while (alive) { const { value, done } = await reader.read(); if (done) break; inbox += dec.decode(value); } } catch (e) { /* closed */ }
    })();
    return {
      async send(line, ms = 4000, expect = /^(ok|error)[^\n]*/m) {
        inbox = "";
        await writer.write(new TextEncoder().encode(line + "\n"));
        const t0 = Date.now();
        while (Date.now() - t0 < ms) {
          const m = inbox.match(expect);
          if (m) return m[0];
          await sleep(50);
        }
        throw new Error("timeout");
      },
      async close() {
        alive = false;
        try { await reader.cancel(); } catch (e) { /* ignore */ }
        try { reader.releaseLock(); writer.releaseLock(); await port.close(); } catch (e) { /* ignore */ }
      },
    };
  }

  $("where").addEventListener("change", () => { $("coords").hidden = $("where").value !== "manual"; });
  $("setup-form").addEventListener("submit", async (ev) => {
    ev.preventDefault();
    if (!("serial" in navigator)) { status("This needs Chrome or Edge on a computer.", "error"); return; }
    let cmds;
    try { cmds = buildCommands(); } catch (e) { status(e.message, "error"); return; }
    const btn = $("send");
    btn.disabled = true;
    status("Choose your board in the window that opens...");
    let s;
    try {
      s = await session();
    } catch (e) {
      status(e && e.name === "NotFoundError" ? "No board was chosen." : "Could not open the board. Is another program (a serial monitor) using it?", "error");
      btn.disabled = false;
      return;
    }
    try {
      await sleep(600);
      const done = [];
      let newSettings = true;  // false once an older firmware refused one of the 0.5.0 settings
      for (const [cmd, label, optional] of cmds) {
        status(`${done.join("\n")}${done.length ? "\n" : ""}Sending: ${label}...`);
        const reply = await s.send(cmd);
        if (!reply.startsWith("ok") && optional && /unknown setting/.test(reply)) { newSettings = false; continue; }
        if (!reply.startsWith("ok")) throw new Error(`${label}: ${reply.replace(/^error:\s*/, "")}`);
        done.push(`✓ ${label}`);
        if (cmd.startsWith("set ssid") || cmd.startsWith("set pass")) await sleep(300);
      }
      let extra = "";
      if (newSettings && ($("web-on").checked || $("mcp-on").checked)) {  // the board made an access token: show it once, here
        let token = "";
        try { token = (await s.send("token", 4000, /^access token: \S+/m)).slice("access token: ".length); } catch (e) { /* none yet */ }
        const host = $("adv-on").checked ? "skywatch.local" : "<the board's IP address>";
        if (token) {
          extra = `\n\nAccess token (keep it private): ${token}\nOn your network: http://${host}/` +
            ($("mcp-on").checked ? `\nMCP endpoint: http://${host}/mcp  with the header  Authorization: Bearer ${token}` : "");
        }
      }
      status(`${done.join("\n")}\n\nAll set. The board is connecting to your WiFi; the first plane should appear within a minute.${extra}`, "ok");
    } catch (e) {
      status(e.message === "timeout"
        ? "The board didn't answer. Make sure the skywatch-cyd firmware is installed, then press the board's reset button and try again."
        : e.message, "error");
    } finally {
      await s.close();
      btn.disabled = false;
    }
  });

  // ------------------------------------------------------------------ community numbers (shown only when known)
  const stats = {};
  function showStats() {
    const parts = [["Forks", stats.forks], ["Stars", stats.stars], ["Boards flashed from this page", stats.flashes]].filter(([, v]) => typeof v === "number");
    const box = $("community");
    if (!parts.length) { box.hidden = true; return; }
    box.replaceChildren(...parts.map(([k, v]) => el("span", {}, `${k} `, el("b", { text: v.toLocaleString() }))));
    box.hidden = false;
  }
  if (cfg.repo) get(`https://api.github.com/repos/${cfg.repo}`).then((r) => { stats.forks = r.forks_count; stats.stars = r.stargazers_count; showStats(); }).catch(() => {});
  if (cfg.counterUrl) get(`${cfg.counterUrl}/stats`).then((s) => { stats.flashes = s.total; showStats(); }).catch(() => {});
})();
