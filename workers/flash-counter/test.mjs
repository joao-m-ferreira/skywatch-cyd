// Run: node --test workers/flash-counter/
import test from "node:test";
import assert from "node:assert/strict";
import { handle } from "./src/index.js";

class FakeKV {
  constructor() { this.m = new Map(); }
  async get(k) { return this.m.has(k) ? this.m.get(k) : null; }
  async put(k, v) { this.m.set(k, v); }
}
const env = () => ({ COUNTS: new FakeKV(), ALLOWED_ORIGIN: "https://site.example", BOARDS: "cyd,amoled18", SALT: "s" });
const post = (body, headers = {}) => new Request("https://w.example/flash", {
  method: "POST", headers: { Origin: "https://site.example", "CF-Connecting-IP": "1.2.3.4", ...headers },
  body: typeof body === "string" ? body : JSON.stringify(body),
});

test("counts a flash and reports it in /stats", async () => {
  const e = env();
  const r = await handle(post({ board: "cyd", version: "0.3.0", mode: "install" }), e);
  assert.equal(r.status, 200);
  assert.deepEqual(await r.json(), { counted: true, total: 1 });
  const s = await (await handle(new Request("https://w.example/stats", { headers: { Origin: "https://site.example" } }), e)).json();
  assert.deepEqual(s, { total: 1, boards: { cyd: 1, amoled18: 0 } });
});

test("ignores a repeat from the same caller within a minute, but not another caller", async () => {
  const e = env();
  const body = { board: "cyd", version: "0.3.0", mode: "update" };
  assert.equal((await handle(post(body), e)).status, 200);
  const dup = await handle(post(body), e);
  assert.equal(dup.status, 202);
  assert.equal((await dup.json()).counted, false);
  assert.equal((await handle(post(body, { "CF-Connecting-IP": "5.6.7.8" }), e)).status, 200);
});

test("rejects unknown boards, bad versions, bad modes and bad JSON", async () => {
  const e = env();
  assert.equal((await handle(post({ board: "toaster", version: "1", mode: "install" }), e)).status, 400);
  assert.equal((await handle(post({ board: "cyd", version: "../../x", mode: "install" }), e)).status, 400);
  assert.equal((await handle(post({ board: "cyd", version: "1.0.0", mode: "erase" }), e)).status, 400);
  assert.equal((await handle(post("not json"), e)).status, 400);
});

test("refuses other origins, answers preflight, 404s the rest", async () => {
  const e = env();
  assert.equal((await handle(post({ board: "cyd", version: "1", mode: "install" }, { Origin: "https://evil.example" }), e)).status, 403);
  const pre = await handle(new Request("https://w.example/flash", { method: "OPTIONS", headers: { Origin: "https://site.example" } }), e);
  assert.equal(pre.status, 204);
  assert.equal(pre.headers.get("Access-Control-Allow-Origin"), "https://site.example");
  assert.equal((await handle(new Request("https://w.example/nope", { headers: { Origin: "https://site.example" } }), e)).status, 404);
});

test("stores nothing about the visitor except a salted, expiring hash", async () => {
  const e = env();
  await handle(post({ board: "cyd", version: "0.3.0", mode: "install" }), e);
  const keys = [...e.COUNTS.m.keys()];
  assert.ok(keys.every((k) => k.startsWith("flash:") || /^rl:cyd:[0-9a-f]{16}$/.test(k)), keys.join());
  assert.ok(![...e.COUNTS.m.values()].some((v) => v.includes("1.2.3.4")));
});
