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
#include <regex>
#include <tuple>

#include "chat_overlay.h"
#include "drawable_mgr.h"
#include "player.h"
#include "baseui.h"
#include "output.h"
#include "input_buttons.h"
#include "cache.h"
#include "game_message.h"
#include "scene.h"
#include "multiplayer/scene_overlay.h"

namespace {
	int ChatTextHeight() {
		return Font::ChatText()->GetCurrentStyle().size;
	}
}

ChatOverlay::ChatOverlay() : Drawable(Priority_Overlay, Drawable::Flags::Global | Drawable::Flags::Screenspace)
{

}

void ChatOverlay::Draw(Bitmap& dst) {
	if (!IsAnyMessageVisible() && !show_all) return;
	// We don't wanna paint over settings.
	if (Scene::instance && Scene::instance->type == Scene::SceneType::Settings) return;

	if (!dirty) {
		dst.Blit(ox, oy, *bitmap, bitmap->GetRect(), 255);
		return;
	}

	bitmap->Clear();

	Rect screen_rect = DisplayUi->GetScreenSurfaceRect();
	int y_end = screen_rect.height;

	auto& font = *Font::ChatText();
	const int text_height = font.GetCurrentStyle().size;

	int i = 0;
	int half_screen = y_end / 2 / text_height;

	bool unlocked_start = scroll < half_screen;
	bool unlocked_end = scroll > messages.size() - half_screen;
	int last_sticky = std::max(0, (int)messages.size() - half_screen * 2);
	// we want half a screen before first sticky and half a screen after the last sticky.
	int viewport = show_all
		? std::clamp(scroll - half_screen, 0, std::min(scroll + half_screen, last_sticky))
		: 0;

	std::vector<std::vector<std::shared_ptr<ChatComponent>>> lines;
	int lidx = 0;
	const auto& system = *Cache::SystemOrBlack();
	constexpr Color offwhite = Color(200, 200, 200, 255);

	// draw input
	int input_row_offset = 0;
	if (show_all) {
		input_row_offset = 1;
		int offset = 2;
		int y = y_end - (lidx + 1) * text_height;
		bitmap->Blit(0, y, *black, black->GetRect(), 200);
		int prompt_width = Text::Draw(*bitmap, offset, y, font, offwhite, ">").x;
		offset += prompt_width;
		if (!input.empty())
			offset += Text::Draw(*bitmap, offset, y, font, offwhite, input).x;
		// cursor
		bitmap->FillRect({ offset, y + text_height - 3, prompt_width, 3 }, offwhite);
	}

	// we're doing the inverse of MessageOverlay: draw from the bottom up
	for (auto message = messages.rbegin() + viewport; message != messages.rend(); ++message) {
		if (!message->hidden || show_all) {
			auto& bg = !show_all ? black :
				unlocked_start ? (i == scroll ? grey : black) :
				unlocked_end ? (last_sticky + i == scroll ? grey : black) :
				i == half_screen ? grey : black;
			bool highlight = bg == grey;

			bool first = true;
			Game_Message::WordWrap(
				message->text,
				bitmap->width(),
				[&](lcf::Span<std::shared_ptr<ChatComponent>> line) {
					if (first) {	
						first = false;
						// we added the username as a virtual span, so now we remove it.
						line = { line.begin() + 1, line.end() };
					}
					if (!line.empty()) {
						auto& nextline = lines.emplace_back();
						for (auto& span : line) {
							if (auto str = span->Downcast<ChatComponents::String>())
								nextline.emplace_back(std::make_shared<ChatString>(str->string));
							else
								nextline.emplace_back(span);
						}
					}
				}, font);

			for (auto line = lines.rbegin(); line != lines.rend(); ++line) {
				// semitransparent bg
				int y = y_end - (lidx + 1 + input_row_offset) * text_height;
				bitmap->Blit(0, y, *bg, bg->GetRect(), show_all ? highlight ? 240 : 200 : 150);

				int offset = 2;
				if (std::next(line) == lines.rend()) {
					// draw the username
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, "[").x;
					offset += Text::Draw(*bitmap, offset, y, font, *message->system, 0, message->sender).x;
					if (message->badge) {
						// TODO: Handle badge sizes <36
						bitmap->Blit(offset, y, *message->badge, {0, 0, text_height, text_height}, 255);
						offset += text_height;
					}
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, "] ").x;
				}
				for (auto& span : *line) {
					//auto& component = static_cast<ChatComponent&>(span);
					if (auto str = span->Downcast<ChatComponents::String>()) {
						offset += Text::Draw(*bitmap, offset, y, font, offwhite, str->string).x;
					}
					else if (auto emoji = span->Downcast<ChatComponents::Emoji>()) {
						auto dims = *static_cast<Point*>(emoji);
						if (emoji->bitmap) {
							double zoom = emoji->bitmap->GetRect().y / (double)text_height;
							bitmap->StretchBlit({offset, y, text_height, text_height}, *emoji->bitmap, emoji->bitmap->GetRect(), 255);
						}
						else {
							// the emoji is still live, request it now
							emoji->RequestBitmap(this);
							bitmap->FillRect({ offset, y, dims.x, text_height }, offwhite);
						}
						offset += dims.x;
					}
					else if (auto box = span->Downcast<ChatComponents::Box>()) {
						int width = box->x;
						bitmap->FillRect({ offset, y, width, text_height }, offwhite);
						offset += width;
					}
				}
				++lidx;
				if ((lidx + input_row_offset) * text_height > y_end) break;
			}
			++i;
			lines.clear();
		}
		if (!show_all && i > message_max_minimized) break;
		if ((lidx + input_row_offset) * text_height > y_end) break;
	}

	if (show_all && scrollbar) {
		double scrollbar_height = screen_rect.height / (double)text_height / messages.size() * screen_rect.height;
		Rect dims{ 0, 0, (int)ceilf(0.008 * screen_rect.width), (int)ceilf(scrollbar_height) };
		//how far should the scrollbar go
		double scrollbar_extent = scroll / (double)message_max;
		int desired_y = floorf(y_end * (1 - scrollbar_extent)) - dims.height;
		bitmap->BlitFast(bitmap->width() - dims.width, std::clamp(desired_y, 0, y_end - dims.height), *scrollbar, dims, 255);
	}

	dst.Blit(ox, oy, *bitmap, bitmap->GetRect(), 255);
	dirty = false;
}

