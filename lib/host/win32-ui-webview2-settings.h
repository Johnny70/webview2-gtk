/* WebViewSettings properties backed by a real ICoreWebView2Settings COM
 * property (plan 2d). Properties with no WebView2 equivalent stay Vala-only
 * stored values in WebViewSettings itself and never reach this file. */

#ifndef WIN32_UI_WEBVIEW2_SETTINGS_H
#define WIN32_UI_WEBVIEW2_SETTINGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct WebView2Host;

bool vala_webview2_host_set_enable_back_forward_navigation_gestures (
	struct WebView2Host *host, bool enabled);
bool vala_webview2_host_get_enable_back_forward_navigation_gestures (
	struct WebView2Host *host);

/* Called once from finish_setup, after ICoreWebView2 exists, to push
 * whatever was set (or left at its default) before the host was ready. */
void vala_webview2_settings_register_host (struct WebView2Host *host);

#ifdef __cplusplus
}
#endif

#endif /* WIN32_UI_WEBVIEW2_SETTINGS_H */
