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

ChatOverlay::ChatOverlay() : Drawable(Priority_Overlay, Drawable::Flags::Global)
{

}

void ChatOverlay::Draw(Bitmap& dst) {
	dst.Blit(ox, oy, *bitmap, bitmap->GetRect(), 255);
	if (!dirty) return;

	bitmap->Clear();

	constexpr int y_end = 240;
	

	int i = 0;
	// we're doing the inverse of MessageOverlay: draw from the bottom up
	int text_height = 12; // Font::NameText()->vGetSize('T').height;
	int half_screen = y_end / 2 / text_height;

	bool unlocked_start = scroll < half_screen;
	bool unlocked_end = scroll > messages.size() - half_screen;
	int last_sticky = (int)messages.size() - half_screen * 2;
	// we want half a screen before first sticky and half a screen after the last sticky.
	int viewport = std::clamp(scroll - half_screen, 0, std::min(scroll + half_screen, last_sticky));

	for (auto message = messages.rbegin() + viewport; message != messages.rend(); ++message) {
		if (!message->hidden || show_all) {
			auto& bg = !show_all ? black :
				unlocked_start ? (i == scroll ? grey : black) :
				unlocked_end ? (last_sticky + i == scroll ? grey : black) :
				i == half_screen ? grey : black;

			bitmap->Blit(0, y_end - (i + 1) * text_height, *bg, bg->GetRect(), 128);
			constexpr Color white = Color(255, 255, 255, 255);
			Text::Draw(*bitmap, 2, y_end - (i + 1) * text_height, *Font::NameText(), white, message->text);
			++i;
		}
		if (!show_all && i > message_max_minimized) break;
		if (i * text_height > y_end) break;
	}

	dirty = false;
}

void ChatOverlay::AddMessage(const std::string& message) {
	if (message.empty()) {
		return;
	}

	Game_Message::WordWrap(
		message,
		Player::screen_width,
		[&](StringView line) {
			messages.emplace_back(std::string(line));
		}, *Font::NameText()
	);

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
		if (Input::IsTriggered(Input::InputButton::SCROLL_DOWN) ||
			Input::IsTriggered(Input::InputButton::DOWN) ||
			Input::IsRepeated(Input::InputButton::DOWN)) {
			DoScroll(-1);
		}
		if (Input::IsTriggered(Input::InputButton::SCROLL_UP) ||
			Input::IsTriggered(Input::InputButton::UP) ||
			Input::IsRepeated(Input::InputButton::UP)) {
			DoScroll(1);
		}
		if (Input::IsTriggered(Input::InputButton::PAGE_DOWN)) {
			scroll = 0;
			dirty = true;
		}
		if (Input::IsTriggered(Input::InputButton::PAGE_UP)) {
			scroll = messages.size() - 1;
			dirty = true;
		}
	}


	if (++counter > 150) {
		counter = 0;
		for (auto& message : messages) {
			if (!message.hidden) {
				message.hidden = true;
				break;
			}
		}
		dirty = true;
	}
}

void ChatOverlay::SetShowAll(bool show_all) {
	Output::Warning("SetShowAll");
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
	black = Bitmap::Create(Player::screen_width, text_height, Color(0, 0, 0, 255));
	grey = Bitmap::Create(Player::screen_width, text_height, Color(100, 100, 100, 255));
	bitmap = Bitmap::Create(Player::screen_width, text_height * message_max, true);
}

ChatOverlayMessage::ChatOverlayMessage(std::string text) :
	text(std::move(text)) {}
