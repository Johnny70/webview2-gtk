/* WebView2Loader.dll bootstrap only (Phase 7i). */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <wchar.h>

#include "win32-ui-webview2-loader.h"
#include "win32-ui-webview2-automation.h"
#include "win32-ui-webview2-sdk.h"
#include <shlobj.h>

typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateCoreWebView2EnvironmentWithOptions)(
	PCWSTR browserExecutableFolder,
	PCWSTR userDataFolder,
	ICoreWebView2EnvironmentOptions *environmentOptions,
	ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler);

static HMODULE g_loader_module;
static PFN_CreateCoreWebView2EnvironmentWithOptions g_create_env_with_options;
static BOOL g_com_inited;

BOOL vala_webview2_loader_init (void)
{
	HRESULT hr;

	if (!g_com_inited) {
		hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
		if (FAILED (hr) && hr != RPC_E_CHANGED_MODE) {
			fprintf (stderr, "CoInitializeEx failed: 0x%08lx\n", (unsigned long) hr);
			return FALSE;
		}
		g_com_inited = TRUE;
	}

	if (g_create_env_with_options != NULL) {
		return TRUE;
	}

	g_loader_module = LoadLibraryW (L"WebView2Loader.dll");
	if (g_loader_module == NULL) {
		fprintf (stderr, "LoadLibrary WebView2Loader.dll failed: %lu\n", (unsigned long) GetLastError ());
		return FALSE;
	}

	g_create_env_with_options = (PFN_CreateCoreWebView2EnvironmentWithOptions) (void *) GetProcAddress (
		g_loader_module,
		"CreateCoreWebView2EnvironmentWithOptions");
	if (g_create_env_with_options == NULL) {
		fprintf (stderr, "GetProcAddress CreateCoreWebView2EnvironmentWithOptions failed\n");
		FreeLibrary (g_loader_module);
		g_loader_module = NULL;
		return FALSE;
	}
	return TRUE;
}

HRESULT vala_webview2_loader_create_environment (
	struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler)
{
	ICoreWebView2EnvironmentOptions *options = NULL;
	HRESULT hr;

	if (g_create_env_with_options == NULL || handler == NULL) {
		return E_FAIL;
	}

	/* Honor WEBKIT_INSPECTOR_SERVER / autoplay DENY / webdriver DISABLED / proxy → AdditionalBrowserArguments. */
	options = vala_webview2_host_create_environment_options ();
	hr = g_create_env_with_options (NULL, NULL, options, handler);
	if (options != NULL) {
		ICoreWebView2EnvironmentOptions_Release (options);
	}
	return hr;
}

static BOOL
make_host_user_data_folder (int route_id, wchar_t *out, size_t out_cch)
{
	wchar_t base[MAX_PATH];
	DWORD n;

	if (out == NULL || out_cch < 8 || route_id <= 0) {
		return FALSE;
	}
	n = GetEnvironmentVariableW (L"LOCALAPPDATA", base, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) {
		return FALSE;
	}
	_snwprintf (out, out_cch, L"%s\\webview2gtk\\profiles\\wv_%d", base, route_id);
	out[out_cch - 1] = L'\0';
	if (SHCreateDirectoryExW (NULL, out, NULL) != ERROR_SUCCESS
	    && GetLastError () != ERROR_ALREADY_EXISTS
	    && GetLastError () != ERROR_FILE_EXISTS) {
		/* Parent missing — try again after creating webview2gtk\profiles. */
		SHCreateDirectoryExW (NULL, out, NULL);
	}
	return TRUE;
}

HRESULT vala_webview2_loader_create_environment_for_host (
	int route_id,
	struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler)
{
	ICoreWebView2EnvironmentOptions *options = NULL;
	wchar_t folder[MAX_PATH];
	HRESULT hr;

	if (g_create_env_with_options == NULL || handler == NULL || route_id <= 0) {
		return E_FAIL;
	}
	if (!make_host_user_data_folder (route_id, folder, MAX_PATH)) {
		fprintf (stderr, "webview2gtk: local host proxy UserDataFolder failed for wv_%d\n", route_id);
		return E_FAIL;
	}
	options = vala_webview2_host_create_environment_options_for_route (route_id);
	hr = g_create_env_with_options (NULL, folder, options, handler);
	if (options != NULL) {
		ICoreWebView2EnvironmentOptions_Release (options);
	}
	return hr;
}
