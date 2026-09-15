[CCode(cheader_filename = "webview2gtk-host-api.h", cname = "vala_webview2_host_note_embedded_proxy")]
extern void wv2_host_note_embedded_proxy(uint16 port);

[CCode(cheader_filename = "webview2gtk-host-api.h", cname = "vala_webview2_host_embedded_proxy_active")]
extern bool wv2_host_embedded_proxy_active();

namespace WebView2Gtk
{

	/**
	 * Library loopback HTTP CONNECT hop (plan 6.0).
	 *
	 * Port of the consumer CONNECT / forward server: own thread +
	 * ''GLib.MainContext''. Routes are per view id (Proxy-Authorization)
	 * instead of per host; GTK posts via ''context.invoke''. Dummy
	 * ''http://127.0.0.1'' CUSTOM is pass-through, not an upstream.
	 */
	internal class EmbeddedProxy : Object
	{
		private static EmbeddedProxy? instance = null;
		private static Mutex start_mutex;

		private uint16 port = 0;
		private Thread<void*>? thread = null;
		private MainContext? context = null;
		private MainLoop? loop = null;
		private SocketService? service = null;
		private Gee.HashMap<int, string> routes { get; set; default = new Gee.HashMap<int, string>(); }
		private Gee.HashMap<int, string> pending { get; set; default = new Gee.HashMap<int, string>(); }
		private Mutex wait_mutex = Mutex();
		private Cond wait_cond = Cond();
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
			proxy.thread = new Thread<void*>("webview2gtk-proxy", proxy.thread_main);
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

		/* Empty string = DIRECT. Null = drop the row (fail-closed). */
		internal static void set_upstream(int id, string? proxy_uri)
		{
			if (id <= 0 || instance == null) {
				return;
			}
			var ctx = instance.context;
			if (ctx == null) {
				if (proxy_uri == null) {
					instance.pending.unset(id);
				} else {
					instance.pending[id] = proxy_uri;
				}
				return;
			}
			ctx.invoke(() => {
				if (proxy_uri == null) {
					instance.routes.unset(id);
				} else {
					instance.routes[id] = proxy_uri;
				}
				return Source.REMOVE;
			});
		}

