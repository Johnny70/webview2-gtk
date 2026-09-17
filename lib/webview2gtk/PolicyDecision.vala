namespace WebView2Gtk {

/** WebKitGTK-shaped — type of {@link WebView.decide_policy} decision. */
public enum PolicyDecisionType {
	NAVIGATION_ACTION,
	NEW_WINDOW_ACTION,
	RESPONSE
}

/**
 * WebKitGTK-shaped pending policy decision.
 *
 * {@link use}, {@link ignore}, and {@link download} are no-ops here by
 * default (observe-only). {@link NavigationPolicyDecision} overrides
 * ignore() to actually cancel the navigation -- see its own doc comment.
 * {@link ResponsePolicyDecision} (PolicyDecisionType.RESPONSE) does not
 * override anything yet, so it stays observe-only.
 */
public abstract class PolicyDecision : Object {
	public virtual void use() {
	}

	public virtual void ignore() {
	}

	public virtual void download() {
	}
}

}