void ChatOverlay::AddMessage(std::string_view message, std::string_view sender, std::string_view system, std::string_view badge) {
	if (message.empty()) {
		return;
	}
	counter = 0;

	auto sysref = system.empty() ? Cache::SystemOrBlack() : Cache::System(system.data());

	messages.emplace_back(this, std::string(message), std::string(sender), sysref, std::string(badge));

	while (messages.size() > message_max) {
		messages.pop_front();
	}

	if (scroll && (scroll + 1 < messages.size()))
		scroll += 1;

	dirty = true;
}

void ChatOverlay::Update() {
	if (!DisplayUi) {
		return;
	}

	if (!bitmap) {
		OnResolutionChange();
	}

	if (!show_all && IsAnyMessageVisible()) {
		if (++counter > 450) {
			counter = 0;
			for (auto& message : messages) {
				message.hidden = true;
			}
			dirty = true;
		}
	}
}

void ChatOverlay::UpdateScene() {
	if (!show_all) {
		Scene::Pop();
		return;
	}
	static int delta;
	if (
		Input::IsTriggered(Input::InputButton::CHAT_SCROLL_DOWN)) {
		delta = 5;
		DoScroll(-1);
	}
	else if (Input::IsRepeated(Input::InputButton::DOWN)) {
		DoScroll(std::clamp(--delta / 4, -4, -1));
	}

	if (Input::IsPressed(Input::InputButton::SCROLL_UP) ||
		Input::IsTriggered(Input::InputButton::CHAT_SCROLL_UP)) {
		delta = -5;
		DoScroll(1);
	}
	else if (Input::IsRepeated(Input::InputButton::UP)) {
		DoScroll(std::clamp(++delta / 4, 1, 4));
	}

	if (Input::IsPressed(Input::InputButton::SCROLL_DOWN)) {
		DoScroll(-3);
	}
	else if (Input::IsPressed(Input::InputButton::SCROLL_UP)) {
		DoScroll(3);
	}

	constexpr int offset = U'A' - Input::Keys::A;
	constexpr int uppercase = U'a' - U'A';
	bool shift = Input::IsRawKeyPressed(Input::Keys::LSHIFT) || Input::IsRawKeyPressed(Input::Keys::RSHIFT);
	static std::map<Input::Keys::InputKey, char> symbols{
		{Input::Keys::N0, '0'},
		{Input::Keys::N1, '1'},
		{Input::Keys::N2, '2'},
		{Input::Keys::N3, '3'},
		{Input::Keys::N4, '4'},
		{Input::Keys::N5, '5'},
		{Input::Keys::N6, '6'},
		{Input::Keys::N7, '7'},
		{Input::Keys::N8, '8'},
		{Input::Keys::N9, '9'},
		{Input::Keys::SPACE, ' '},
		//{Input::Keys::SEMICOLON, ';'},
	};
	for (int letter = Input::Keys::A; letter <= Input::Keys::Z; ++letter) {
		if (Input::IsRawKeyTriggered((Input::Keys::InputKey)letter)) {
			input.push_back(letter + offset + (shift ? 0 : uppercase));
			dirty = true;
		}
	}
	if (Input::IsRawKeyTriggered(Input::Keys::SEMICOLON)) {
		input.push_back(shift ? ':' : ';');
		dirty = true;
	}
	for (auto& [key, chara] : symbols) {
		if (Input::IsRawKeyTriggered(key)) {
			input.push_back(chara);
			dirty = true;
		}
	}
	if (Input::IsRawKeyPressed(Input::Keys::BACKSPACE) && !input.empty()) {
		input.pop_back();
		dirty = true;
	}
	if (Input::IsRawKeyTriggered(Input::Keys::RETURN)) {
		AddMessage(input, "YNO");
		input.clear();
		dirty = true;
	}
}

