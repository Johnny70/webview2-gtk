/* WebView background color -- see win32-ui-webview2-background.h. */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "win32-ui-webview2-background.h"
#include "win32-ui-webview2-host-priv.h"
#include "win32-ui-webview2-sdk.h"

static void
apply_background_color_to_host (WebView2Host *host)
{
	ICoreWebView2Controller2 *controller2 = NULL;
	COREWEBVIEW2_COLOR color;

	if (host == NULL || host->controller == NULL || !host->bg_color_set) {
		return;
	}
	if (FAILED (ICoreWebView2Controller_QueryInterface (
	                host->controller, &IID_ICoreWebView2Controller2, (void **) &controller2))
	    || controller2 == NULL) {
		fprintf (stderr, "WebView2 ICoreWebView2Controller2 unavailable -- background color ignored\n");
		return;
	}
	color.A = host->bg_a;
	color.R = host->bg_r;
	color.G = host->bg_g;
	color.B = host->bg_b;
	ICoreWebView2Controller2_put_DefaultBackgroundColor (controller2, color);
	ICoreWebView2Controller2_Release (controller2);
}

bool
vala_webview2_host_set_background_color (
	WebView2Host *host, uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	if (host == NULL) {
		return false;
	}
	host->bg_color_set = TRUE;
	host->bg_a = a;
	host->bg_r = r;
	host->bg_g = g;
	host->bg_b = b;
	if (host->controller != NULL) {
		apply_background_color_to_host (host);
	}
	return true;
}

void
vala_webview2_background_register_host (WebView2Host *host)
{
	apply_background_color_to_host (host);
}
