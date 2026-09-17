/* Per-instance WebView2 navigation/title events (plan 4.3).
 * Hand-maintained (was generate-binding); tokens and callbacks live on WebView2Host. */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "win32-ui-webview2-com-glue.h"
#include "win32-ui-webview2-events.h"
#include "win32-ui-webview2-host-priv.h"
#include "win32-ui-webview2-proxy.h"
#include "win32-ui-webview2-sdk.h"
#include <wchar.h>

typedef struct WebView2EventHandler0 {
	ICoreWebView2NavigationCompletedEventHandler iface;
	ICoreWebView2NavigationCompletedEventHandlerVtbl vtbl;
	LONG ref_count;
	WebView2Host *host;
} WebView2EventHandler0;

typedef struct WebView2EventHandler1 {
	ICoreWebView2NavigationStartingEventHandler iface;
	ICoreWebView2NavigationStartingEventHandlerVtbl vtbl;
	LONG ref_count;
	WebView2Host *host;
} WebView2EventHandler1;

typedef struct WebView2EventHandler2 {
	ICoreWebView2DocumentTitleChangedEventHandler iface;
	ICoreWebView2DocumentTitleChangedEventHandlerVtbl vtbl;
	LONG ref_count;
	WebView2Host *host;
} WebView2EventHandler2;

