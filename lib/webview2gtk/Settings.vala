namespace WebView2Gtk {

public class WebViewSettings : Object {
	public string user_agent { get; set; default = ""; }
	public HardwareAccelerationPolicy hardware_acceleration_policy {
		get;
		set;
		default = HardwareAccelerationPolicy.ON_DEMAND;
	}
	public bool enable_javascript { get; set; default = true; }
	public bool enable_developer_extras { get; set; default = false; }
	/** WebKitGTK-shaped — camera/mic media stream APIs. */
	public bool enable_media_stream { get; set; default = true; }
	/** WebKitGTK-shaped — WebRTC. */
	public bool enable_webrtc { get; set; default = true; }
	/** WebKitGTK-shaped — require a user gesture before media playback. */
	public bool media_playback_requires_user_gesture { get; set; default = false; }
	/**
	 * WebKitGTK-shaped — control navigator.webdriver visibility.
	 * DISABLED merges `--disable-blink-features=AutomationControlled` before
	 * WebView2 environment create (process-wide).
	 */
	public NavigatorWebDriverActivePolicy navigator_webdriver_active_policy {
		get;
		set;
		default = NavigatorWebDriverActivePolicy.AUTO;
	}
	/**
	 * WebKitGTK-shaped — trackpad/touch swipe-to-go-back/forward.
	 * Backed by the real ICoreWebView2Settings6.IsSwipeNavigationEnabled
	 * (checked against the vendored WebView2.h; WebView2Gtk.WebView pushes
	 * this to COM as soon as the host is attached, or immediately if it
	 * already is).
	 */
	public bool enable_back_forward_navigation_gestures { get; set; default = false; }
	/**
	 * WebKitGTK-shaped — HTTP back/forward cache. No WebView2 equivalent is
	 * exposed via ICoreWebView2Settings (checked across every …SettingsN
	 * version in the vendored WebView2.h) -- stored only, no-op.
	 */
	public bool enable_page_cache { get; set; default = true; }
	/**
	 * WebKitGTK-shaped — coarse "allow media" toggle. No WebView2 equivalent
	 * is exposed via ICoreWebView2Settings -- stored only, no-op.
	 */
	public bool enable_media { get; set; default = true; }
	/**
	 * WebKitGTK-shaped — Web Audio API. No WebView2 equivalent is exposed
	 * via ICoreWebView2Settings -- stored only, no-op.
	 */
	public bool enable_webaudio { get; set; default = true; }
	/**
	 * WebKitGTK-shaped — WebGL. No WebView2 equivalent is exposed via
	 * ICoreWebView2Settings -- Chromium's own --disable-webgl exists but
	 * only as a process-wide flag at environment creation (like
	 * navigator_webdriver_active_policy above), not a per-WebView COM
	 * property, and no consumer of this fork needs that yet -- stored only,
	 * no-op.
	 */
	public bool enable_webgl { get; set; default = true; }
}

/** WebKitGTK-shaped free function twin of Settings.set_navigator_webdriver_active_policy. */
public void set_navigator_webdriver_active_policy(
	WebViewSettings settings,
	NavigatorWebDriverActivePolicy policy
) {
	settings.navigator_webdriver_active_policy = policy;
}

/** WebKitGTK-shaped free function twin of Settings.get_navigator_webdriver_active_policy. */
public NavigatorWebDriverActivePolicy get_navigator_webdriver_active_policy(
	WebViewSettings settings
) {
	return settings.navigator_webdriver_active_policy;
}

}
