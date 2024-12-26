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

#include <uv.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <chrono>
using json = nlohmann::json;

#include "chat_overlay.h"
#include "drawable_mgr.h"
#include "player.h"
#include "baseui.h"
#include "output.h"
#include "input_buttons.h"
#include "cache.h"
#include "game_message.h"
#include "scene.h"
#include "multiplayer/game_multiplayer.h"
#include "multiplayer/scene_overlay.h"
#include "messages.h"

namespace {
	bool LargeScreen() {
		return DisplayUi->GetScreenSurfaceRect().width >= 640;
	}
	int ChatTextHeight() {
		return LargeScreen() ? 37 : 12;
	}
	Point ChatTextSquare() {
		int text_height = ChatTextHeight();
		return { text_height, text_height };
	}
	enum class FileExtensions : char { png, gif, webp };
	struct EmojiInfo {
	public:
		FileExtensions extensions = FileExtensions::png;
	};
	std::map<std::string, EmojiInfo> emojis;

	void InitializeEmojiMappings() {
		if (!emojis.empty()) return;

		uv_queue_work(uv_default_loop(), new uv_work_t{},
		[](uv_work_t*) {
			auto resp = cpr::Get(cpr::Url{"https://ynoproject.net/game/ynomoji.json"});
			if (!(resp.status_code >= 200 && resp.status_code < 300))
				Output::Error("no emojis??");
			json data = json::parse(resp.text, nullptr, false);
			if (data.is_discarded())
				Output::Error("invalid ynomoji json");

			for (const auto& [key, value] : data.items()) {
				std::string path(value);
				if (auto extoffset = path.rfind('.'); extoffset != std::string::npos) {
					auto ext = path.substr(extoffset);
					if (ext == ".png") {
						auto& emoji = emojis[(std::string)key];
						emoji.extensions = FileExtensions::png;
					}
					//else Output::Debug("unimplemented emoji {}.{}", key, ext);
				}
			}
		},
		[](uv_work_t* task, int) { delete task; });
	}
}

