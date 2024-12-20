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
#include <algorithm>

#include "chat_overlay.h"
#include "drawable_mgr.h"
#include "player.h"
#include "baseui.h"
#include "game_message.h"
#include "output.h"
#include "input_buttons.h"
#include "cache.h"

ChatOverlay::ChatOverlay() : Drawable(Priority_Overlay, Drawable::Flags::Global | Drawable::Flags::Screenspace)
{

}

void ChatOverlay::Draw(Bitmap& dst) {
	if (!IsAnyMessageVisible() && !show_all) return;

	if (!dirty) {
		dst.Blit(ox, oy, *bitmap, bitmap->GetRect(), 255);
		return;
	}

	bitmap->Clear();

	int y_end = DisplayUi->GetScreenSurfaceRect().height;
	

	int i = 0;
	// we're doing the inverse of MessageOverlay: draw from the bottom up
	int text_height = 12; // Font::NameText()->vGetSize('T').height;
	int half_screen = y_end / 2 / text_height;

	bool unlocked_start = scroll < half_screen;
	bool unlocked_end = scroll > messages.size() - half_screen;
	int last_sticky = std::max(0, (int)messages.size() - half_screen * 2);
	// we want half a screen before first sticky and half a screen after the last sticky.
	int viewport = std::clamp(scroll - half_screen, 0, std::min(scroll + half_screen, last_sticky));

	std::vector<std::string> lines;
	int lidx = 0;
	const auto& system = *Cache::SystemOrBlack();
	constexpr Color offwhite = Color(200, 200, 200, 255);

	for (auto message = messages.rbegin() + viewport; message != messages.rend(); ++message) {
		if (!message->hidden || show_all) {
			auto& bg = !show_all ? black :
				unlocked_start ? (i == scroll ? grey : black) :
				unlocked_end ? (last_sticky + i == scroll ? grey : black) :
				i == half_screen ? grey : black;

			// figure out how many lines we need to print
			std::string temp = fmt::format("[{}] {}", message->sender, message->text);
			bool first = true;
			Game_Message::WordWrap(
				temp,
				bitmap->width(),
				[&](StringView line) {
					if (first) {
						first = false;
						lines.emplace_back(line.substr(1 + message->sender.size() + 2));
					} else
						lines.emplace_back(line);
				}, *Font::NameText());
			for (auto line = lines.rbegin(); line != lines.rend(); ++line) {
				// semitransparent bg
				int y = y_end - (lidx + 1) * text_height;
				bitmap->Blit(0, y, *bg, bg->GetRect(), 200);

				int offset = 2;
				if (std::next(line) == lines.rend()) {
					// draw the username
					offset += Text::Draw(*bitmap, offset, y, *Font::NameText(), offwhite, "[").x;
					offset += Text::Draw(*bitmap, offset, y, *Font::NameText(), *message->system, 0, message->sender).x;
					offset += Text::Draw(*bitmap, offset, y, *Font::NameText(), offwhite, "] ").x;
				}
				Text::Draw(*bitmap, offset, y, *Font::NameText(), offwhite, *line);
				++lidx;
				if (lidx * text_height > y_end) break;
			}
			++i;
			lines.clear();
		}
		if (!show_all && i > message_max_minimized) break;
		if (lidx * text_height > y_end) break;
	}

	dst.Blit(ox, oy, *bitmap, bitmap->GetRect(), 255);
	dirty = false;
}

void ChatOverlay::AddMessage(std::string_view message, std::string_view sender, std::string_view system) {
	if (message.empty()) {
		return;
	}
	counter = 0;

	auto sysref = system.empty() ? Cache::SystemOrBlack() : Cache::System(system.data());

	messages.emplace_back(std::string(message), std::string(sender), sysref);

	while (messages.size() > message_max) {
		messages.pop_front();
	}

	dirty = true;
}

void ChatOverlay::Update() {
	if (!DisplayUi) {
		return;
	}

	if (!bitmap) {
		OnResolutionChange();
	}


	if (show_all) {
		static int delta;
		if (Input::IsTriggered(Input::InputButton::SCROLL_DOWN) ||
			Input::IsTriggered(Input::InputButton::PAGE_DOWN)) {
			delta = 5;
			DoScroll(-1);
		}
		else if (Input::IsRepeated(Input::InputButton::PAGE_DOWN)) {
			DoScroll(std::clamp(--delta / 5, -4, -1));
		}
		if (Input::IsTriggered(Input::InputButton::SCROLL_UP) ||
			Input::IsTriggered(Input::InputButton::PAGE_UP)) {
			delta = -5;
			DoScroll(1);
		}
		else if (Input::IsRepeated(Input::InputButton::PAGE_UP)) {
			DoScroll(std::clamp(++delta / 5, 1, 4));
		}
	}


	if (IsAnyMessageVisible()) {
		if (++counter > 450) {
			counter = 0;
			for (auto& message : messages) {
				message.hidden = true;
			}
			dirty = true;
		}
	}
}

void ChatOverlay::SetShowAll(bool show_all) {
	this->show_all = show_all;
	dirty = true;
}

void ChatOverlay::DoScroll(int increase) {
	scroll += increase;
	scroll = (scroll + messages.size()) % messages.size();
	dirty = true;
}

void ChatOverlay::OnResolutionChange() {
	if (!bitmap) {
		DrawableMgr::Register(this);
	}

	int text_height = 12; // Font::NameText()->vGetSize('T').height;
	Rect rect = DisplayUi->GetScreenSurfaceRect();
	int width = rect.width;
	black = Bitmap::Create(width, text_height, Color(0, 0, 0, 255));
	grey = Bitmap::Create(width, text_height, Color(100, 100, 100, 255));
	bitmap = Bitmap::Create(width, text_height * message_max, true);
	dirty = true;
}

bool ChatOverlay::IsAnyMessageVisible() const {
	return std::any_of(messages.cbegin(), messages.cend(), [](const ChatOverlayMessage& m) { return !m.hidden; });
}