		private void* thread_main()
		{
			this.context = new MainContext();
			this.context.push_thread_default();
			this.loop = new MainLoop(this.context, false);
			try {
				this.service = new SocketService();
				SocketAddress effective;
				this.service.add_address(
					new InetSocketAddress(new InetAddress.loopback(SocketFamily.IPV4), 0),
					SocketType.STREAM,
					SocketProtocol.TCP,
					null,
					out effective
				);
				var inet = effective as InetSocketAddress;
				if (inet == null || inet.port == 0) {
					throw new IOError.FAILED("loopback bind did not return a port");
				}
				this.port = inet.port;
				foreach (var id in this.pending.keys) {
					this.routes[id] = this.pending[id];
				}
				this.pending.clear();
				this.service.incoming.connect((connection) => {
					this.serve.begin(connection);
					return false;
				});
				this.service.start();
			} catch (Error listen_error) {
				warning("WebView2Gtk: embedded proxy listen failed: %s", listen_error.message);
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
			message("webview2gtk: embedded proxy listening http://127.0.0.1:%u", this.port);
			this.loop.run();
			this.context.pop_thread_default();
			return null;
		}

		private async void serve(SocketConnection client)
		{
			try {
				var client_in = new DataInputStream(client.input_stream);
				client_in.close_base_stream = false;
				client_in.set_newline_type(DataStreamNewlineType.ANY);
				client_in.set_buffer_size(65536);
				var head = yield this.read_head(client_in);
				if (head.length == 0) {
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
					client.close();
					return;
				}

				var view_id = 0;
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
					var raw = hdr_lines[i].substring(name_end + 1).strip();
					var auth = raw.split(" ", 2);
					if (auth.length < 2 || auth[0].down() != "basic") {
						break;
					}
					try {
						var decoded = (string) Base64.decode(auth[1].strip());
						var colon = decoded.index_of_char(':');
						var id_text = decoded;
						if (colon >= 0) {
							id_text = decoded.substring(0, colon);
						}
						view_id = int.parse(id_text);
					} catch (Error e) {
					}
					break;
				}
				if (view_id <= 0) {
					var need_auth = "HTTP/1.1 407 Proxy Authentication Required\r\nConnection: close\r\n\r\n";
					client.output_stream.write_all(need_auth.data[0:need_auth.length], null);
					client.close();
					return;
				}
				if (!this.routes.has_key(view_id)) {
					var bad_gw = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
					client.output_stream.write_all(bad_gw.data[0:bad_gw.length], null);
					client.close();
					return;
				}

				if (parts[0].up() == "CONNECT") {
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
				yield this.forward_http(client.output_stream, client_in, parts[0], target,
					head, view_id);
				client.close();
			} catch (Error serve_error) {
				debug("%s", serve_error.message);
				try {
					client.close();
				} catch (Error close_error) {
				}
			}
		}

		private async string read_head(DataInputStream stream) throws Error
		{
			var head = new StringBuilder();
			while (true) {
				size_t length;
				var line = yield stream.read_line_utf8_async(Priority.DEFAULT, null, out length);
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
			SocketConnection client,
			string host_port,
			int view_id
		) throws Error
		{
			var host = host_port;
			var port = 443;
			var colon = host_port.last_index_of_char(':');
			if (colon > 0) {
				host = host_port.substring(0, colon);
				port = int.parse(host_port.substring(colon + 1));
			}

			SocketConnection remote;
			try {
				remote = yield this.open_far(host, (uint16) port, host_port, view_id);
			} catch (Error connect_error) {
				var bad = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
				client.output_stream.write_all(bad.data[0:bad.length], null);
				client.close();
				debug("%s", connect_error.message);
				return;
			}

			var established = "HTTP/1.1 200 Connection Established\r\n\r\n";
			client.output_stream.write_all(established.data[0:established.length], null);

			var cancel = new Cancellable();
			var pending_splices = 2;
			SourceFunc resume = () => {
				tunnel_connect.callback();
				return Source.REMOVE;
			};
			remote.output_stream.splice_async.begin(
				client.input_stream,
				OutputStreamSpliceFlags.NONE,
				Priority.DEFAULT, cancel, (o, res) => {
					try {
						remote.output_stream.splice_async.end(res);
					} catch (Error e) {
					}
					cancel.cancel();
					pending_splices--;
					if (pending_splices <= 0) {
						resume();
					}
				});
			client.output_stream.splice_async.begin(
				remote.input_stream,
				OutputStreamSpliceFlags.NONE,
				Priority.DEFAULT, cancel, (o, res) => {
					try {
						client.output_stream.splice_async.end(res);
					} catch (Error e) {
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
			} catch (Error close_client) {
			}
			try {
				remote.close();
			} catch (Error close_remote) {
			}
		}

		/**
		 * Forward one HTTP request (absolute-form, or origin-form already
		 * rewritten to ''http://host/path''). Optional upstream hop.
		 */
		private async void forward_http(
			OutputStream client_out,
			DataInputStream client_in,
			string method,
			string absolute_uri,
			string head,
			int view_id
		) throws Error
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

			var upstream = this.routes[view_id];
			var req_target = path;
			if (upstream != "") {
				req_target = absolute_uri;
			}
			var out_head = new StringBuilder();
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
				yield client_in.read_all_async(body, Priority.DEFAULT, null, out n_read);
			}

			SocketConnection remote;
			if (upstream != "") {
				remote = yield this.connect_upstream(upstream);
			} else {
				remote = yield new SocketClient().connect_to_host_async(host, (uint16) dest_port, null);
			}
			remote.output_stream.write_all(out_head.str.data[0:out_head.len], null);
			if (content_length > 0) {
				remote.output_stream.write_all(body, null);
			}
			yield client_out.splice_async(remote.input_stream,
				OutputStreamSpliceFlags.CLOSE_SOURCE | OutputStreamSpliceFlags.CLOSE_TARGET,
				Priority.DEFAULT, null);
		}

		/**
		 * Open TCP to host:port, or CONNECT via mapped upstream.
		 */
		private async SocketConnection open_far(
			string host,
			uint16 dest_port,
			string host_port,
			int view_id
		) throws Error
		{
			var upstream = this.routes[view_id];
			if (upstream == "") {
				return yield new SocketClient().connect_to_host_async(host, dest_port, null);
			}
			var remote = yield this.connect_upstream(upstream);
			var req = ("CONNECT %s HTTP/1.1\r\nHost: %s\r\n"
				+ "Proxy-Connection: keep-alive\r\n\r\n").printf(host_port, host_port);
			remote.output_stream.write_all(req.data[0:req.length], null);
			var remote_in = new DataInputStream(remote.input_stream);
			remote_in.close_base_stream = false;
			remote_in.set_newline_type(DataStreamNewlineType.ANY);
			remote_in.set_buffer_size(65536);
			var resp = yield this.read_head(remote_in);
			var status_nl = resp.index_of("\r\n");
			var status = resp;
			if (status_nl >= 0) {
				status = resp.substring(0, status_nl);
			}
			if (!status.contains(" 200")) {
				throw new IOError.FAILED("upstream CONNECT failed: %s", status);
			}
			return remote;
		}

		private async SocketConnection connect_upstream(string proxy_uri) throws Error
		{
			var parsed = Uri.parse(proxy_uri, UriFlags.NONE);
			var up_host = parsed.get_host() ?? "";
			if (up_host == "") {
				throw new IOError.FAILED("upstream missing host: %s", proxy_uri);
			}
			var up_port = parsed.get_port();
			if (up_port < 0) {
				up_port = 80;
			}
			return yield new SocketClient().connect_to_host_async(up_host, (uint16) up_port, null);
		}

	}

}
