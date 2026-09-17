namespace WebView2Gtk {

/**
 * Minimal WebKit NavigationAction stand-in -- the request and navigation
 * type are all {@link NavigationPolicyDecision} needs to expose.
 */
public sealed class NavigationAction : Object {
	public URIRequest request { get; construct; }
	public NavigationType navigation_type { get; construct; }

	public NavigationAction(string uri, NavigationType navigation_type) {
		Object(request: new URIRequest(uri), navigation_type: navigation_type);
	}
}

}