void ChatOverlay::SetShowAll(bool show_all) {
	if (this->show_all == show_all) return;
	this->show_all = show_all;
	counter = 0;
	dirty = true;

	if (show_all) {
		bool in_overlay = Scene::instance && Scene::instance->type == Scene::SceneType::ChatOverlay;
		if (in_overlay)
			((Scene_Overlay*)Scene::instance.get())->SetOnUpdate([this] { UpdateScene(); });
		else
			Scene::Push(std::make_shared<Scene_Overlay>([this] { UpdateScene(); }));
	}
}

void ChatOverlay::DoScroll(int increase) {
	scroll += increase;
	scroll = (scroll + messages.size()) % messages.size();
	counter = 0;
	dirty = true;
}

void ChatOverlay::OnResolutionChange() {
	if (!bitmap) {
		DrawableMgr::Register(this);
	}

	int text_height = ChatTextHeight();
	Rect rect = DisplayUi->GetScreenSurfaceRect();
	int width = rect.width;
	black = Bitmap::Create(width, text_height, Color(0, 0, 0, 255));
	grey = Bitmap::Create(width, text_height, Color(80, 80, 80, 255));
	bitmap = Bitmap::Create(width, rect.height, true);
	scrollbar = Bitmap::Create((int)ceilf(0.008 * width), rect.height, Color(255, 255, 255, 255));
	dirty = true;
}

bool ChatOverlay::IsAnyMessageVisible() const {
	return std::any_of(messages.cbegin(), messages.cend(), [](const ChatOverlayMessage& m) { return !m.hidden; });
}

std::vector<std::shared_ptr<ChatComponent>> ChatOverlayMessage::Convert(ChatOverlay* parent, bool has_badge) const {
	StringView msg(text_orig);
	std::vector<std::shared_ptr<ChatComponent>> out;
	static std::regex pattern(":(\\w+):");

	out.emplace_back(std::make_shared<ChatComponent>(
		Text::GetSize(*Font::ChatText(), fmt::format("[{}] ", sender)).width + (has_badge ? ChatTextHeight() : 0),
		ChatTextHeight()));

	auto begin = std::cregex_iterator(msg.begin(), msg.end(), pattern);
	auto end = decltype(begin){};

	auto last_end = msg.begin();
	for (auto& it = begin; it != end; ++it) {
		auto& match = *it;

		StringView between(last_end, match[0].first - last_end);
		if (!between.empty()) {
			out.emplace_back(std::make_shared<ChatString>(between));
		}

		//TODO: parse ynomojis
		out.emplace_back(std::make_shared<ChatEmoji>(parent, ChatTextHeight(), ChatTextHeight(), match[1].str()));
		last_end = match[0].second;
	}

	StringView last(last_end, msg.end() - last_end);
	if (!last.empty()) {
		out.emplace_back(std::make_shared<ChatString>(last));
	}

	return out;
}

ChatOverlayMessage::ChatOverlayMessage(ChatOverlay* parent, std::string text, std::string sender, BitmapRef system, std::string badge) :
	parent(parent), text_orig(std::move(text)), sender(std::move(sender)), system(system)
{
	bool has_badge = badge != "null" && !badge.empty();
	this->text = Convert(parent, has_badge);

	if (has_badge) {
		auto req = AsyncHandler::RequestFile("../images/badge", badge);
		req->SetGraphicFile(true);
		req->SetParentScope(true);
		req->SetRequestExtension(".png");
		badge_request = req->Bind(&ChatOverlayMessage::OnBadgeReady, this);
		req->Start();
	}
}

void ChatOverlayMessage::OnBadgeReady(FileRequestResult* result) {
	if (!result->success) return;
	badge = Cache::Badge(result->file);
	parent->MarkDirty();
}

ChatEmoji::ChatEmoji(ChatOverlay* parent_, int width, int height, std::string emoji_) :
	ChatComponent(width, height, ChatComponents::Emoji), emoji(std::move(emoji_)), parent(parent)
{
	assert(!emoji.empty() && "Cannot create an empty emoji component");
	assert(parent);

	RequestBitmap(parent_);
}

void ChatEmoji::RequestBitmap(ChatOverlay* parent_) {
	parent = parent_;
	auto req = AsyncHandler::RequestFile("../images/ynomoji", emoji);
	req->SetGraphicFile(true);
	req->SetParentScope(true);
	req->SetRequestExtension(".png");
	request = req->Bind([wself=weak_from_this()](FileRequestResult* result) {
		if (!result->success) return;
		if (auto sobj = wself.lock()) {
			auto* self = static_cast<ChatEmoji*>(sobj.get());
			self->bitmap = Cache::Emoji(result->file);
			self->parent->MarkDirty();
		}
		else Output::Debug("expired: {}", result->file);
	});
	req->Start();
}

ChatScreenshot::ChatScreenshot(ChatOverlay* parent, std::string id_, bool temp, bool spoiler) :
	parent(parent), id(std::move(id_)), temp(temp), spoiler(spoiler)
{
}

int ChatEmoji::Draw(Bitmap& dest) {
	// nothing yet
	return ChatTextHeight();
}

int ChatString::Draw(Bitmap& dest) {
	return 0;
}
