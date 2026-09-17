namespace WebView2Gtk {

public enum SnapshotRegion {
	VISIBLE,
	FULL_DOCUMENT
}

public enum SnapshotOptions {
	NONE
}

public enum NetworkProxyMode {
	DEFAULT,
	CUSTOM,
	NONE
}

public enum TLSErrorsPolicy {
	IGNORE,
	FAIL
}

public enum HardwareAccelerationPolicy {
	ON_DEMAND,
	ALWAYS,
	NEVER
}

public enum CookieAcceptPolicy {
	ALWAYS,
	NEVER,
	NO_THIRD_PARTY
}

public enum CookiePersistentStorage {
	TEXT,
	SQLITE
}

/** WebKitGTK-shaped — media autoplay policy for {@link WebsitePolicies}. */
public enum AutoplayPolicy {
	ALLOW,
	ALLOW_WITHOUT_SOUND,
	DENY
}

/**
 * WebKitGTK-shaped — whether page JS sees navigator.webdriver as active.
 * On Windows, {@link NavigatorWebDriverActivePolicy.DISABLED} maps to
 * Chromium `--disable-blink-features=AutomationControlled` at environment create.
 */
public enum NavigatorWebDriverActivePolicy {
	AUTO,
	ENABLED,
	DISABLED
}

/**
 * WebKitGTK-shaped -- LINK_CLICKED maps from WebView2's own IsUserInitiated
 * navigation-starting flag. Every other navigation (redirect, form
 * auto-submit, script navigation, reload) comes through as OTHER, since
 * WebView2's NavigationStarting event does not distinguish them the way
 * WebKit's engine does.
 */
public enum NavigationType {
	LINK_CLICKED,
	FORM_SUBMITTED,
	BACK_FORWARD,
	RELOAD,
	FORM_RESUBMITTED,
	OTHER
}

/** WebKitGTK-shaped subset — used by {@link WebView.load_failed}. */
public errordomain NetworkError {
	FAILED,
	TRANSPORT,
	UNKNOWN_PROTOCOL,
	CANCELLED,
	FILE_DOES_NOT_EXIST;
}

}
