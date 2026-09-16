[CCode(cheader_filename = "webview2gtk-host-api.h", cname = "vala_webview2_host_note_embedded_proxy")]
extern void wv2_host_note_embedded_proxy(uint16 port);

[CCode(cheader_filename = "webview2gtk-host-api.h", cname = "vala_webview2_host_embedded_proxy_active")]
extern bool wv2_host_embedded_proxy_active();

namespace WebView2Gtk
{

	/**
	 * Library local host HTTP CONNECT proxy (plan 6.0).
	 *
	 * Port of the consumer CONNECT / forward server: listen thread starts
	 * first. One route table lives on that thread; GTK posts a one-line
	 * change with ''context.invoke''. Per view id (Proxy-Authorization).
	 * No row (and dummy [[http://127.0.0.1]]) is pass-through: connect out,
	 * no extra upstream. ''about:'' is answered without a table lookup.
	 */
	internal class EmbeddedProxy : Object
	{
		private static EmbeddedProxy? instance = null;
		private static GLib.Mutex start_mutex;

		private uint16 port = 0;
		private GLib.Thread<void*>? thread = null;
		private GLib.MainContext? context = null;
		private GLib.MainLoop? loop = null;
		private GLib.SocketService? service = null;
		private Gee.HashMap<int, string> routes { get; set; default = new Gee.HashMap<int, string>(); }
		private GLib.Mutex wait_mutex = GLib.Mutex();
		private GLib.Cond wait_cond = GLib.Cond();
		private bool listen_done = false;
		private bool listen_ok = false;

		internal static bool ensure()
		{
			if (instance != null && instance.port > 0) {
				return true;
			}
			if (wv2_host_environment_created() && !wv2_host_embedded_proxy_active()) {
				return false;
			}
			start_mutex.lock();
			if (instance != null && instance.port > 0) {
				start_mutex.unlock();
				return true;
			}
			if (wv2_host_environment_created() && !wv2_host_embedded_proxy_active()) {
				start_mutex.unlock();
				return false;
			}
			var proxy = new EmbeddedProxy();
			proxy.thread = new GLib.Thread<void*>("webview2gtk-proxy", proxy.thread_main);
			proxy.wait_mutex.lock();
			while (!proxy.listen_done) {
				proxy.wait_cond.wait(proxy.wait_mutex);
			}
			var ok = proxy.listen_ok;
			proxy.wait_mutex.unlock();
			if (!ok) {
				start_mutex.unlock();
				return false;
			}
			instance = proxy;
			wv2_host_note_embedded_proxy(proxy.port);
			start_mutex.unlock();
			return true;
		}

		/* Empty string = pass-through. Null = drop the row (pass-through). */
		internal static void set_upstream(int id, string? proxy_uri)
		{
			if (id <= 0 || instance == null || instance.context == null) {
				return;
			}
			instance.context.invoke(() => {
				if (proxy_uri == null) {
					instance.routes.unset(id);
				} else {
					instance.routes[id] = proxy_uri;
				}
				return GLib.Source.REMOVE;
			});
		}

		private void* thread_main()
		{
			this.context = new GLib.MainContext();
			this.context.push_thread_default();
			this.loop = new GLib.MainLoop(this.context, false);
			try {
				this.service = new GLib.SocketService();
				GLib.SocketAddress effective;
				this.service.add_address(
					new GLib.InetSocketAddress(new GLib.InetAddress.loopback(GLib.SocketFamily.IPV4), 0),
					GLib.SocketType.STREAM,
					GLib.SocketProtocol.TCP,
					null,
					out effective
				);
				var inet = effective as GLib.InetSocketAddress;
				if (inet == null || inet.port == 0) {
					throw new GLib.IOError.FAILED("localhost bind did not return a port");
				}
				this.port = (uint16) inet.port;
				this.service.incoming.connect((connection) => {
					GLib.debug("webview2gtk: proxy incoming");
					this.serve.begin(connection);
					return true;
				});
				this.service.start();
			} catch (GLib.Error listen_error) {
				GLib.warning("WebView2Gtk: local host proxy listen failed: %s", listen_error.message);
				this.wait_mutex.lock();
				this.listen_ok = false;
				this.listen_done = true;
				this.wait_cond.signal();
				this.wait_mutex.unlock();
				this.context.pop_thread_default();
				return null;
			}
			this.wait_mutex.lock();
			this.listen_ok = true;
			this.listen_done = true;
			this.wait_cond.signal();
			this.wait_mutex.unlock();
			GLib.message("webview2gtk: local host proxy listening http://127.0.0.1:%u", this.port);
			this.loop.run();
			this.context.pop_thread_default();
			return null;
		}

