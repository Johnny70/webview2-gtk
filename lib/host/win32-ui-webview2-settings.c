/* WebViewSettings properties backed by a real ICoreWebView2Settings COM
 * property (plan 2d). Only enable_back_forward_navigation_gestures maps to
 * anything WebView2 actually exposes (ICoreWebView2Settings6::
 * IsSwipeNavigationEnabled) -- checked against the vendored WebView2.h
 * across every ICoreWebView2Settings/…N version, not assumed. */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "win32-ui-webview2-settings.h"
#include "win32-ui-webview2-host-priv.h"
#include "win32-ui-webview2-sdk.h"

static void
apply_swipe_navigation_to_host (WebView2Host *host)
{
	ICoreWebView2Settings *settings = NULL;
	ICoreWebView2Settings6 *settings6 = NULL;

	if (host == NULL || host->webview == NULL) {
		return;
	}
	if (FAILED (ICoreWebView2_get_Settings (host->webview, &settings)) || settings == NULL) {
		fprintf (stderr, "WebView2 get_Settings failed -- swipe navigation ignored\n");
		return;
	}
	if (FAILED (ICoreWebView2Settings_QueryInterface (settings, &IID_ICoreWebView2Settings6,
	                                                  (void **) &settings6))
	    || settings6 == NULL) {
		fprintf (stderr, "WebView2 ICoreWebView2Settings6 unavailable -- swipe navigation ignored\n");
		ICoreWebView2Settings_Release (settings);
		return;
	}
	ICoreWebView2Settings6_put_IsSwipeNavigationEnabled (
		settings6, host->enable_back_forward_navigation_gestures);
	ICoreWebView2Settings6_Release (settings6);
	ICoreWebView2Settings_Release (settings);
}

bool
vala_webview2_host_set_enable_back_forward_navigation_gestures (
	WebView2Host *host, bool enabled)
{
	if (host == NULL) {
		return false;
	}
	host->enable_back_forward_navigation_gestures = enabled ? TRUE : FALSE;
	if (host->webview != NULL) {
		apply_swipe_navigation_to_host (host);
	}
	return true;
}

bool
vala_webview2_host_get_enable_back_forward_navigation_gestures (WebView2Host *host)
{
	ICoreWebView2Settings *settings = NULL;
	ICoreWebView2Settings6 *settings6 = NULL;
	BOOL value = FALSE;

	if (host == NULL) {
		return false;
	}
	if (host->webview == NULL) {
		return host->enable_back_forward_navigation_gestures ? true : false;
	}
	if (FAILED (ICoreWebView2_get_Settings (host->webview, &settings)) || settings == NULL) {
		return host->enable_back_forward_navigation_gestures ? true : false;
	}
	if (FAILED (ICoreWebView2Settings_QueryInterface (settings, &IID_ICoreWebView2Settings6,
	                                                  (void **) &settings6))
	    || settings6 == NULL) {
		ICoreWebView2Settings_Release (settings);
		return host->enable_back_forward_navigation_gestures ? true : false;
	}
	if (SUCCEEDED (ICoreWebView2Settings6_get_IsSwipeNavigationEnabled (settings6, &value))) {
		host->enable_back_forward_navigation_gestures = value ? TRUE : FALSE;
	}
	ICoreWebView2Settings6_Release (settings6);
	ICoreWebView2Settings_Release (settings);
	return host->enable_back_forward_navigation_gestures ? true : false;
}

void
vala_webview2_settings_register_host (WebView2Host *host)
{
	apply_swipe_navigation_to_host (host);
}
