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
static BOOL g_profiles_swept;
static DWORD g_profile_token;

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
process_still_running (DWORD pid)
{
	HANDLE process;
	DWORD status;

	if (pid == 0) {
		return FALSE;
	}
	process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (process == NULL) {
		return GetLastError () == ERROR_ACCESS_DENIED;
	}
	if (!GetExitCodeProcess (process, &status)) {
		CloseHandle (process);
		return FALSE;
	}
	CloseHandle (process);
	return status == STILL_ACTIVE;
}

static void
delete_tree (const wchar_t *path)
{
	WIN32_FIND_DATAW fd;
	wchar_t pattern[MAX_PATH];
	wchar_t child[MAX_PATH];
	HANDLE find;

	if (path == NULL || path[0] == L'\0') {
		return;
	}
	_snwprintf (pattern, MAX_PATH, L"%s\\*", path);
	pattern[MAX_PATH - 1] = L'\0';
	find = FindFirstFileW (pattern, &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if (wcscmp (fd.cFileName, L".") == 0
			    || wcscmp (fd.cFileName, L"..") == 0) {
				continue;
			}
			_snwprintf (child, MAX_PATH, L"%s\\%s", path, fd.cFileName);
			child[MAX_PATH - 1] = L'\0';
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				delete_tree (child);
			} else {
				SetFileAttributesW (child, FILE_ATTRIBUTE_NORMAL);
				DeleteFileW (child);
			}
		} while (FindNextFileW (find, &fd));
		FindClose (find);
	}
	SetFileAttributesW (path, FILE_ATTRIBUTE_NORMAL);
	RemoveDirectoryW (path);
}

static int
profile_underscores (const wchar_t *name)
{
	int n = 0;

	for (; name != NULL && *name != L'\0'; name++) {
		if (*name == L'_') {
			n++;
		}
	}
	return n;
}

/* Drop folders whose owner pid is gone. Legacy wv_<id> has no pid — try
 * delete; a live msedgewebview2 lock just fails the delete. */
static void
sweep_dead_profile_dirs (const wchar_t *profiles_root)
{
	WIN32_FIND_DATAW fd;
	wchar_t glob[MAX_PATH];
	wchar_t names[64][MAX_PATH];
	int n_names = 0;
	int i;
	HANDLE find;
	unsigned long pid;
	unsigned long token;
	int route;

	if (profiles_root == NULL || profiles_root[0] == L'\0') {
		return;
	}
	_snwprintf (glob, MAX_PATH, L"%s\\wv_*", profiles_root);
	glob[MAX_PATH - 1] = L'\0';
	find = FindFirstFileW (glob, &fd);
	if (find == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			continue;
		}
		if (n_names >= (int) (sizeof (names) / sizeof (names[0]))) {
			break;
		}
		pid = 0;
		token = 0;
		route = 0;
		if (profile_underscores (fd.cFileName) == 3
		    && swscanf (fd.cFileName, L"wv_%lu_%lu_%d", &pid, &token, &route) == 3) {
			if (pid == GetCurrentProcessId () || process_still_running ((DWORD) pid)) {
				continue;
			}
		} else if (profile_underscores (fd.cFileName) == 2
		    && swscanf (fd.cFileName, L"wv_%lu_%d", &pid, &route) == 2) {
			if (pid == GetCurrentProcessId () || process_still_running ((DWORD) pid)) {
				continue;
			}
		} else if (profile_underscores (fd.cFileName) != 1) {
			continue;
		}
		wcsncpy (names[n_names], fd.cFileName, MAX_PATH);
		names[n_names][MAX_PATH - 1] = L'\0';
		n_names++;
	} while (FindNextFileW (find, &fd));
	FindClose (find);
	for (i = 0; i < n_names; i++) {
		wchar_t child[MAX_PATH];

		_snwprintf (child, MAX_PATH, L"%s\\%s", profiles_root, names[i]);
		child[MAX_PATH - 1] = L'\0';
		delete_tree (child);
	}
}

static DWORD
profile_token (void)
{
	if (g_profile_token == 0) {
		g_profile_token = GetTickCount ();
		if (g_profile_token == 0) {
			g_profile_token = 1;
		}
	}
	return g_profile_token;
}

static BOOL
make_host_user_data_folder (int route_id, wchar_t *out, size_t out_cch)
{
	wchar_t base[MAX_PATH];
	wchar_t profiles[MAX_PATH];
	DWORD n;

	if (out == NULL || out_cch < 8 || route_id <= 0) {
		return FALSE;
	}
	n = GetEnvironmentVariableW (L"LOCALAPPDATA", base, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) {
		return FALSE;
	}
	_snwprintf (profiles, MAX_PATH, L"%s\\webview2gtk\\profiles", base);
	profiles[MAX_PATH - 1] = L'\0';
	if (!g_profiles_swept) {
		g_profiles_swept = TRUE;
		sweep_dead_profile_dirs (profiles);
	}
	/* pid + start tick: never reuse wv_<id>. A leftover browser process
	 * from the previous run holds that folder and env create hangs. */
	_snwprintf (
		out,
		out_cch,
		L"%s\\wv_%lu_%lu_%d",
		profiles,
		(unsigned long) GetCurrentProcessId (),
		(unsigned long) profile_token (),
		route_id
	);
	out[out_cch - 1] = L'\0';
	if (SHCreateDirectoryExW (NULL, out, NULL) != ERROR_SUCCESS
	    && GetLastError () != ERROR_ALREADY_EXISTS
	    && GetLastError () != ERROR_FILE_EXISTS) {
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
		fprintf (stderr, "webview2gtk: local host proxy UserDataFolder failed for route %d\n", route_id);
		return E_FAIL;
	}
	fprintf (stderr, "webview2gtk: local host proxy UserDataFolder %ls\n", folder);
	options = vala_webview2_host_create_environment_options_for_route (route_id);
	hr = g_create_env_with_options (NULL, folder, options, handler);
	if (options != NULL) {
		ICoreWebView2EnvironmentOptions_Release (options);
	}
	return hr;
}
