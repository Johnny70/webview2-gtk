/* WebView background color, backed by ICoreWebView2Controller2's real
 * DefaultBackgroundColor COM property (WebKitGTK-shaped: mirrors
 * WebKit.WebView.set_background_color). Without this, a freshly created
 * WebView2 control shows its own opaque-white default until the first
 * paint completes -- visible as a flash/flicker every time a widget tree
 * that recreates its WebView on selection (see postcard's MessageView)
 * swaps one in. */

#ifndef WIN32_UI_WEBVIEW2_BACKGROUND_H
#define WIN32_UI_WEBVIEW2_BACKGROUND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct WebView2Host;

bool vala_webview2_host_set_background_color (
	struct WebView2Host *host, uint8_t a, uint8_t r, uint8_t g, uint8_t b);

/* Called once from finish_setup, before the controller becomes visible, so
 * a color set before attach is applied before the first frame ever paints. */
void vala_webview2_background_register_host (struct WebView2Host *host);

#ifdef __cplusplus
}
#endif

#endif /* WIN32_UI_WEBVIEW2_BACKGROUND_H */