		private async void serve(GLib.SocketConnection client)
		{
			try {
				var client_in = new GLib.DataInputStream(client.input_stream);
				client_in.close_base_stream = false;
				client_in.set_newline_type(GLib.DataStreamNewlineType.ANY);
				client_in.set_buffer_size(65536);
				var head = yield this.read_head(client_in);
				if (head.length == 0) {
					GLib.debug("webview2gtk: proxy empty request");
					client.close();
					return;
				}
				var first_nl = head.index_of("\r\n");
				if (first_nl < 0) {
					client.close();
					return;
				}
				var parts = head.substring(0, first_nl).split(" ", 3);
				if (parts.length < 2) {
					GLib.debug("webview2gtk: proxy short request line");
					client.close();
					return;
				}
				GLib.debug("webview2gtk: proxy req %s %s", parts[0], parts[1]);

				var view_id = 0;
				var saw_proxy_auth = false;
				var hdr_lines = head.split("\r\n");
				for (var i = 1; i < hdr_lines.length; i++) {
					if (hdr_lines[i] == "") {
						continue;
					}
					var name_end = hdr_lines[i].index_of_char(':');
					if (name_end < 0) {
						continue;
					}
					if (hdr_lines[i].substring(0, name_end).strip().down() != "proxy-authorization") {
						continue;
					}
					saw_proxy_auth = true;
					var raw = hdr_lines[i].substring(name_end + 1).strip();
					var auth = raw.split(" ", 2);
					if (auth.length < 2 || auth[0].down() != "basic") {
						break;
					}
					var decoded = (string) GLib.Base64.decode(auth[1].strip());
					var colon = decoded.index_of_char(':');
					var id_text = decoded;
					if (colon >= 0) {
						id_text = decoded.substring(0, colon);
					}
					view_id = int.parse(id_text);
					break;
				}
				if (view_id <= 0) {
					GLib.debug("webview2gtk: proxy no auth %s %s (pass-through)",
						parts[0], parts[1]);
				}
				if (parts[1].down().has_prefix("about:")) {
					string about_ok;
					if (parts[0].up() == "CONNECT") {
						about_ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
					} else {
						about_ok = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
					}
					client.output_stream.write_all(about_ok.data[0:about_ok.length], null);
					client.close();
					GLib.debug("webview2gtk: proxy about: id=%d %s %s", view_id, parts[0], parts[1]);
					return;
				}

				if (parts[0].up() == "CONNECT") {
					GLib.debug("webview2gtk: proxy CONNECT id=%d %s row=%s",
						view_id, parts[1], this.routes.has_key(view_id) ? "yes" : "no");
					yield this.tunnel_connect(client, parts[1], view_id);
					return;
				}
				/* HTTPS uses CONNECT (byte tunnel). Plain HTTP is forwarded:
				   Chromium may send absolute-form or origin-form + Host. */
				var target = parts[1];
				if (target.has_prefix("/")) {
					var host_hdr = "";
					for (var i = 1; i < hdr_lines.length; i++) {
						if (hdr_lines[i] == "") {
							continue;
						}
						var name_end = hdr_lines[i].index_of_char(':');
						if (name_end < 0) {
							continue;
						}
						if (hdr_lines[i].substring(0, name_end).strip().down() != "host") {
							continue;
						}
						host_hdr = hdr_lines[i].substring(name_end + 1).strip();
						break;
					}
					if (host_hdr == "") {
						var no_host = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
						client.output_stream.write_all(no_host.data[0:no_host.length], null);
						client.close();
						return;
					}
					target = "http://" + host_hdr + target;
				}
				if (!target.has_prefix("http://")) {
					var bad = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
					client.output_stream.write_all(bad.data[0:bad.length], null);
					client.close();
					return;
				}
				GLib.debug("webview2gtk: proxy HTTP id=%d %s %s row=%s",
					view_id, parts[0], target, this.routes.has_key(view_id) ? "yes" : "no");
				yield this.forward_http(client.output_stream, client_in, parts[0], target,
					head, view_id);
				client.close();
			} catch (GLib.Error serve_error) {
				GLib.debug("webview2gtk: proxy serve: %s", serve_error.message);
				try {
					client.close();
				} catch (GLib.Error close_error) {
				}
			}
		}

