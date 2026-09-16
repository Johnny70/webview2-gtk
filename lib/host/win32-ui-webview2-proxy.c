/* Process latch for the library local host proxy (plan 6.0). */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "win32-ui-webview2-proxy.h"

static BOOL g_embedded_proxy_active = FALSE;
static unsigned g_embedded_proxy_port = 0;

void
vala_webview2_host_note_embedded_proxy (unsigned port)
{
	g_embedded_proxy_port = port;
	g_embedded_proxy_active = (port > 0 && port <= 65535) ? TRUE : FALSE;
	if (g_embedded_proxy_active) {
		fprintf (
			stderr,
			"webview2gtk: local host proxy on 127.0.0.1:%u\n",
			g_embedded_proxy_port
		);
	}
}

bool
vala_webview2_host_embedded_proxy_active (void)
{
	return g_embedded_proxy_active ? true : false;
}

unsigned
vala_webview2_host_embedded_proxy_port (void)
{
	return g_embedded_proxy_port;
}
