#include "yno_connection.h"
#include "multiplayer/packet.h"
#include "output.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <cctype>
#ifdef __EMSCRIPTEN__
#  include <emscripten/websocket.h>
#else
#  include <libwebsockets.h>
#  include <thread>
#  include <uv.h>
#  include <cpr/cpr.h>
#  if defined(_WIN32)
#    include <synchapi.h>
#  elif defined(__linux__)
#    include <linux/futex.h>
#    include <sys/syscall.h>
#  else
#    error No futex implementation available for this platform
#  endif
#endif
#include "../external/TinySHA1.hpp"

struct YNOConnection::IMPL {
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T socket;
#else
	std::shared_ptr<WebSocketClient> _wsclient;
	WebSocketClient* GetWsClient() { return _wsclient.get(); };
#endif

	uint32_t msg_count = 0;
	bool closed = true;

	static bool onopen_common(void* userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(true);
		_this->DispatchSystem(SystemMessage::OPEN);
		return true;
	}
	static bool onclose_common(bool exit, void* userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(false);
		_this->DispatchSystem(
			exit ?
			SystemMessage::EXIT :
			SystemMessage::CLOSE
		);
		return true;
	}
	static bool onmessage_common(const std::string& cstr, void* userData, bool isText) {
		auto _this = static_cast<YNOConnection*>(userData);
		// IMPORTANT!! numBytes is always one byte larger than the actual length
		// so the actual length is numBytes - 1

		// NOTE: that extra byte is just in text mode, and it does not exist in binary mode
		if (isText) {
			return false;
		}
		std::vector<std::string_view> mstrs = Split(cstr, Multiplayer::Packet::MSG_DELIM);
		for (auto& mstr : mstrs) {
			auto p = mstr.find(Multiplayer::Packet::PARAM_DELIM);
			if (p == mstr.npos) {
				/*
				Usually npos is the maximum value of size_t.
				Adding to it is undefined behavior.
				If it returns end iterator instead of npos, the if statement is
				duplicated code because the statement in else clause will handle it.
				*/
				_this->Dispatch(mstr);
			}
			else {
				auto namestr = mstr.substr(0, p);
				auto argstr = mstr.substr(p + Multiplayer::Packet::PARAM_DELIM.size());
				_this->Dispatch(namestr, Split(argstr));
			}
		}
		return true;
	}

#ifdef __EMSCRIPTEN__
	static bool onopen(int eventType, const EmscriptenWebSocketOpenEvent *event, void *userData) {
		return onopen_common(userData);
	}
	static bool onclose(int eventType, const EmscriptenWebSocketCloseEvent *event, void *userData) {
		return onclose_common(event->code == 1028, event->data, event->numBytes, userData);
	}
	static bool onmessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData) {
		return onmessage_common(std::string_view(reinterpret_cast<const char*>(event->data, event->numBytes)), userData, event->isText);
	}
	static void set_callbacks(int socket, void* userData) {
		emscripten_websocket_set_onopen_callback(socket, userData, onopen);
		emscripten_websocket_set_onclose_callback(socket, userData, onclose);
		emscripten_websocket_set_onmessage_callback(socket, userData, onmessage);
	}
#else
	void set_callbacks(void* userData) {
		assert(_wsclient && "wsclient not initialized");
		_wsclient->SetUserData(userData);
		_wsclient->RegisterOnConnect(onopen_common);
		_wsclient->RegisterOnDisconnect(onclose_common);
		_wsclient->RegisterOnMessage([](const std::string& str, void* userdata) { return onmessage_common(str, userdata, false); });
	}
	void create_client(const std::string& url) {
		//if (_wsclient) _wsclient->Stop();
		_wsclient.reset(new WebSocketClient(url));
		//_wsclient->Start();
	}
	void start() {
		_wsclient->Start();
	}
#endif
};

namespace {
	lws_context* default_context;

	static struct lws_protocols protocols_[] = {
		{"binary", WebSocketClient::CallbackFunction, 0, 65536, 0, nullptr, 0},
		{nullptr, nullptr}  // terminator
	};