ChatOverlay::ChatOverlay() : Drawable(Priority_Overlay, Drawable::Flags::Global | Drawable::Flags::Screenspace)
{
	InitializeEmojiMappings();
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
	int text_height;
	std::optional<Font::StyleScopeGuard> _guard;
	if (LargeScreen()) {
		Font::Style style{};
		style.italic = true;
		style.size = 33; // ChatTextHeight();
		_guard = font.ApplyStyle(style);
		text_height = 37; // font.GetCurrentStyle().size;
	}
	else {
		text_height = 12;
	}

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
		if (GMI().CanChat()) {
			int prompt_width = Text::Draw(*bitmap, offset, y, font, offwhite, ">").x;
			offset += prompt_width;
			if (!input.empty())
				offset += Text::Draw(*bitmap, offset, y, font, offwhite, input).x;
			// cursor
			bitmap->FillRect({ offset, y + text_height - 3, prompt_width, 3 }, offwhite);
		}
		else {
			Text::Draw(*bitmap, offset, y, font, Color(160, 160, 160, 255),
				"Set a username in Online settings to join the chat.");
		}
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
			bool has_header = true;
			Game_Message::WordWrap(
				message->text,
				bitmap->width(),
				[&](lcf::Span<std::shared_ptr<ChatComponent>> line) {
					if (first) {	
						first = false;
						// we added the username as a virtual span, so now we remove it.
						if (line[0]->Downcast<ChatComponents::Header>())
							line = { line.begin() + 1, line.end() };
						else
							has_header = false;
					}
					if (!line.empty()) {
						auto& nextline = lines.emplace_back();
						for (auto& span : line) {
							nextline.emplace_back(span);
						}
					}
				}, font);

			for (auto line = lines.rbegin(); line != lines.rend(); ++line) {
				// semitransparent bg
				int y = y_end - (lidx + 1 + input_row_offset) * text_height;
				bitmap->Blit(0, y, *bg, bg->GetRect(), show_all ? highlight ? 240 : 200 : 150);

				int offset = 2;
				if (std::next(line) == lines.rend() && has_header) {
					// draw the username
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, message->account ? "[" : "<").x;
					offset += Text::Draw(*bitmap, offset, y, font, *message->system, 0, message->sender).x;
					if (message->badge) {
						bitmap->StretchBlit({offset, y, text_height, text_height}, *message->badge, message->badge->GetRect(), 255);
						offset += text_height;
					}
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, message->account ? "] " : "> ").x;
				}
				for (auto& span : *line) {
					//auto& component = static_cast<ChatComponent&>(span);
					if (auto str = span->Downcast<ChatComponents::String>()) {
						offset += Text::Draw(*bitmap, offset, y, font, str->color, str->string).x;
					}
					else if (auto emoji = span->Downcast<ChatComponents::Emoji>()) {
						//auto dims = *static_cast<Point*>(emoji);
						Point dims = emoji->GetSize();
						if (emoji->bitmap) {
							double zoom = emoji->bitmap->GetRect().y / (double)text_height;
							bitmap->StretchBlit({offset, y, text_height, text_height}, *emoji->bitmap, emoji->bitmap->GetRect(), 255);
						}
						else {
							// the emoji is still live, request it now
							emoji->RequestBitmap(this);
							bitmap->FillRect({ offset, y, text_height, text_height }, offwhite);
						}
						offset += text_height;
					}
					else if (auto box = span->Downcast<ChatComponents::Box>()) {
						int width = box->GetSize().x;
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

ChatOverlayMessage& ChatOverlay::AddMessage(
	std::string_view message, std::string_view sender, std::string_view system, std::string_view badge, bool account) {
	counter = 0;

	auto sysref = system.empty() ? Cache::SystemOrBlack() : Cache::System(system.data());

	auto& ret = messages.emplace_back(this, std::string(message), std::string(sender), sysref, std::string(badge), account);

	while (messages.size() > message_max) {
		messages.pop_front();
	}

	if (scroll && (scroll + 1 < messages.size()))
		scroll += 1;

	dirty = true;
	return ret;
}

void ChatOverlay::AddSystemMessage(StringView msg) {
	auto& chatmsg = messages.emplace_back(this, std::string(msg), std::string(""), Cache::SystemOrBlack(), std::string(""), false);
	chatmsg.text.erase(chatmsg.text.begin());
	for (auto& span : chatmsg.text) {
		if (auto string = span->Downcast<ChatComponents::String>()) {
			string->color = Color(230, 230, 180, 255);
			break;
		}
	}
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
	if (Input::IsTriggered(Input::InputButton::CHAT_SCROLL_DOWN)) {
		delta = 5;
		DoScroll(-1);
	}
	else if (Input::IsRepeated(Input::InputButton::CHAT_SCROLL_DOWN)) {
		DoScroll(std::clamp(--delta / 4, -4, -1));
	}

	if (Input::IsTriggered(Input::InputButton::CHAT_SCROLL_UP)) {
		delta = -5;
		DoScroll(1);
	}
	else if (Input::IsRepeated(Input::InputButton::CHAT_SCROLL_UP)) {
		DoScroll(std::clamp(++delta / 4, 1, 4));
	}

	if (Input::IsRawKeyPressed(Input::Keys::MOUSE_SCROLLDOWN)) {
		DoScroll(-3);
	}
	else if (Input::IsRawKeyPressed(Input::Keys::MOUSE_SCROLLUP)) {
		DoScroll(3);
	}

	if (!GMI().CanChat()) return;
	// chat input only below this point

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
	if (IsTriggeredOrRepeating(CustomKeyTimers::backspace) && !input.empty()) {
		input.pop_back();
		dirty = true;
	}
	if (Input::IsRawKeyTriggered(Input::Keys::RETURN) && !input.empty()) {
		GMI().sessionConn.SendPacketAsync<Messages::C2S::SessionGSay>(input);
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

bool ChatOverlay::IsTriggeredOrRepeating(CustomKeyTimers key) {
	assert(key < CustomKeyTimers::END && "invalid key");
	auto& timer = key_timers[static_cast<int>(key)];
	auto now = Game_Clock::GetFrameTime();

	if (Input::IsRawKeyTriggered(timer.raw_key)) {
		timer.last_triggered = now;
		return true;
	}

	if (Input::IsRawKeyPressed(timer.raw_key)) {
		auto held_duration = now - timer.last_triggered;	
		if (held_duration >= timer.delay && (now - timer.last_repeated) >= timer.rate) {
			timer.last_repeated = now;
			return true;
		}
	}

	return false;
}

std::vector<std::shared_ptr<ChatComponent>> ChatOverlayMessage::Convert(ChatOverlay* parent, bool has_badge) const {
	StringView msg(text_orig);
	std::vector<std::shared_ptr<ChatComponent>> out;
	static std::regex pattern(":(\\w+):");

	int text_height = ChatTextHeight();
	auto font = Font::ChatText();
	Font::Style style{};
	style.size = text_height;
	auto _guard = font->ApplyStyle(style);

	out.emplace_back(std::make_shared<ChatComponent>([this, has_badge] {
		auto& font = Font::ChatText();
		Rect rect = Text::GetSize(*font, fmt::format("[{}] ", sender));
		if (has_badge)
			rect.width += ChatTextHeight();
		return Point{ rect.width, rect.height };
	}, ChatComponents::Header));

	auto begin = std::cregex_iterator(msg.begin(), msg.end(), pattern);
	auto end = decltype(begin){};

	auto last_end = msg.begin();
	for (auto& it = begin; it != end; ++it) {
		auto& match = *it;

		StringView between(last_end, match[0].first - last_end);
		if (!between.empty()) {
			out.emplace_back(std::make_shared<ChatString>(between));
		}

		auto emoji_key = match[1].str();
		if (emojis.empty() || emojis.find(emoji_key) != emojis.end())
			out.emplace_back(std::make_shared<ChatEmoji>(parent, ChatTextHeight(), ChatTextHeight(), emoji_key));
		else
			out.emplace_back(std::make_shared<ChatString>(match[0].str()));
		last_end = match[0].second;
	}

	StringView last(last_end, msg.end() - last_end);
	if (!last.empty()) {
		out.emplace_back(std::make_shared<ChatString>(last));
	}

	return out;
}

ChatOverlayMessage::ChatOverlayMessage(
	ChatOverlay* parent, std::string text, std::string sender, BitmapRef system, std::string badge, bool account) :
	parent(parent), text_orig(std::move(text)), sender(std::move(sender)), system(system), account(account)
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
	ChatComponent(width, height, ChatComponents::Emoji), emoji(std::move(emoji_)), parent(parent_)
{
	assert(!emoji.empty() && "Cannot create an empty emoji component");
	assert(parent);
	sizer = ChatTextSquare;

	RequestBitmap(parent);
}


void ChatEmoji::RequestBitmap(ChatOverlay* parent_) {
	if (emoji.empty()) return;
	parent = parent_;
	auto req = AsyncHandler::RequestFile("../images/ynomoji", emoji);
	req->SetGraphicFile(true);
	req->SetParentScope(true);
	req->SetRequestExtension(".png");
	request = req->Bind([this](FileRequestResult* result) {
		if (!result->success) {
			emoji.clear();
			Output::Debug("failed: {}", result->file);
			return;
		}
		bitmap = Cache::Emoji(result->file);
		parent->MarkDirty();
	});
	req->Start();
}

ChatScreenshot::ChatScreenshot(ChatOverlay* parent, std::string id_, bool temp, bool spoiler) :
	parent(parent), id(std::move(id_)), temp(temp), spoiler(spoiler)
{
}

