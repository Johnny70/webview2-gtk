namespace WebView2Gtk {

/**
 * WebKitGTK-shaped navigation policy decision for NavigationStarting.
 *
 * Unlike {@link ResponsePolicyDecision}, ignore() here is real: WebView2's
 * NavigationStarting is answered synchronously -- the native host
 * (win32-ui-webview2-events.c) reads is_ignored() the moment the
 * decide_policy signal emission that carries this object returns, and
 * applies it to the real WebView2 NavigationStartingEventArgs before the
 * navigation proceeds.
 */
public sealed class NavigationPolicyDecision : PolicyDecision {
	public NavigationAction navigation_action { get; construct; }
	private bool _is_ignored = false;

	public NavigationPolicyDecision(NavigationAction navigation_action) {
		Object(navigation_action: navigation_action);
	}

	public override void ignore() {
		_is_ignored = true;
	}

	public bool is_ignored() {
		return _is_ignored;
	}
}

}
