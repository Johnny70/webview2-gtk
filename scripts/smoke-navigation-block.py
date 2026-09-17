"""Security gate: prove WebView2Gtk.PolicyDecision.ignore() actually cancels
a navigation, not just records that it was asked to.

Loads a real page, then fires a real navigation to a URL the decide_policy
handler ignores, and asserts get_uri() did not change afterward. This is
the one thing untrusted HTML mail (WinPostcard's whole reason for this
fork) depends on -- it must be an observed fact, not an assumption.

Run against a built lib/install-staging tree, e.g.:
  PATH="$PWD/build/install-staging/bin:$PATH" \
  GI_TYPELIB_PATH="$PWD/build/install-staging/lib/girepository-1.0" \
  python scripts/smoke-navigation-block.py
"""

import sys

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("WebView2Gtk", "1.0")

from gi.repository import GLib, Gtk, WebView2Gtk  # noqa: E402

BLOCKED_URL = "https://example.invalid/blocked-target"
INITIAL_HTML = f'<html><body><a id="l" href="{BLOCKED_URL}">go</a></body></html>'

result = {
    "decide_policy_fired": False,
    "uri_after_initial_load": None,
    "uri_after_blocked_nav_attempt": None,
    "failed": None,
}


def on_decide_policy(_web: WebView2Gtk.WebView, decision: WebView2Gtk.PolicyDecision, kind: int) -> bool:
    if kind != WebView2Gtk.PolicyDecisionType.NAVIGATION_ACTION:
        return False
    uri = decision.get_navigation_action().get_request().get_uri()
    if uri == BLOCKED_URL:
        result["decide_policy_fired"] = True
        decision.ignore()
    else:
        decision.use()
    return True


def finish(app: Gtk.Application, ok: bool, message: str) -> None:
    if not ok:
        result["failed"] = message
    app.quit()


def main() -> int:
    app = Gtk.Application(application_id="dev.webview2gtk.smoke-navigation-block")

    def on_activate(app: Gtk.Application) -> None:
        window = Gtk.ApplicationWindow(application=app)
        window.set_default_size(640, 480)
        web = WebView2Gtk.WebView()
        web.connect("decide-policy", on_decide_policy)
        window.set_child(web)
        window.present()

        state = {"stage": "initial_load"}

        def on_load_changed(_web: WebView2Gtk.WebView, load_event: int) -> None:
            if load_event != WebView2Gtk.LoadEvent.FINISHED:
                return
            if state["stage"] == "initial_load":
                state["stage"] = "navigating_to_blocked"
                result["uri_after_initial_load"] = web.get_uri()
                web.load_uri(BLOCKED_URL)
                # NavigationStarting for the ignored navigation is synchronous
                # (no GetDeferral), but WebView2's own dispatch is still async
                # relative to this call -- give it a beat, then check state.
                GLib.timeout_add(1500, check_after_blocked_attempt)
            elif state["stage"] == "navigating_to_blocked":
                # A real navigation actually finished after we asked to
                # ignore one -- that is the failure this test exists to catch.
                if web.get_uri() == BLOCKED_URL:
                    finish(app, False, "load_changed(FINISHED) fired for the blocked URL -- ignore() did not cancel it")

        def check_after_blocked_attempt() -> bool:
            result["uri_after_blocked_nav_attempt"] = web.get_uri()
            if not result["decide_policy_fired"]:
                finish(app, False, "decide_policy never fired for the navigation to BLOCKED_URL")
            elif web.get_uri() == BLOCKED_URL:
                finish(app, False, "WebView navigated to BLOCKED_URL despite decision.ignore()")
            else:
                finish(app, True, "ok")
            return GLib.SOURCE_REMOVE

        web.connect("load-changed", on_load_changed)
        web.load_html(INITIAL_HTML, None)

        # Overall safety net: this fork's async WebView2 attach could simply
        # never come up (e.g. missing runtime) -- do not hang the test suite.
        def on_timeout() -> bool:
            finish(app, False, "timed out waiting for WebView2 host to attach / navigate")
            return GLib.SOURCE_REMOVE

        GLib.timeout_add(20000, on_timeout)

    app.connect("activate", on_activate)
    app.run([])

    print(f"result: {result}")
    if result["failed"] is not None:
        print(f"FAIL: {result['failed']}", file=sys.stderr)
        return 1
    print("PASS: decision.ignore() prevented the navigation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