	lws_context* GetDefaultContext() {
		if (default_context)
			return default_context;
		struct lws_context_creation_info info = {};
		info.options = 0
			| LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT
			| LWS_SERVER_OPTION_LIBUV
			;
		uv_loop_t* loop = uv_default_loop();
		info.protocols = protocols_;
		info.foreign_loops = (void **)&loop;
		info.port = CONTEXT_PORT_NO_LISTEN;
		info.fd_limit_per_thread = 1 + 1 + 1;

		default_context = lws_create_context(&info);
		if (!default_context) {
			Output::ErrorStr("Failed to create LWS context");
		}
		return default_context;
	}
}

#ifndef EMSCRIPTEN
WebSocketClient::WebSocketClient(const std::string& url) : url_(url), context_(nullptr), wsi_(nullptr), running_(false), userdata_(nullptr) {
}

WebSocketClient::~WebSocketClient() {
	Stop();
}

void WebSocketClient::Start() {
	if (running_) return;

	context_ = GetDefaultContext();

	const char *prot, *addr, *path;
	int port;
	if (lws_parse_uri(url_.data(), &prot, &addr, &port, &path)) {
		Output::Error("Invalid wsurl: {}", url_);
	}

	lws_client_connect_info ccinfo{};
	ccinfo.context = context_;
	ccinfo.address = addr;
	ccinfo.port = port;
	ccinfo.path = path;
	ccinfo.host = ccinfo.address;
	ccinfo.origin = lws_canonical_hostname(context_);
	ccinfo.ssl_connection = LCCSCF_USE_SSL;
	ccinfo.protocol = protocols_[0].name;
	ccinfo.opaque_user_data = this;

	wsi_ = lws_client_connect_via_info(&ccinfo);
	if (!wsi_) {
		lws_context_destroy(context_);
		Output::Debug("Failed to connect to the WebSocket server");
		return;
	}

	running_.store(true, std::memory_order_acquire);
}

void WebSocketClient::Stop() {
	if (!running_) return;
	Output::Debug("Stopping {}", url_);
	running_.store(false, std::memory_order_release);
	lws_set_timeout(wsi_, PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, LWS_TO_KILL_ASYNC);
	//lws_cancel_service(lws_get_context(wsi_));
}

void WebSocketClient::Send(std::string_view data) {
	std::lock_guard _guard(bmutex_);
	buffer_.insert(buffer_.end(), data.begin(), data.end());
}

int WebSocketClient::CallbackFunction(struct lws* wsi, enum lws_callback_reasons reason,
	void* user, void* in, size_t len) {

	WebSocketClient* client = reinterpret_cast<WebSocketClient*>(lws_get_opaque_user_data(wsi));
//	if (client && !client->running_ && client->ready_) {
//#define $print(x) case (x): Output::Debug("{}: " #x, client->url_); break;
//		switch (reason) {
//		$print(LWS_CALLBACK_CLIENT_ESTABLISHED)
//		$print(LWS_CALLBACK_CLIENT_WRITEABLE)
//		$print(LWS_CALLBACK_CLIENT_RECEIVE)
//		$print(LWS_CALLBACK_WS_PEER_INITIATED_CLOSE)
//		$print(LWS_CALLBACK_CLIENT_CLOSED)
//		$print(LWS_CALLBACK_CLIENT_CONNECTION_ERROR)
//		$print(LWS_CALLBACK_EVENT_WAIT_CANCELLED)
//		$print(LWS_CALLBACK_LOCK_POLL)
//		$print(LWS_CALLBACK_UNLOCK_POLL)
//		$print(LWS_CALLBACK_CHANGE_MODE_POLL_FD)
//		$print(LWS_CALLBACK_DEL_POLL_FD)
//		default:
//			Output::Debug("Unknown LWS event {}", (int)reason);
//		}
//#undef $print
//	}

	switch (reason) {
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		if (!client->running_ && client->ready_) return -1;
		client->ready_ = true;
		if (client->on_connect_) client->on_connect_(client->userdata_);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE: {
		if (!client->running_ && client->ready_) return -1;
		if (!client->buffer_.empty()) {
			size_t current_size = client->buffer_.size();
			std::vector<unsigned char> buffer;
			{
				std::lock_guard _guard(client->bmutex_);
				buffer.resize(LWS_PRE + current_size);
				buffer.insert(buffer.begin() + LWS_PRE, client->buffer_.begin(), client->buffer_.end());
				client->buffer_.clear();
			}

			if (lws_write(wsi, &buffer[LWS_PRE], current_size, LWS_WRITE_BINARY) < current_size) {
				return -1;
			}
		}
	} break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		if (!client->running_ && client->ready_) return -1;
		if (client->on_message_) {
			std::string message(reinterpret_cast<const char*>(in), len);
			client->on_message_(message, client->userdata_);
		}
		break;

	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
		client->running_.store(false, std::memory_order_release);
		if (client->on_disconnect_) {
			lcf::Span<unsigned char> close_span(
				lws_get_close_payload(wsi),
				lws_get_close_length(wsi)
			);
			bool exit = false;
			if (close_span.size() >= 2) {
				// close reason in network order (bigendian)
				int16_t close_reason = ((int16_t)(close_span[1]) << 8) | close_span[0];
				exit = close_reason == 1028;
			}
			client->on_disconnect_(exit, client->userdata_);
		}
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		client->running_.store(false, std::memory_order_release);
		if (client->on_disconnect_) {
			client->on_disconnect_(false, client->userdata_);
		}
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		Output::Warning("LWS error: {} ({})", std::string((const char*)in, len), client->url_);
		client->running_.store(false, std::memory_order_release);
		if (client->on_disconnect_) {
			client->on_disconnect_(false, client->userdata_);
		}
		break;

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		if (client && client->ready_) {
			Output::Debug("{}: event wait cancelled", client->url_);
			client->running_.store(false, std::memory_order_release);
			if (client->on_disconnect_) {
				client->on_disconnect_(false, client->userdata_);
			}
		}
		break;

	//case LWS_CALLBACK_LOCK_POLL:
	//case LWS_CALLBACK_UNLOCK_POLL:
	//case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
	//case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	//case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
	//case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
	//	break;

	//default:
	//	Output::Debug("Unhandled LWS event {}", (int)reason);
	//	break;
	}

	return 0;
}
#endif

