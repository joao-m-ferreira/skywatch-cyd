// Flash counter for the skywatch-cyd web portal: a tiny Cloudflare Worker.
//
//   POST /flash   {"board": "cyd", "version": "0.3.0", "mode": "install"|"update"}  -> counts one flash
//   GET  /stats   -> {"total": 12, "boards": {"cyd": 9, "amoled18": 3}}
//
// It stores one number per board and a short-lived salted hash of the caller (60 s, to ignore
// double clicks); it never stores an address, a user agent or anything else about the visitor.
// KV is not atomic, so two flashes in the same instant can be counted once: fine for a hobby counter.

const BOARD = /^[a-z0-9][a-z0-9-]{0,40}$/;
const VERSION = /^[0-9A-Za-z][0-9A-Za-z.+-]{0,30}$/;

export function cors(env, origin) {
  const allowed = (env.ALLOWED_ORIGIN || "").split(",").map((s) => s.trim()).filter(Boolean);
  const ok = allowed.length === 0 || allowed.includes(origin);
  return {
    "Access-Control-Allow-Origin": ok && origin ? origin : allowed[0] || "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "content-type",
    "Access-Control-Max-Age": "86400",
    Vary: "Origin",
  };
}

const json = (obj, status, headers) =>
  new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json", ...headers } });

async function digest(text) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].slice(0, 8).map((b) => b.toString(16).padStart(2, "0")).join("");
}

export async function handle(request, env) {
  const url = new URL(request.url);
  const origin = request.headers.get("Origin") || "";
  const headers = cors(env, origin);
  const allowedOrigins = (env.ALLOWED_ORIGIN || "").split(",").map((s) => s.trim()).filter(Boolean);
  if (request.method === "OPTIONS") return new Response(null, { status: 204, headers });
  if (allowedOrigins.length && origin && !allowedOrigins.includes(origin)) {
    return json({ error: "origin not allowed" }, 403, headers);
  }
  const boards = (env.BOARDS || "").split(",").map((s) => s.trim()).filter(Boolean);

  if (url.pathname === "/stats" && request.method === "GET") {
    const out = { total: Number((await env.COUNTS.get("flash:total")) || 0), boards: {} };
    for (const id of boards) out.boards[id] = Number((await env.COUNTS.get(`flash:${id}`)) || 0);
    return json(out, 200, { ...headers, "cache-control": "public, max-age=60" });
  }

  if (url.pathname === "/flash" && request.method === "POST") {
    let body;
    try {
      body = await request.json();
    } catch {
      return json({ error: "invalid JSON" }, 400, headers);
    }
    const { board, version, mode } = body || {};
    if (typeof board !== "string" || !BOARD.test(board) || !boards.includes(board)) {
      return json({ error: "unknown board" }, 400, headers);
    }
    if (typeof version !== "string" || !VERSION.test(version) || (mode !== "install" && mode !== "update")) {
      return json({ error: "invalid version or mode" }, 400, headers);
    }
    const ip = request.headers.get("CF-Connecting-IP") || "unknown";
    const key = `rl:${board}:${await digest(ip + (env.SALT || ""))}`;
    if (await env.COUNTS.get(key)) return json({ counted: false, reason: "duplicate" }, 202, headers);
    await env.COUNTS.put(key, "1", { expirationTtl: 60 });
    const total = Number((await env.COUNTS.get("flash:total")) || 0) + 1;
    const one = Number((await env.COUNTS.get(`flash:${board}`)) || 0) + 1;
    await env.COUNTS.put("flash:total", String(total));
    await env.COUNTS.put(`flash:${board}`, String(one));
    return json({ counted: true, total }, 200, headers);
  }
  return json({ error: "not found" }, 404, headers);
}

export default { fetch: (request, env) => handle(request, env) };
