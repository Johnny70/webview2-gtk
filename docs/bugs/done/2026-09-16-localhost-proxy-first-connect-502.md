# Bug — local host proxy never paints (grey screen)

**Status:** ✅ fixed in **0.6.2**  
**Date:** 2026-09-16  
**Component:** `lib/webview2gtk/EmbeddedProxy.vala` + `lib/host/win32-ui-webview2-automation.c`  
**Related:** [6.0](../../plans/done/6.0-embedded-proxy.md); consumer grey screen after dummy `CUSTOM` `http://127.0.0.1`  
**Smoke:** `examples/add-cookie --smoke-proxy-direct` (`TEST_PASS` on `snappr-win`)

---

## Design

Once the local host proxy is on, Chromium talks only to
`http://127.0.0.1:<port>` (no userinfo — Chromium does not CONNECT when the
URI is `http://id@127.0.0.1:port`). The table is **only** “extra upstream
for this id”, not “use the proxy / skip the proxy”.

| Table / request | After we accept |
|-----------------|-----------------|
| No `Proxy-Authorization` / no row / dummy `http://127.0.0.1` | Pass-through: connect to the destination |
| CUSTOM other host (row present) | Chain to that upstream |
| `about:` target | Answer 200, do not look up the table |

`SocketService.incoming` must return `true` or GLib closes the connection.

---

## Symptom

`--smoke-proxy-direct` (dummy CUSTOM before the first `WebView`, then
`https://example.com/`):

```
--proxy-server=http://1@127.0.0.1:51300
load_failed about:blank: Navigation failed
load_failed https://example.com/: Navigation failed
TEST_FAIL
```

---

## Cause

1. `incoming` returned `false` — GLib closed the accept immediately.
2. `--proxy-server=http://id@127.0.0.1:port` — Chromium never CONNECTed.
   `http://127.0.0.1:port` does CONNECT.
3. No `Proxy-Authorization` → we `407`’d. Missing auth must pass through.

---

## Fix

- `incoming` returns `true`.
- AdditionalBrowserArguments: `--proxy-server=http://127.0.0.1:<port>`.
- No auth / no row → pass-through. `about:` → 200, no table lookup.

---

## Verify

On `snappr-win`: `--smoke-proxy-direct` → `TEST_PASS` (Example Domain).