static HRESULT STDMETHODCALLTYPE event_handler0_qi (
	ICoreWebView2NavigationCompletedEventHandler *This,
	REFIID riid,
	void **ppv)
{
	if (IsEqualIID (riid, &IID_IUnknown)
	    || IsEqualIID (riid, &IID_ICoreWebView2NavigationCompletedEventHandler)) {
		*ppv = This;
		ICoreWebView2NavigationCompletedEventHandler_AddRef (This);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE event_handler0_addref (
	ICoreWebView2NavigationCompletedEventHandler *This)
{
	WebView2EventHandler0 *self = (WebView2EventHandler0 *) This;
	return (ULONG) InterlockedIncrement (&self->ref_count);
}

static ULONG STDMETHODCALLTYPE event_handler0_release (
	ICoreWebView2NavigationCompletedEventHandler *This)
{
	WebView2EventHandler0 *self = (WebView2EventHandler0 *) This;
	LONG count = InterlockedDecrement (&self->ref_count);
	if (count == 0) {
		CoTaskMemFree (self);
	}
	return (ULONG) count;
}

static HRESULT STDMETHODCALLTYPE event_handler0_invoke (
	ICoreWebView2NavigationCompletedEventHandler *This,
	ICoreWebView2 *sender,
	ICoreWebView2NavigationCompletedEventArgs *args)
{
	WebView2EventHandler0 *self = (WebView2EventHandler0 *) This;
	WebView2Host *host = self->host;
	BOOL success = FALSE;

	(void) sender;
	if (args != NULL) {
		ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess (args, &success);
		{
			COREWEBVIEW2_WEB_ERROR_STATUS err = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
			ICoreWebView2NavigationCompletedEventArgs_get_WebErrorStatus (args, &err);
			if (err == COREWEBVIEW2_WEB_ERROR_STATUS_VALID_PROXY_AUTHENTICATION_REQUIRED) {
				/* 407 handshake; Chromium retries with Proxy-Authorization. */
				return S_OK;
			}
		}
	}
	if (host != NULL && host->cb_nav_completed != NULL) {
		host->cb_nav_completed (host->event_user_data, success ? true : false);
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE event_handler1_qi (
	ICoreWebView2NavigationStartingEventHandler *This,
	REFIID riid,
	void **ppv)
{
	if (IsEqualIID (riid, &IID_IUnknown)
	    || IsEqualIID (riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
		*ppv = This;
		ICoreWebView2NavigationStartingEventHandler_AddRef (This);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE event_handler1_addref (
	ICoreWebView2NavigationStartingEventHandler *This)
{
	WebView2EventHandler1 *self = (WebView2EventHandler1 *) This;
	return (ULONG) InterlockedIncrement (&self->ref_count);
}

static ULONG STDMETHODCALLTYPE event_handler1_release (
	ICoreWebView2NavigationStartingEventHandler *This)
{
	WebView2EventHandler1 *self = (WebView2EventHandler1 *) This;
	LONG count = InterlockedDecrement (&self->ref_count);
	if (count == 0) {
		CoTaskMemFree (self);
	}
	return (ULONG) count;
}

/* Security-relevant: this is the only point that can stop a navigation
 * before it happens (a message body's own decide-policy handler relies on
 * this to block anything but a user-clicked link). Answered synchronously,
 * inside this call, before returning to WebView2 -- no GetDeferral, since
 * the app's decide_policy handler is itself synchronous. */
static HRESULT STDMETHODCALLTYPE event_handler1_invoke (
	ICoreWebView2NavigationStartingEventHandler *This,
	ICoreWebView2 *sender,
	ICoreWebView2NavigationStartingEventArgs *args)
{
	WebView2EventHandler1 *self = (WebView2EventHandler1 *) This;
	WebView2Host *host = self->host;

	(void) sender;
	if (host != NULL && args != NULL && host->cb_nav_decide != NULL) {
		LPWSTR uri = NULL;
		BOOL is_user_initiated = FALSE;
		if (SUCCEEDED (ICoreWebView2NavigationStartingEventArgs_get_Uri (args, &uri)) && uri != NULL) {
			/* wide_length1 is an element count (Vala uint16[] length), not a
			 * WideCharToMultiByte-style "-1 means null-terminated" sentinel --
			 * passing -1 would make the conversion return an empty string. */
			char *uri_utf8 = win32_ui_utf16_to_utf8 ((uint16_t *) uri, (int) wcslen (uri) + 1);
			ICoreWebView2NavigationStartingEventArgs_get_IsUserInitiated (args, &is_user_initiated);
			if (uri_utf8 != NULL) {
				int cancel = 0;
				if (host->cb_nav_decide (
					    uri_utf8, is_user_initiated ? 1 : 0, &cancel, host->nav_decide_ctx)
				    && cancel) {
					ICoreWebView2NavigationStartingEventArgs_put_Cancel (args, TRUE);
				}
				free (uri_utf8);
			}
			CoTaskMemFree (uri);
		}
	}
	if (host != NULL && host->cb_nav_starting != NULL) {
		host->cb_nav_starting (host->event_user_data);
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE event_handler2_qi (
	ICoreWebView2DocumentTitleChangedEventHandler *This,
	REFIID riid,
	void **ppv)
{
	if (IsEqualIID (riid, &IID_IUnknown)
	    || IsEqualIID (riid, &IID_ICoreWebView2DocumentTitleChangedEventHandler)) {
		*ppv = This;
		ICoreWebView2DocumentTitleChangedEventHandler_AddRef (This);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE event_handler2_addref (
	ICoreWebView2DocumentTitleChangedEventHandler *This)
{
	WebView2EventHandler2 *self = (WebView2EventHandler2 *) This;
	return (ULONG) InterlockedIncrement (&self->ref_count);
}

static ULONG STDMETHODCALLTYPE event_handler2_release (
	ICoreWebView2DocumentTitleChangedEventHandler *This)
{
	WebView2EventHandler2 *self = (WebView2EventHandler2 *) This;
	LONG count = InterlockedDecrement (&self->ref_count);
	if (count == 0) {
		CoTaskMemFree (self);
	}
	return (ULONG) count;
}

static HRESULT STDMETHODCALLTYPE event_handler2_invoke (
	ICoreWebView2DocumentTitleChangedEventHandler *This,
	ICoreWebView2 *sender,
	IUnknown *args)
{
	WebView2EventHandler2 *self = (WebView2EventHandler2 *) This;
	WebView2Host *host = self->host;

	(void) sender;
	(void) args;
	if (host != NULL && host->cb_title_changed != NULL) {
		host->cb_title_changed (host->event_user_data);
	}
	return S_OK;
}

typedef struct WebView2ProxyAuthHandler {
	ICoreWebView2BasicAuthenticationRequestedEventHandler iface;
	ICoreWebView2BasicAuthenticationRequestedEventHandlerVtbl vtbl;
	LONG ref_count;
	WebView2Host *host;
} WebView2ProxyAuthHandler;

static BOOL
uri_is_local_host_proxy (LPCWSTR uri, unsigned port)
{
	wchar_t needle[32];
	const wchar_t *found;
	size_t nlen;

	if (uri == NULL || port == 0 || wcsstr (uri, L"127.0.0.1") == NULL) {
		return FALSE;
	}
	_snwprintf (needle, 32, L":%u", port);
	needle[31] = L'\0';
	found = wcsstr (uri, needle);
	if (found == NULL) {
		return FALSE;
	}
	nlen = wcslen (needle);
	return found[nlen] == L'\0' || found[nlen] == L'/' || found[nlen] == L':';
}

static HRESULT STDMETHODCALLTYPE proxy_auth_qi (
	ICoreWebView2BasicAuthenticationRequestedEventHandler *This,
	REFIID riid,
	void **ppv)
{
	if (IsEqualIID (riid, &IID_IUnknown)
	    || IsEqualIID (riid, &IID_ICoreWebView2BasicAuthenticationRequestedEventHandler)) {
		*ppv = This;
		ICoreWebView2BasicAuthenticationRequestedEventHandler_AddRef (This);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE proxy_auth_addref (
	ICoreWebView2BasicAuthenticationRequestedEventHandler *This)
{
	WebView2ProxyAuthHandler *self = (WebView2ProxyAuthHandler *) This;
	return (ULONG) InterlockedIncrement (&self->ref_count);
}

static ULONG STDMETHODCALLTYPE proxy_auth_release (
	ICoreWebView2BasicAuthenticationRequestedEventHandler *This)
{
	WebView2ProxyAuthHandler *self = (WebView2ProxyAuthHandler *) This;
	LONG count = InterlockedDecrement (&self->ref_count);
	if (count == 0) {
		CoTaskMemFree (self);
	}
	return (ULONG) count;
}

static HRESULT STDMETHODCALLTYPE proxy_auth_invoke (
	ICoreWebView2BasicAuthenticationRequestedEventHandler *This,
	ICoreWebView2 *sender,
	ICoreWebView2BasicAuthenticationRequestedEventArgs *args)
{
	WebView2ProxyAuthHandler *self = (WebView2ProxyAuthHandler *) This;
	WebView2Host *host = self->host;
	ICoreWebView2BasicAuthenticationResponse *response = NULL;
	LPWSTR uri = NULL;
	wchar_t user[16];
	unsigned port;

	(void) sender;
	if (host == NULL || args == NULL || host->route_id <= 0) {
		return S_OK;
	}
	port = vala_webview2_host_embedded_proxy_port ();
	if (FAILED (ICoreWebView2BasicAuthenticationRequestedEventArgs_get_Uri (args, &uri))
	    || uri == NULL) {
		return S_OK;
	}
	if (!uri_is_local_host_proxy (uri, port)) {
		CoTaskMemFree (uri);
		return S_OK;
	}
	if (FAILED (ICoreWebView2BasicAuthenticationRequestedEventArgs_get_Response (
		    args, &response))
	    || response == NULL) {
		CoTaskMemFree (uri);
		return S_OK;
	}
	_snwprintf (user, 16, L"%d", host->route_id);
	user[15] = L'\0';
	ICoreWebView2BasicAuthenticationResponse_put_UserName (response, user);
	ICoreWebView2BasicAuthenticationResponse_put_Password (response, L"");
	fprintf (
		stderr,
		"webview2gtk: local host proxy BasicAuthenticationRequested id=%d uri=%ls\n",
		host->route_id,
		uri
	);
	ICoreWebView2BasicAuthenticationResponse_Release (response);
	CoTaskMemFree (uri);
	return S_OK;
}

void
vala_webview2_events_register_host (WebView2Host *host)
{
	ICoreWebView2 *webview;
	HRESULT hr;

	if (host == NULL || host->webview == NULL || host->events_registered) {
		return;
	}
	webview = host->webview;

	{
		WebView2EventHandler0 *handler = (WebView2EventHandler0 *) CoTaskMemAlloc (sizeof (WebView2EventHandler0));
		if (handler != NULL) {
			ZeroMemory (handler, sizeof (*handler));
			handler->iface.lpVtbl = &handler->vtbl;
			handler->vtbl.QueryInterface = event_handler0_qi;
			handler->vtbl.AddRef = event_handler0_addref;
			handler->vtbl.Release = event_handler0_release;
			handler->vtbl.Invoke = event_handler0_invoke;
			handler->ref_count = 1;
			handler->host = host;
			hr = ICoreWebView2_add_NavigationCompleted (webview, &handler->iface, &host->tok_nav_completed);
			if (FAILED (hr)) {
				fprintf (stderr, "WebView2 add_NavigationCompleted failed: 0x%08lx\n", (unsigned long) hr);
				ICoreWebView2NavigationCompletedEventHandler_Release (&handler->iface);
			} else {
				ICoreWebView2NavigationCompletedEventHandler_Release (&handler->iface);
			}
		}
	}

	{
		WebView2EventHandler1 *handler = (WebView2EventHandler1 *) CoTaskMemAlloc (sizeof (WebView2EventHandler1));
		if (handler != NULL) {
			ZeroMemory (handler, sizeof (*handler));
			handler->iface.lpVtbl = &handler->vtbl;
			handler->vtbl.QueryInterface = event_handler1_qi;
			handler->vtbl.AddRef = event_handler1_addref;
			handler->vtbl.Release = event_handler1_release;
			handler->vtbl.Invoke = event_handler1_invoke;
			handler->ref_count = 1;
			handler->host = host;
			hr = ICoreWebView2_add_NavigationStarting (webview, &handler->iface, &host->tok_nav_starting);
			if (FAILED (hr)) {
				fprintf (stderr, "WebView2 add_NavigationStarting failed: 0x%08lx\n", (unsigned long) hr);
				ICoreWebView2NavigationStartingEventHandler_Release (&handler->iface);
			} else {
				ICoreWebView2NavigationStartingEventHandler_Release (&handler->iface);
			}
		}
	}

	{
		WebView2EventHandler2 *handler = (WebView2EventHandler2 *) CoTaskMemAlloc (sizeof (WebView2EventHandler2));
		if (handler != NULL) {
			ZeroMemory (handler, sizeof (*handler));
			handler->iface.lpVtbl = &handler->vtbl;
			handler->vtbl.QueryInterface = event_handler2_qi;
			handler->vtbl.AddRef = event_handler2_addref;
			handler->vtbl.Release = event_handler2_release;
			handler->vtbl.Invoke = event_handler2_invoke;
			handler->ref_count = 1;
			handler->host = host;
			hr = ICoreWebView2_add_DocumentTitleChanged (webview, &handler->iface, &host->tok_title);
			if (FAILED (hr)) {
				fprintf (stderr, "WebView2 add_DocumentTitleChanged failed: 0x%08lx\n", (unsigned long) hr);
				ICoreWebView2DocumentTitleChangedEventHandler_Release (&handler->iface);
			} else {
				ICoreWebView2DocumentTitleChangedEventHandler_Release (&handler->iface);
			}
		}
	}

	{
		ICoreWebView2_10 *wv10 = NULL;

		hr = ICoreWebView2_QueryInterface (
			webview, &IID_ICoreWebView2_10, (void **) &wv10);
		if (SUCCEEDED (hr) && wv10 != NULL) {
			WebView2ProxyAuthHandler *handler = (WebView2ProxyAuthHandler *) CoTaskMemAlloc (
				sizeof (WebView2ProxyAuthHandler));
			if (handler != NULL) {
				ZeroMemory (handler, sizeof (*handler));
				handler->iface.lpVtbl = &handler->vtbl;
				handler->vtbl.QueryInterface = proxy_auth_qi;
				handler->vtbl.AddRef = proxy_auth_addref;
				handler->vtbl.Release = proxy_auth_release;
				handler->vtbl.Invoke = proxy_auth_invoke;
				handler->ref_count = 1;
				handler->host = host;
				hr = ICoreWebView2_10_add_BasicAuthenticationRequested (
					wv10, &handler->iface, &host->tok_proxy_auth);
				if (FAILED (hr)) {
					fprintf (
						stderr,
						"WebView2 add_BasicAuthenticationRequested failed: 0x%08lx\n",
						(unsigned long) hr
					);
					ICoreWebView2BasicAuthenticationRequestedEventHandler_Release (
						&handler->iface);
				} else {
					ICoreWebView2BasicAuthenticationRequestedEventHandler_Release (
						&handler->iface);
				}
			}
			ICoreWebView2_10_Release (wv10);
		}
	}

	host->events_registered = TRUE;
}

void
vala_webview2_events_unregister_host (WebView2Host *host)
{
	ICoreWebView2 *webview;

	if (host == NULL || !host->events_registered || host->webview == NULL) {
		return;
	}
	webview = host->webview;
	ICoreWebView2_remove_NavigationCompleted (webview, host->tok_nav_completed);
	ICoreWebView2_remove_NavigationStarting (webview, host->tok_nav_starting);
	ICoreWebView2_remove_DocumentTitleChanged (webview, host->tok_title);
	{
		ICoreWebView2_10 *wv10 = NULL;

		if (SUCCEEDED (ICoreWebView2_QueryInterface (
			    webview, &IID_ICoreWebView2_10, (void **) &wv10))
		    && wv10 != NULL) {
			ICoreWebView2_10_remove_BasicAuthenticationRequested (
				wv10, host->tok_proxy_auth);
			ICoreWebView2_10_Release (wv10);
		}
	}
	host->events_registered = FALSE;
}

/* Legacy entry points used by Vala finish_setup until callers pass host. */
void
vala_webview2_events_register (ICoreWebView2 *webview)
{
	(void) webview;
	fprintf (stderr, "WebView2: events_register(webview) is obsolete; use events_register_host\n");
}

void
vala_webview2_events_unregister (ICoreWebView2 *webview)
{
	(void) webview;
}
