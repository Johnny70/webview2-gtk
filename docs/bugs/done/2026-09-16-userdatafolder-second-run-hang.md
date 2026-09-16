# Bug — second local host proxy launch hangs on load

**Status:** ✅ fixed in **0.6.3**  
**Date:** 2026-09-16  
**Component:** `lib/host/win32-ui-webview2-loader.c` + `lib/host/win32-ui-webview2-com-glue.c`  
**Related:** [6.0](../../plans/done/6.0-embedded-proxy.md); consumer: first run paints, every later run hangs  
**Smoke:** `examples/add-cookie --smoke-proxy-direct` twice in a row

---

## Symptom

First process with dummy `CUSTOM` `http://127.0.0.1` loads. Every later launch stays on a loading/grey view. No `CreateCoreWebView2EnvironmentWithOptions` error — the callback never runs.

---

## Cause

With the local host proxy on, each view’s env uses
`%LOCALAPPDATA%\webview2gtk\profiles\wv_<id>`. `route_id` starts at 1 every
process, so every launch reuses `wv_1`.

WebView2 takes an exclusive lock on that folder. If the previous
`msedgewebview2` is still alive (typical after a GUI kill, or Close racing
process exit), the next `CreateCoreWebView2EnvironmentWithOptions` waits
forever. `ICoreWebView2Controller.Close` was also called only after
`ICoreWebView2_Release`, which can leave the browser process holding the
folder.

---

## Fix

- Folder is `wv_<pid>_<tick>_<id>` so a leftover browser cannot pin the next
  process.
- On first create, sweep `wv_*` dirs whose owner pid is gone. Legacy
  `wv_<id>` is best-effort delete (lock → skip).
- Destroy: `Controller.Close`, then Release webview, controller, env.
