#ifndef WIN32_UI_WEBVIEW2_PROXY_H
#define WIN32_UI_WEBVIEW2_PROXY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void vala_webview2_host_note_embedded_proxy (unsigned port);
bool vala_webview2_host_embedded_proxy_active (void);
unsigned vala_webview2_host_embedded_proxy_port (void);

#ifdef __cplusplus
}
#endif

#endif
