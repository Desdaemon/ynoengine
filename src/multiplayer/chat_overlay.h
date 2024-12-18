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

#include "drawable.h"
#include "bitmap.h"
#include "memory_management.h"

//using ChatOverlayMessage = std::string;
class ChatOverlayMessage {
public:
	ChatOverlayMessage(std::string text);
	std::string text;
	bool hidden = false;
};

class ChatOverlay : public Drawable {
public:
	ChatOverlay();
	void Draw(Bitmap& dst) override;
	void Update();
	void AddMessage(const std::string& msg);
	void OnResolutionChange();
	void SetShowAll(bool show_all);
	inline void SetShowAll() { SetShowAll(!show_all); }
	inline bool ShowingAll() const noexcept { return show_all;  }
	void DoScroll(int increase);
private:
	bool dirty = false;
	bool show_all = false;
	int ox = 0;
	int oy = 0;

	int message_max = 100;
	int message_max_minimized = 4;
	int counter = 0;
	int scroll = 0;

	BitmapRef bitmap, black, grey;

	std::deque<ChatOverlayMessage> messages;
};

#endif