		private async string read_head(GLib.DataInputStream stream) throws GLib.Error
		{
			var head = new GLib.StringBuilder();
			while (true) {
				size_t length;
				var line = yield stream.read_line_utf8_async(GLib.Priority.DEFAULT, null, out length);
				if (line == null) {
					return head.str;
				}
				head.append(line + "\r\n");
				if (line.strip() == "") {
					return head.str;
				}
			}
		}

		/**
		 * CONNECT tunnel: optional upstream CONNECT, then byte pipe.
		 */
		private async void tunnel_connect(
			GLib.SocketConnection client,
			string host_port,
			int view_id
		) throws GLib.Error
		{
			var host = host_port;
			var port = 443;
			var colon = host_port.last_index_of_char(':');
			if (colon > 0) {
				host = host_port.substring(0, colon);
				port = int.parse(host_port.substring(colon + 1));
			}

			GLib.SocketConnection remote;
			try {
				remote = yield this.open_far(host, (uint16) port, host_port, view_id);
			} catch (GLib.Error connect_error) {
				var bad = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
				client.output_stream.write_all(bad.data[0:bad.length], null);
				client.close();
				GLib.debug("webview2gtk: proxy CONNECT 502 %s: %s", host_port, connect_error.message);
				return;
			}

			var established = "HTTP/1.1 200 Connection Established\r\n\r\n";
			client.output_stream.write_all(established.data[0:established.length], null);

			var cancel = new GLib.Cancellable();
			var pending_splices = 2;
			GLib.SourceFunc resume = () => {
				tunnel_connect.callback();
				return GLib.Source.REMOVE;
			};
			remote.output_stream.splice_async.begin(
				client.input_stream,
				GLib.OutputStreamSpliceFlags.NONE,
				GLib.Priority.DEFAULT, cancel, (o, res) => {
					try {
						remote.output_stream.splice_async.end(res);
					} catch (GLib.Error e) {
					}
					cancel.cancel();
					pending_splices--;
					if (pending_splices <= 0) {
						resume();
					}
				});
			client.output_stream.splice_async.begin(
				remote.input_stream,
				GLib.OutputStreamSpliceFlags.NONE,
				GLib.Priority.DEFAULT, cancel, (o, res) => {
					try {
						client.output_stream.splice_async.end(res);
					} catch (GLib.Error e) {
					}
					cancel.cancel();
					pending_splices--;
					if (pending_splices <= 0) {
						resume();
					}
				});
			yield;
			try {
				client.close();
			} catch (GLib.Error close_client) {
			}
			try {
				remote.close();
			} catch (GLib.Error close_remote) {
			}
		}

