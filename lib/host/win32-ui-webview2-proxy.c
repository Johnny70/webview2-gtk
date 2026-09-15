/* Process latch for the library loopback hop (plan 6.0). */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "win32-ui-webview2-proxy.h"

static BOOL g_hop_active = FALSE;
static unsigned g_hop_port = 0;

void
vala_webview2_host_note_embedded_proxy (unsigned port)
{
	g_hop_port = port;
	g_hop_active = (port > 0 && port <= 65535) ? TRUE : FALSE;
	if (g_hop_active) {
		fprintf (
			stderr,
			"webview2gtk: embedded proxy hop on 127.0.0.1:%u\n",
			g_hop_port
		);
	}
}

bool
vala_webview2_host_embedded_proxy_active (void)
{
	return g_hop_active ? true : false;
}

unsigned
vala_webview2_host_embedded_proxy_port (void)
{
	return g_hop_port;
}
