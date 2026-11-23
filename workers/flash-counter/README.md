# Flash counter

A small Cloudflare Worker that counts flashes made from the web portal (`web/`). The portal works
without it: if `counterUrl` in `web/config.json` is empty it shows "not counted".

Deploy (needs a free Cloudflare account):

```sh
cd workers/flash-counter
npx wrangler kv namespace create COUNTS        # paste the id into wrangler.toml
npx wrangler secret put SALT                   # any random string
npx wrangler deploy                            # prints the worker URL
```

Then set `"counterUrl"` in `web/config.json` to that URL (no trailing slash) and make sure
`ALLOWED_ORIGIN` in `wrangler.toml` is the address of your portal.

Privacy: it keeps one number per board and, for 60 seconds, a salted hash of the caller so that a
double click is not counted twice. Test: `node --test workers/flash-counter/test.mjs`.