const size_t YNOConnection::MAX_QUEUE_SIZE{ 4088 };



YNOConnection::YNOConnection() : impl(new IMPL) {
	impl->closed = true;
}

YNOConnection::YNOConnection(YNOConnection&& o)
	: Connection(std::move(o)), impl(std::move(o.impl)) {
#ifdef EMSCRIPTEN
	IMPL::set_callbacks(impl->socket, this);
#else
	impl->set_callbacks(this);
#endif
}
YNOConnection& YNOConnection::operator=(YNOConnection&& o) {
	Connection::operator=(std::move(o));
	if (this != &o) {
		Close();
		impl = std::move(o.impl);
#ifdef EMSCRIPTEN
		IMPL::set_callbacks(impl->socket, this);
#else
		impl->set_callbacks(this);
#endif
	}
	return *this;
}

YNOConnection::~YNOConnection() {
	if (impl)
		Close();
}

void YNOConnection::Open(std::string_view uri) {
	std::string s {uri};
#ifdef __EMSCRIPTEN__
	if (!impl->closed) {
		Close();
	}
	EmscriptenWebSocketCreateAttributes ws_attrs = {
		s.data(),
		"binary",
		EM_TRUE,
	};
	impl->socket = emscripten_websocket_new(&ws_attrs);
	impl->closed = false;
	IMPL::set_callbacks(impl->socket, this);
#else
	if (!impl->_wsclient) {
		impl->create_client(s);
		impl->closed = false;
		impl->set_callbacks(this);
		impl->start();
		return;
	}
	uv_work_t* task = new uv_work_t{};
	using work_data_t = std::pair<YNOConnection*, std::string>;
	task->data = new work_data_t{this, std::move(s)};

	uv_queue_work(uv_default_loop(), task,
	[](uv_work_t* task) {
		auto& [self, s] = *static_cast<work_data_t*>(task->data);
		if (!self->IsConnected()) return;
		self->Close();
		bool expected_value = false;
#if defined(_WIN32)
		if (!WaitOnAddress(self->ConnectedFutex(), &expected_value, sizeof(expected_value), INFINITE))
			Output::Debug("Failed to wait for previous session to close: {}", GetLastError());
#else
		if (syscall(SYS_futex, self->ConnectedFutex(), FUTEX_WAKE, expected_value, nullptr, nullptr, 0)) {
			Output::Debug("Failed to wait for previous session to close: {}", strerror(errno));
		}
#endif
	},
	[](uv_work_t* task, int) {
		auto& [self, s] = *static_cast<work_data_t*>(task->data);
		self->impl->create_client(s);
		self->impl->closed = false;
		self->impl->set_callbacks(self);
		self->impl->start();

		delete (work_data_t*)task->data;
		delete task;
	});
#endif
}

