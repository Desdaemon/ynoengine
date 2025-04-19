/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EP_MP_CHAT_OVERLAY_H
#define EP_MP_CHAT_OVERLAY_H

#include <string>
#include <deque>
#include <lcf/span.h>
#include <chrono>
#include <uv.h>
#include <mutex>

#include "drawable.h"
#include "bitmap.h"
#include "memory_management.h"
#include "async_handler.h"
#include "game_clock.h"
#include "input.h"
#include "point.h"

enum ChatComponents {
	Box,
	String,
	Emoji,
	Screenshot,
	// no associated components
	Header,
	END
};

template <ChatComponents>
struct ChatComponentsMap { using type = void; };

class ChatOverlay;

class ChatComponent : public std::enable_shared_from_this<ChatComponent> {
protected:
	const ChatComponents runtime_type;
	ChatComponent(ChatComponents type = ChatComponents::Box) noexcept :
		static_dim(0, 0), runtime_type(type) { }
public:
	const Point static_dim;
	std::function<Point()> sizer;

	ChatComponent(int width, int height, ChatComponents type = ChatComponents::Box) noexcept :
		static_dim(width, height), runtime_type(type) {}
	template <typename Sizer>
	ChatComponent(Sizer&& sizer, ChatComponents type = ChatComponents::Box) noexcept :
		runtime_type(type), sizer(std::forward<Sizer>(sizer)), static_dim(0, 0) {}
	virtual Point Draw(Bitmap& dest) { return static_dim; }
	template <ChatComponents Wanted>
	typename ChatComponentsMap<Wanted>::type* Downcast();
	Point GetSize() const;
};

inline Point ChatComponent::GetSize() const {
	return sizer ? sizer() : static_dim;
}

class ChatOverlayMessage {
public:
	ChatOverlayMessage(
		ChatOverlay* parent, std::string text, std::string sender, std::string sender_uuid, std::string system, std::string badge, bool account, bool global, int rank);
	std::string text_orig;
	std::vector<std::shared_ptr<ChatComponent>> text;
	std::string sender;
	std::string sender_uuid;
	std::string system;
	BitmapRef system_bm;
	BitmapRef badge;
	bool hidden = false;
	ChatOverlay* parent;
	bool account;
	bool global;
	int rank;

	std::vector<std::shared_ptr<ChatComponent>> Convert(ChatOverlay* parent, bool has_badge) const;
	void SetOnInteract(std::function<void(ChatOverlayMessage&)> on_interact);
private:
	FileRequestBinding badge_request;
	FileRequestBinding system_request;

	std::function<void(ChatOverlayMessage&)> OnInteract;
};

inline void ChatOverlayMessage::SetOnInteract(std::function<void(ChatOverlayMessage&)> on_interact) { OnInteract = on_interact; }

class ChatOverlay : public Drawable {
public:
	ChatOverlay();
	void Draw(Bitmap& dst) override;
	void OnResolutionChange() override;

	void Update();
	/** Run by Scene_Overlay on behalf of this component. */
	void UpdateScene();
	ChatOverlayMessage& AddMessage(
		std::string_view msg, std::string_view sender, std::string_view sender_uuid, std::string_view system = "", std::string_view badge = "",
		bool account = true, bool global = true, int rank = 0);
	void SetShowAll(bool show_all);
	inline void SetShowAll() { SetShowAll(!show_all); }
	inline bool ShowingAll() const noexcept { return show_all;  }
	void DoScroll(int increase);
	void MarkDirty() noexcept;
	void AddSystemMessage(std::string_view string);
private:
	bool IsAnyMessageVisible() const;

	bool dirty = false;
	bool show_all = false;
	int ox = 0;
	int oy = 0;

	int message_max = 200;
	int counter = 0;
	int scroll = 0;
	int scrollbar_width = 0;

	BitmapRef bitmap;
	constexpr static Color black{ 0, 0, 0, 255 };
	constexpr static Color grey{ 80, 80, 80, 255 };
	constexpr static Color scrollbar{ 255, 255, 255, 255 };

	std::u32string input;

	std::deque<ChatOverlayMessage> messages;

	struct KeyTimer {
	public:
		const Input::Keys::InputKey raw_key;
		const std::chrono::milliseconds delay;
		const std::chrono::milliseconds rate;
		Game_Clock::time_point last_triggered;
		Game_Clock::time_point last_repeated;
	};

	enum class CustomKeyTimers : char {
		backspace,
		END,
	};
	std::array<KeyTimer, (int)CustomKeyTimers::END> key_timers {
		{Input::Keys::BACKSPACE, std::chrono::milliseconds{300}, std::chrono::milliseconds{24}}
	};

	bool IsTriggeredOrRepeating(CustomKeyTimers key);
};

inline void ChatOverlay::MarkDirty() noexcept { dirty = true; }

class ChatString : public ChatComponent {
	constexpr static Color default_color = Color(200, 200, 200, 255);
public:
	std::string string;
	Color color;

	ChatString(std::string_view other, Color color = default_color) : ChatComponent(0, 0, ChatComponents::String),
		string(ToString(other)), color(color) {}
};

class ChatEmoji : public ChatComponent {
public:
	std::string emoji;
	BitmapRef bitmap = nullptr;
	ChatOverlay* parent = nullptr;
	FileRequestBinding request = nullptr;

	ChatEmoji(ChatOverlay* parent, int width, int height, std::string emoji);
	void RequestBitmap(ChatOverlay* parent);
	void UpdateAnimation();
	static std::shared_ptr<ChatEmoji> GetOrCreate(const std::string& emojiKey, ChatOverlay* parent);

	inline bool HasAnimation() const noexcept {
		return frames.size() > 1;
	}
private:
	std::vector<std::shared_ptr<Bitmap>> frames; // Store frames for GIFs
	std::vector<int> frameDelays; // Store delays for each frame in milliseconds
	int currentFrame = 0; // Current frame index for animated GIFs
	int loopLength = 0;
	std::chrono::steady_clock::time_point lastFrameTime; // Time of the last frame update

	void DecodeGif(const std::string& filePath);
	void DecodeWebP(const std::string& filePath);
};

class ChatScreenshot : public ChatComponent {
public:
	bool spoiler;
	bool temp;
	std::string uuid;
	std::string id;
	BitmapRef bitmap = nullptr;
	FileRequestBinding request = nullptr;
	ChatOverlay* parent = nullptr;

	ChatScreenshot(ChatOverlay* parent, std::string uuid, std::string id, bool temp, bool spoiler);
	static Point sizer();
private:
	uv_work_t task{};
};


template<>
struct ChatComponentsMap<ChatComponents::Box> { using type = ChatComponent; };
template<>
struct ChatComponentsMap<ChatComponents::String> { using type = ChatString; };
template<>
struct ChatComponentsMap<ChatComponents::Emoji> { using type = ChatEmoji; };
template<>
struct ChatComponentsMap<ChatComponents::Header> { using type = ChatComponent; };
template<>
struct ChatComponentsMap<ChatComponents::Screenshot> { using type = ChatScreenshot; };

template<ChatComponents Wanted>
inline typename ChatComponentsMap<Wanted>::type* ChatComponent::Downcast() {
	if (runtime_type == Wanted) {
		return static_cast<typename ChatComponentsMap<Wanted>::type*>(this);
	}
	if (Wanted == ChatComponents::Box && runtime_type != ChatComponents::String) {
		// a string doesn't have an intrinsic size
		return static_cast<typename ChatComponentsMap<Wanted>::type*>(this);
	}
	return nullptr;
}

#endif
