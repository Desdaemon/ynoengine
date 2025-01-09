#ifndef EP_YNO_CONNECTION_H
#define EP_YNO_CONNECTION_H

#ifndef EMSCRIPTEN
#  include <libwebsockets.h>
#  include <thread>
#  include <atomic>
#  include <mutex>
#endif

#include "connection.h"

class YNOConnection : public Multiplayer::Connection {
public:
	const static size_t MAX_QUEUE_SIZE;

	YNOConnection();
	YNOConnection(YNOConnection&&);
	YNOConnection& operator=(YNOConnection&&);
	~YNOConnection();

	bool need_header = true;

	void Open(std::string_view uri) override;
	void Send(std::string_view data) override;
	void FlushQueue() override;
	void Close() override;
protected:
	struct IMPL;
	std::unique_ptr<IMPL> impl;
};

#ifndef EMSCRIPTEN
class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
public:
	using Callback = std::function<void(void*)>;
	using DisconnectCallback = std::function<void(bool, void*)>;
	using MessageCallback = std::function<void(const std::string&, void*)>;

	WebSocketClient(const std::string& url);
	~WebSocketClient();

	inline void RegisterOnConnect(Callback callback) { on_connect_ = callback; }
	inline void RegisterOnMessage(MessageCallback callback) { on_message_ = callback; }
	inline void RegisterOnDisconnect(DisconnectCallback callback) { on_disconnect_ = callback; }

	void Start();
	void Stop();
	inline void SetUserData(void* userdata) noexcept { userdata_ = userdata; }
	void Send(std::string_view data);
	inline bool Empty() const noexcept { return buffer_.empty(); }
	inline bool Ready() const noexcept { return running_ && ready_; }
	inline void RequestFlush() {
		lws_callback_on_writable(wsi_);
	}
	static int CallbackFunction(struct lws* wsi, enum lws_callback_reasons reason,
		void* user, void* in, size_t len);
private:

	static inline struct lws_protocols protocols_[] = {
		{"binary", CallbackFunction, 0, 65536, 0, nullptr, 0},
		{nullptr, nullptr}  // terminator
	};

	std::string url_;
	struct lws_context* context_;
	void* userdata_;
	struct lws* wsi_;
	std::atomic<bool> running_;
	std::atomic<bool> ready_;
	std::mutex bmutex_;
	std::vector<unsigned char> buffer_{};

	Callback on_connect_;
	MessageCallback on_message_;
	DisconnectCallback on_disconnect_;
};
#endif

#endif
