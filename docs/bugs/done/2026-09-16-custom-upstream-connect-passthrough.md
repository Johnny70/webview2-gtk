# Bug — later CUSTOM upstream never leaves pass-through

**Status:** ✅ fixed in **0.6.4**  
**Date:** 2026-09-16  
**Component:** `lib/webview2gtk/EmbeddedProxy.vala` +
`lib/host/win32-ui-webview2-events.c`  
**Related:** [6.0](../../plans/done/6.0-embedded-proxy.md);
[0.6.2 no `id@` CONNECT](./2026-09-16-localhost-proxy-first-connect-502.md)  
**Smoke:** `--smoke-proxy-late` (dummy, attach, then CUSTOM TEST-NET);
`--smoke-proxy-direct` still paints; `--smoke-proxy` still fail-closed

---

## Symptom

Dummy `CUSTOM` `http://127.0.0.1` starts the local host proxy. A later
`set_proxy_settings(CUSTOM, http://upstream:port)` was stored under
`route_id` 1, 2, … CONNECT had no `Proxy-Authorization`, lookup used id 0,
pass-through (DIRECT).

```
webview2gtk: proxy no auth CONNECT host.example:443 (pass-through)
webview2gtk: proxy CONNECT id=0 host.example:443 row=no
```

---

## Cause

Chromium **rejects** `--proxy-server` with userinfo
(`net/base/proxy_string_util.cc`). `http://id@127.0.0.1:port` is an invalid
proxy spec, not “proxy plus login”. After 0.6.2 dropped userinfo so CONNECT
works, Chromium still does not send `Proxy-Authorization` until a **407**.

---

## Fix

- `--proxy-server=http://127.0.0.1:<port>` unchanged.
- First CONNECT with no Basic user: **407** + `Proxy-Authenticate: Basic`.
- `ICoreWebView2_10` `BasicAuthenticationRequested` (attached **before**
  Navigate), only when `Uri` is `127.0.0.1:<port>`, puts `UserName` =
  `route_id`.
- Retry CONNECT has `Proxy-Authorization`; table lookup works.
- `NavigationCompleted` `ValidProxyAuthenticationRequired` is the handshake,
  not `load_failed`.

---

## Verify

`--smoke-proxy-direct` → Example Domain. `--smoke-proxy-late` → no Example
Domain (TEST-NET fail-closed after attach).