void YNOConnection::Close() {
	Multiplayer::Connection::Close();
	if (impl->closed)
		return;
	impl->closed = true;
#ifdef __EMSCRIPTEN__
	// strange bug:
	// calling with (impl->socket, 1005, "any reason") raises exceptions
	// might be an emscripten bug
	emscripten_websocket_close(impl->socket, 0, nullptr);
	emscripten_websocket_delete(impl->socket);
#else
	// handled by create_client
	//impl->_wsclient->Stop();
#endif
}

template<typename T>
std::string_view as_bytes(const T& v) {
	static_assert(sizeof(v) % 2 == 0, "Unsupported numeric type");
	return std::string_view(
		reinterpret_cast<const char*>(&v),
		sizeof(v)
	);
}

static bool is_big_endian() {
	const uint16_t n = 0x1;
	return reinterpret_cast<const char*>(&n)[0] == 0x00;
}

std::string reverse_endian(std::string src) {
	if (src.size() % 2 == 1)
		return src;

	size_t it1{0}, it2{src.size() - 1}, itend{src.size() / 2};
	while (it1 != itend) {
		std::swap(src[it1], src[it2]);
		++it1;
		--it2;
	}
	return src;
}

template<typename T>
std::string as_big_endian_bytes(T v) {
	auto r = as_bytes<T>(v);
	std::string sr{r.data(), r.size()};
	if (is_big_endian())
		return sr;
	else
		return reverse_endian(sr);
}

const unsigned char psk[] = {};

std::string calculate_header(uint32_t key, uint32_t count, std::string_view msg) {
	std::string hashmsg{as_bytes(psk)};
	hashmsg += as_big_endian_bytes(key);
	hashmsg += as_big_endian_bytes(count);
	hashmsg += msg;

	sha1::SHA1 digest;
	uint32_t digest_result[5];
	digest.processBytes(hashmsg.data(), hashmsg.size());
	digest.getDigest(digest_result);

	std::string r{as_big_endian_bytes(digest_result[0])};
	r += as_big_endian_bytes(count);
	return r;
}

void YNOConnection::Send(std::string_view data) {
	if (!IsConnected())
		return;
	unsigned short ready;
#ifdef __EMSCRIPTEN__
	emscripten_websocket_get_ready_state(impl->socket, &ready);
#else
	auto wsclient = impl->GetWsClient();
	ready = wsclient->Ready();
#endif
	if (ready) { // OPEN
		++impl->msg_count;
		std::string sendmsg;
		if (need_header) {
			sendmsg = calculate_header(GetKey(), impl->msg_count, data);
			sendmsg += data;
		}
		else sendmsg = data;
#ifdef __EMSCRIPTEN__
		emscripten_websocket_send_binary(impl->socket, sendmsg.data(), sendmsg.size());
#else
		wsclient->Send(sendmsg);
		wsclient->RequestFlush();
#endif
	}
}

void YNOConnection::FlushQueue() {
	auto namecmp = [] (std::string_view v, bool include) {
		return (v != "sr") == include;
	};

	bool include = false;
	while (!m_queue.empty()) {
		std::string bulk;
		while (!m_queue.empty()) {
			auto& e = m_queue.front();
			if (namecmp(e->GetName(), include))
				break;
			auto data = e->ToBytes();
			// send before overflow
			if (bulk.size() + data.size() > MAX_QUEUE_SIZE) {
				Send(bulk);
				bulk.clear();
			}
			if (!bulk.empty())
				bulk += Multiplayer::Packet::MSG_DELIM;
			bulk += data;
			m_queue.pop();
		}
		if (!bulk.empty()) {
			Send(bulk);
#ifndef EMSCRIPTEN
			// yield early for this batch, since otherwise the switch room command
			// has no time to register
			return;
#endif
		}
		include = !include;
	}
}
