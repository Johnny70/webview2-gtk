# Bug — `NetworkSession` ctor is not WebKit-shaped `(string?, string?)`

**Status:** ✅ fixed in **0.6.1**  
**Date:** 2026-09-15  
**Component:** `lib/webview2gtk/NetworkSession.vala`  
**API parity:** WebKitGTK `WebKit.NetworkSession.new (string? data_directory, string? cache_directory)`  
**Related:** [6.0 embedded proxy](../../plans/done/6.0-embedded-proxy.md) (first `set_proxy_settings(CUSTOM)` before any WebView)

---

## Symptom

Shared apps call the WebKit-shaped bootstrap **before** the first `WebView`
exists (required so the first `CUSTOM` can start the local host proxy):

```vala
var session = new NetworkSession(null, null);
session.set_proxy_settings(
	NetworkProxyMode.CUSTOM,
	new NetworkProxySettings("http://127.0.0.1", null)
);
/* then construct WebViews / present */
```

On **WebKitGTK** this compiles (`NetworkSession` takes two nullable directory
args).

On **webview2-gtk** the Vala ctor is parameterless:

```vala
public NetworkSession() {
```

so `new NetworkSession(null, null)` fails to compile. Apps cannot use one
source line for Linux + Windows without `#if` around the constructor.

---

## Expected

Match WebKitGTK:

```vala
public NetworkSession(string? data_directory = null, string? cache_directory = null)
```

- Accept the two args for API parity.
- Ignore them on Windows if unused (WebView2 user-data is per view / host).
- Keep existing `new NetworkSession()` call sites working via defaults.

---

## Why it matters

Plan 6.0 / automation docs: first `CUSTOM` must run **before** any WebView is
shown. The natural shared pattern is a throwaway `NetworkSession` +
`set_proxy_settings`, then create views. Constructor mismatch forces
platform `#if` in every consumer that follows that pattern.

---

## Fix sketch

Replace the no-arg ctor with the two-arg WebKit-shaped signature (defaults
`null, null`). No behaviour change for current `new NetworkSession()` sites.

---

## Verify

1. `new NetworkSession(null, null)` compiles against webview2-gtk.
2. `new NetworkSession()` still compiles (defaults).
3. First `set_proxy_settings(CUSTOM, http://127.0.0.1)` on that session before
   any WebView still starts the local host proxy (`--smoke-proxy`).
