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

#include "drawable.h"
#include "bitmap.h"
#include "memory_management.h"
#include "async_handler.h"

enum ChatComponents {
	Box,
	String,
	Emoji,
	END
};

template <ChatComponents>
struct ChatComponentsMap { using type = void; };

class ChatOverlay;

class ChatComponent : public Point, public std::enable_shared_from_this<ChatComponent> {
protected:
	const ChatComponents runtime_type;
	ChatComponent(ChatComponents type = ChatComponents::Box) noexcept :
		Point(0, 0), runtime_type(type) {}
public:
	// fields for children classes

	ChatComponent(int width, int height, ChatComponents type = ChatComponents::Box) noexcept :
		Point(width, height), runtime_type(type) {}
	virtual int Draw(Bitmap& dest) { return x; }
	template <ChatComponents Wanted>
	typename ChatComponentsMap<Wanted>::type* Downcast();
};

class ChatOverlayMessage {
public:
	ChatOverlayMessage(ChatOverlay* parent, std::string text, std::string sender, BitmapRef system, std::string badge);
	std::string text_orig;
	std::vector<std::shared_ptr<ChatComponent>> text;
	std::string sender;
	BitmapRef system;
	BitmapRef badge;
	bool hidden = false;
	ChatOverlay* parent;

	std::vector<std::shared_ptr<ChatComponent>> Convert(ChatOverlay* parent, bool has_badge) const;
private:
	FileRequestBinding badge_request;

	void OnBadgeReady(FileRequestResult*);
};

class ChatOverlay : public Drawable {
public:
	ChatOverlay();
	void Draw(Bitmap& dst) override;
	void Update();
	void UpdateScene();
	void AddMessage(std::string_view msg, std::string_view sender, std::string_view system = "", std::string_view badge = "");
	void OnResolutionChange() override;
	void SetShowAll(bool show_all);
	inline void SetShowAll() { SetShowAll(!show_all); }
	inline bool ShowingAll() const noexcept { return show_all;  }
	void DoScroll(int increase);
	void MarkDirty() noexcept;
private:
	bool IsAnyMessageVisible() const;

	bool dirty = false;
	bool show_all = false;
	int ox = 0;
	int oy = 0;

	int message_max = 200;
	int message_max_minimized = 4;
	int counter = 0;
	int scroll = 0;

	BitmapRef bitmap, black, grey, scrollbar;
	std::string input;

	std::deque<ChatOverlayMessage> messages;
};

inline void ChatOverlay::MarkDirty() noexcept { dirty = true; }

class ChatString : public ChatComponent {
public:
	StringView string;

	ChatString(StringView other) : ChatComponent(0, 0, ChatComponents::String), string(other) {}
	int Draw(Bitmap& dst) override;
};

class ChatEmoji : public ChatComponent {
public:
	std::string emoji;
	BitmapRef bitmap = nullptr;
	ChatOverlay* parent = nullptr;
	FileRequestBinding request = nullptr;

	ChatEmoji(ChatOverlay* parent, int width, int height, std::string emoji);
	int Draw(Bitmap& dest) override;
	void RequestBitmap(ChatOverlay* parent);
};

class ChatScreenshot : public ChatComponent {
public:
	bool spoiler;
	bool temp;
	std::string id;
	BitmapRef bitmap = nullptr;
	FileRequestBinding request = nullptr;
	ChatOverlay* parent = nullptr;

	ChatScreenshot(ChatOverlay* parent, std::string id, bool temp, bool spoiler);
};


template<>
struct ChatComponentsMap<ChatComponents::Box> { using type = ChatComponent; };
template<>
struct ChatComponentsMap<ChatComponents::String> { using type = ChatString; };
template<>
struct ChatComponentsMap<ChatComponents::Emoji> { using type = ChatEmoji; };

template<ChatComponents Wanted>
inline typename ChatComponentsMap<Wanted>::type* ChatComponent::Downcast() {
	if (runtime_type == Wanted || Wanted == ChatComponents::Box) {
		return static_cast<ChatComponentsMap<Wanted>::type*>(this);
	}
	return nullptr;
}

#endif