		/**
		 * Forward one HTTP request (absolute-form, or origin-form already
		 * rewritten to [[http://host/path]]). Optional upstream.
		 */
		private async void forward_http(
			GLib.OutputStream client_out,
			GLib.DataInputStream client_in,
			string method,
			string absolute_uri,
			string head,
			int view_id
		) throws GLib.Error
		{
			var without_scheme = absolute_uri.substring(7);
			var slash = without_scheme.index_of_char('/');
			var host_port = without_scheme;
			var path = "/";
			if (slash >= 0) {
				host_port = without_scheme.substring(0, slash);
				path = without_scheme.substring(slash);
			}
			var host = host_port;
			var dest_port = 80;
			var colon = host_port.last_index_of_char(':');
			if (colon > 0) {
				host = host_port.substring(0, colon);
				dest_port = int.parse(host_port.substring(colon + 1));
			}

			var upstream = "";
			if (this.routes.has_key(view_id)) {
				upstream = this.routes[view_id];
			}
			var req_target = path;
			if (upstream != "") {
				req_target = absolute_uri;
			}
			var out_head = new GLib.StringBuilder();
			out_head.append_printf("%s %s HTTP/1.1\r\n", method, req_target);
			var has_host = false;
			var content_length = 0;
			var lines = head.split("\r\n");
			for (var i = 1; i < lines.length; i++) {
				if (lines[i] == "") {
					continue;
				}
				var name_end = lines[i].index_of_char(':');
				if (name_end < 0) {
					continue;
				}
				var name = lines[i].substring(0, name_end).strip().down();
				if (name == "connection" || name == "proxy-connection"
				    || name == "proxy-authorization") {
					continue;
				}
				if (name == "host") {
					has_host = true;
				}
				if (name == "content-length") {
					content_length = int.parse(lines[i].substring(name_end + 1).strip());
				}
				out_head.append(lines[i] + "\r\n");
			}
			if (!has_host) {
				out_head.append_printf("Host: %s\r\n", host_port);
			}
			out_head.append("Connection: close\r\n\r\n");

			var body = new uint8[content_length];
			if (content_length > 0) {
				size_t n_read;
				yield client_in.read_all_async(body, GLib.Priority.DEFAULT, null, out n_read);
			}

			GLib.SocketConnection remote;
			if (upstream != "") {
				remote = yield this.connect_upstream(upstream);
			} else {
				remote = yield new GLib.SocketClient().connect_to_host_async(host, (uint16) dest_port, null);
			}
			remote.output_stream.write_all(out_head.str.data[0:out_head.len], null);
			if (content_length > 0) {
				remote.output_stream.write_all(body, null);
			}
			yield client_out.splice_async(remote.input_stream,
				GLib.OutputStreamSpliceFlags.CLOSE_SOURCE | GLib.OutputStreamSpliceFlags.CLOSE_TARGET,
				GLib.Priority.DEFAULT, null);
		}

		/**
		 * Open TCP to host:port, or CONNECT via mapped upstream.
		 */
		private async GLib.SocketConnection open_far(
			string host,
			uint16 dest_port,
			string host_port,
			int view_id
		) throws GLib.Error
		{
			var upstream = "";
			if (this.routes.has_key(view_id)) {
				upstream = this.routes[view_id];
			}
			if (upstream == "") {
				return yield new GLib.SocketClient().connect_to_host_async(host, dest_port, null);
			}
			var remote = yield this.connect_upstream(upstream);
			var req = ("CONNECT %s HTTP/1.1\r\nHost: %s\r\n"
				+ "Proxy-Connection: keep-alive\r\n\r\n").printf(host_port, host_port);
			remote.output_stream.write_all(req.data[0:req.length], null);
			var remote_in = new GLib.DataInputStream(remote.input_stream);
			remote_in.close_base_stream = false;
			remote_in.set_newline_type(GLib.DataStreamNewlineType.ANY);
			remote_in.set_buffer_size(65536);
			var resp = yield this.read_head(remote_in);
			var status_nl = resp.index_of("\r\n");
			var status = resp;
			if (status_nl >= 0) {
				status = resp.substring(0, status_nl);
			}
			if (!status.contains(" 200")) {
				throw new GLib.IOError.FAILED("upstream CONNECT failed: %s", status);
			}
			return remote;
		}

		private async GLib.SocketConnection connect_upstream(string proxy_uri) throws GLib.Error
		{
			var parsed = GLib.Uri.parse(proxy_uri, GLib.UriFlags.NONE);
			var up_host = parsed.get_host() ?? "";
			if (up_host == "") {
				throw new GLib.IOError.FAILED("upstream missing host: %s", proxy_uri);
			}
			var up_port = parsed.get_port();
			if (up_port < 0) {
				up_port = 80;
			}
			return yield new GLib.SocketClient().connect_to_host_async(up_host, (uint16) up_port, null);
		}

	}

}
