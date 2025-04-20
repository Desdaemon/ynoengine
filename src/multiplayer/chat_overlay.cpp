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
#include "multiplayer/overlay_utils.h"
#include <algorithm>
#include <regex>

#include <uv.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include "chat_overlay.h"
#include "drawable_mgr.h"
#include "baseui.h"
#include "output.h"
#include "input_buttons.h"
#include "cache.h"
#include "game_message.h"
#include "scene.h"
#include "multiplayer/game_multiplayer.h"
#include "multiplayer/scene_overlay.h"
#include "messages.h"
#include "input.h"
#include "font.h"
#include "compiler.h"
#include "icons.h"
#include "overlay_utils.h"
#include "image_webp.h"
#include "image_gif.h"

namespace {
	Point ChatTextSquare() {
		int text_height = OverlayUtils::ChatTextHeight();
		return { text_height, text_height };
	}
	enum class FileExtensions : char { png, gif, webp };

	struct EmojiInfo {
	public:
		FileExtensions extensions = FileExtensions::png;
	};
	std::map<std::string, EmojiInfo> emojis;
	std::unordered_map<std::string, std::weak_ptr<ChatEmoji>> emojiCache; // Cache for animated emojis

	void InitializeEmojiMappings() {
		if (!emojis.empty()) return;

		uv_queue_work(uv_default_loop(), new uv_work_t{},
		[](uv_work_t*) {
			auto resp = cpr::Get(cpr::Url{"https://ynoproject.net/game/ynomoji.json"});
			if (!(resp.status_code >= 200 && resp.status_code < 300))
				Output::Error("no emojis: {}", resp.text);
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
					} else if (ext == ".gif") {
						auto& emoji = emojis[(std::string)key];
						emoji.extensions = FileExtensions::gif;
					} else if (ext == ".webp") {
						auto& emoji = emojis[(std::string)key];
						emoji.extensions = FileExtensions::webp;
					}
					// else Output::Debug("unimplemented emoji {}.{}", key, ext);
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
		dst.BlitFast(ox, oy, *bitmap, bitmap->GetRect(), Opacity::Opaque());
		return;
	}

	bitmap->Clear();

	Rect screen_rect = DisplayUi->GetScreenSurfaceRect();
	int y_end = screen_rect.height;

	auto& font = *Font::ChatText();
	int text_height;
	std::optional<Font::StyleScopeGuard> _guard;
	if (OverlayUtils::LargeScreen()) {
		Font::Style style{};
		style.italic = true;
		style.size = 33; // ChatTextHeight();
		_guard = font.ApplyStyle(style);
		text_height = 37; // font.GetCurrentStyle().size;
	}
	else {
		text_height = 12;
	}

	for (auto it = emojiCache.begin(); it != emojiCache.end(); ++it) {
		if (auto emoji = it->second.lock()) {
			emoji->in_viewport = false;
		} 
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
		bitmap->FillRect({ 0, y, screen_rect.width, text_height }, Color{0, 0, 0, 200});
		if (GMI().CanChat()) {
			int prompt_width = 0;
			prompt_width = Text::Draw(*bitmap, offset, y, font, offwhite, ">").x;
			offset += prompt_width;
			for (const auto& ch : input)
				offset += Text::Draw(*bitmap, offset, y, font, offwhite, ch, false).x;
			// cursor
			bitmap->FillRect({ offset, y + text_height - 3, prompt_width, 3 }, offwhite);
			offset += Input::RenderTextComposition(*bitmap, offset, y, &font);
		}
		else {
			Text::Draw(*bitmap, offset, y, font, Color(160, 160, 160, 255),
				"Set a username in Online settings to join the chat.");
		}
	}

	int y_offset = 0;
	int message_max_minimized = OverlayUtils::LargeScreen() ? 4 : 2;

	// we're doing the inverse of MessageOverlay: draw from the bottom up
	for (auto message = messages.rbegin() + viewport; message != messages.rend(); ++message) {
		if (!message->hidden || show_all) {
			auto& bg = !show_all ? black :
				unlocked_start ? (i == scroll ? grey : black) :
				unlocked_end ? (last_sticky + i == scroll ? grey : black) :
				i == half_screen ? grey : black;
			bool highlight = bg != black;

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
				int yidx = lidx + 1 + input_row_offset;
				int line_height = text_height;
				bool emojis_only = false;

				// begin override line height for components
				// if adding new components, remember to update LayoutViewport

				// expand messages with only emojis
				if (std::any_of(line->cbegin(), line->cend(), [](const std::shared_ptr<ChatComponent>& comp) { return bool(comp->Downcast<ChatComponents::Screenshot>()); })) {
					line_height = ChatScreenshot::sizer().y;
				}
				else if (std::all_of(line->cbegin(), line->cend(), [](const std::shared_ptr<ChatComponent>& comp) { return bool(comp->Downcast<ChatComponents::Emoji>()); })) {
					emojis_only = true;
					line_height = OverlayUtils::LargeScreen() ? 56 : 18;
				}

				// end override line height

				y_offset += line_height - text_height;
				int y = y_end - yidx * text_height - y_offset;

				uint8_t opacity = show_all ? highlight ? 240 : 200 : 150;
				bitmap->FillRect({ 0, y, screen_rect.width, line_height }, Color{bg.red, bg.green, bg.blue, opacity});

				int offset = 2;
				if (std::next(line) == lines.rend() && EP_LIKELY(has_header)) {
					// draw the username
					if (message->global)
						offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::megaphone, offwhite, font);
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, message->account ? "[" : "<").x;
					offset += Text::Draw(*bitmap, offset, y, font, *message->system_bm, 0, message->sender).x;
					if (message->badge) {
						bitmap->StretchBlit({offset, y, text_height, text_height}, *message->badge, message->badge->GetRect(), Opacity::Opaque());
						offset += text_height;
					}
					switch (message->rank) {
					case 1: // mod
						offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::shield, offwhite, font); break;
					case 2: // dev
						offset += RenderIcon(*bitmap, offset, y, Icons::YnoIcons::wrench, offwhite, font); break;
					}
					offset += Text::Draw(*bitmap, offset, y, font, offwhite, message->account ? "] " : "> ").x;
				}
				int baseline = 0;
				for (auto& span : *line) {
					if (auto str = span->Downcast<ChatComponents::String>()) {
						offset += Text::Draw(*bitmap, offset, y + (baseline ? baseline - text_height : 0), font, str->color, str->string).x;
					}
					else if (auto emoji = span->Downcast<ChatComponents::Emoji>()) {
						emoji->in_viewport = true;
						Point dims = emoji->GetSize();
						if (emojis_only) dims.x = dims.y = line_height;
						int comp_y = y + (baseline ? baseline - text_height : 0);
						if (emoji->bitmap) {
							bitmap->StretchBlit({offset, comp_y, dims.x, dims.y}, *emoji->bitmap, emoji->bitmap->GetRect(), 255);
						}
						else if (!emoji->HasAnimation()) {
							// the emoji is still live, request it now
							emoji->RequestBitmap(this);
							bitmap->FillRect({ offset, comp_y, dims.y, dims.y }, offwhite);
						}
						offset += dims.x;
					}
					else if (auto screenshot = span->Downcast<ChatComponents::Screenshot>()) {
						Point dims = screenshot->GetSize();
						if (screenshot->bitmap) {
							bitmap->StretchBlit({offset, y, dims.x, dims.y}, *screenshot->bitmap, screenshot->bitmap->GetRect(), 255);
						}
						else {
							bitmap->FillRect({ offset, y, dims.x, dims.y }, offwhite);
						}
						offset += dims.x;
						baseline = std::max(baseline, dims.y);
					}
					else if (auto box = span->Downcast<ChatComponents::Box>()) {
						int width = box->GetSize().x;
						bitmap->FillRect({ offset, y + (baseline ? baseline - text_height : 0), width, text_height }, offwhite);
						offset += width;
					}
				}
				++lidx;
				if ((lidx + input_row_offset) * text_height + y_offset > y_end) break;
			}
			++i;
			lines.clear();
		}
		if (!show_all && lidx >= message_max_minimized) break;
		if ((lidx + input_row_offset) * text_height + y_offset > y_end) break;
	}

	if (show_all) {
		double scrollbar_height = screen_rect.height / (double)text_height / messages.size() * screen_rect.height;
		Rect dims{ 0, 0, (int)ceilf(0.008 * screen_rect.width), (int)ceilf(scrollbar_height) };
		//how far should the scrollbar go
		double scrollbar_extent = scroll / (double)message_max;
		int desired_y = floorf(y_end * (1 - scrollbar_extent)) - dims.height;
		bitmap->FillRect({ bitmap->width() - dims.width, std::clamp(desired_y, 0, y_end - dims.height), scrollbar_width, (int)ceilf(scrollbar_height) }, scrollbar);
	}

	dst.BlitFast(ox, oy, *bitmap, bitmap->GetRect(), Opacity::Opaque());
	dirty = false;
}

ViewportInfo ChatOverlay::LayoutViewport(int text_height, const Font& font) const {
	Rect screen_rect = DisplayUi->GetScreenSurfaceRect();
	int y_end = screen_rect.height;
	int half_screen = y_end / 2 / text_height;

	bool unlocked_start = scroll < half_screen;
	bool unlocked_end = scroll > messages.size() - half_screen;
	int last_sticky = std::max(0, (int)messages.size() - half_screen * 2);
	int viewport = show_all
		? std::clamp(scroll - half_screen, 0, std::min(scroll + half_screen, last_sticky))
		: 0;

	int extra = 0, lidx = viewport, scroll_extra = 0, last_sticky_extra = 0;
	for (auto message = messages.rbegin() + viewport; message != messages.rend(); ++message) {
		const auto& components = message->text;
		if (std::any_of(components.cbegin(), components.cend(), [](const std::shared_ptr<ChatComponent>& comp) { return bool(comp->Downcast<ChatComponents::Screenshot>()); })) {
			last_sticky_extra += extra = ChatScreenshot::sizer().y - text_height;
		} else if (std::all_of(components.cbegin(), components.cend(), [](const std::shared_ptr<ChatComponent>& comp) { return bool(comp->Downcast<ChatComponents::Emoji>()); })) {
			last_sticky_extra += extra = (OverlayUtils::LargeScreen() ? 56 : 18) - text_height;
		} else {
			auto dims = Text::GetSize(font, message->text_orig);
			int lines = ceilf(dims.width / (double)bitmap->width());
			last_sticky_extra += extra = std::max(0, lines - 1) * text_height;
			lidx += std::max(0, lines - 1);
		}
		if (lidx <= scroll) scroll_extra += extra;
		if (lidx >= last_sticky) break;
		lidx += 1;
	}

	ViewportInfo out{};
	out.scroll_px = scroll * text_height + scroll_extra;
	out.last_sticky_px = last_sticky * text_height + last_sticky_extra;
	out.extra = last_sticky_extra;

	return out;
}

ChatOverlayMessage& ChatOverlay::AddMessage(
	std::string_view message, std::string_view sender, std::string_view sender_uuid, std::string_view system, std::string_view badge,
	bool account, bool global, int rank) {
	counter = 0;

	auto& ret = messages.emplace_back(this, std::string(message), std::string(sender), std::string(sender_uuid), std::string(system), std::string(badge), account, global, rank);

	while (messages.size() > message_max) {
		messages.pop_front();
	}

	if (scroll && (scroll + 1 < messages.size()))
		scroll += 1;

	dirty = true;
	return ret;
}

void ChatOverlay::AddSystemMessage(std::string_view msg) {
	auto& chatmsg = messages.emplace_back(this, std::string(msg), "", "", "", std::string(""), false, true, 0);
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

	// Update animations for all visible emojis
	for (auto it = emojiCache.begin(); it != emojiCache.end();) {
		auto& emoji = it->second;
		if (auto emojiPtr = emoji.lock()) {
			emojiPtr->UpdateAnimation();
			++it; // Move to the next element
		} else {
			it = emojiCache.erase(it); // Remove expired weak pointers
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

	if (Input::IsRawKeyPressed(Input::Keys::ENDS)) {
		scroll = 0;
		MarkDirty();
	}

	if (!GMI().CanChat()) return;

	// chat input only below this point

	std::string_view ui_input(Input::text_input.data());
	if (!ui_input.empty()) {
		input.append(Utils::DecodeUTF32(ui_input));
		dirty = true;
		Input::text_input.clear();
	}
	else if (Input::composition.active) {
		dirty = true;
	}

	if (IsTriggeredOrRepeating(CustomKeyTimers::backspace) && !input.empty()) {
		input.pop_back();
		dirty = true;
	}
	if (Input::IsRawKeyTriggered(Input::Keys::RETURN) && !input.empty()) {
		// TODO: map chat and party chat
		std::string encoded = Utils::EncodeUTF(input);
		// ChatOverlay::AddMessage(encoded, "blah", "00000000000000000000", "", "", true, true, 0);
		GMI().sessionConn.SendPacket(Messages::C2S::SessionSay{ std::move(encoded) });
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
		Rect screen_rect = DisplayUi->GetScreenSurfaceRect();
		int text_height = OverlayUtils::ChatTextHeight();
		Rect rect{ 0, screen_rect.height - text_height, screen_rect.width, text_height };
		// Rect rect{ -100, -100, screen_rect.width, text_height };
		DisplayUi->BeginTextCapture(&rect);
	}
	else
		DisplayUi->EndTextCapture();
}

void ChatOverlay::DoScroll(int increase) {
	if (messages.empty()) return;
	scroll += increase;
	scroll = (scroll + messages.size()) % messages.size();
	counter = 0;
	dirty = true;
}

void ChatOverlay::OnResolutionChange() {
	if (!bitmap) {
		DrawableMgr::Register(this);
	}

	int text_height = OverlayUtils::ChatTextHeight();
	Rect rect = DisplayUi->GetScreenSurfaceRect();
	bitmap = Bitmap::Create(rect.width, rect.height);
	scrollbar_width = ceilf(0.008 * rect.width);
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
	std::string_view msg(text_orig);
	std::vector<std::shared_ptr<ChatComponent>> out;
	static std::regex emoji_pattern(":(\\w+):");
	static std::regex screenshot_pattern(R"regex(\[(t)?([\w\d]{16})(?::(\d)+)?\])regex");

	int text_height = OverlayUtils::ChatTextHeight();
	auto font = Font::ChatText();
	Font::Style style{};
	style.size = text_height;
	auto _guard = font->ApplyStyle(style);

	out.emplace_back(std::make_shared<ChatComponent>([this, has_badge] {
		auto& font = *Font::ChatText();
		Rect rect = Text::GetSize(font, fmt::format("[{}] ", sender));
		int square = OverlayUtils::ChatTextHeight();
		if (has_badge)
			rect.width += square;
		if (global)
			rect.width += square;
		if (rank > 0)
			rect.width += square;
		return Point{ rect.width, rect.height };
	}, ChatComponents::Header));

	auto it = msg.begin();
	auto last_end = msg.begin();
	while (it != msg.end()) {
		std::match_results<std::string_view::const_iterator> match;
		if (std::regex_search(it, msg.end(), match, emoji_pattern)) {
			std::string_view between(&*last_end, match[0].first - last_end);
			if (!between.empty()) {
				out.emplace_back(std::make_shared<ChatString>(between));
			}

			auto emoji_key = match[1].str();
			if (emojis.empty() || emojis.find(emoji_key) != emojis.end())
				// out.emplace_back(std::make_shared<ChatEmoji>(parent, text_height, text_height, emoji_key));
				out.emplace_back(ChatEmoji::GetOrCreate(emoji_key, parent));
			else
				out.emplace_back(std::make_shared<ChatString>(match[0].str()));
			last_end = match[0].second;
			it = last_end;
		}
		else if (std::regex_search(it, msg.end(), match, screenshot_pattern)) {
			std::string_view between(&*last_end, match[0].first - last_end);
			if (!between.empty()) {
				out.emplace_back(std::make_shared<ChatString>(between));
			}

			auto temp = match[1].str() == "t";
			auto id = match[2].str();
			out.emplace_back(std::make_shared<ChatScreenshot>(parent, sender_uuid, id, temp, false));
			last_end = match[0].second;
			it = last_end;
		}
		else {
			break;
		}
	}

	std::string_view last(&*last_end, msg.end() - last_end);
	if (!last.empty()) {
		out.emplace_back(std::make_shared<ChatString>(last));
	}

	return out;
}

ChatOverlayMessage::ChatOverlayMessage(
	ChatOverlay* parent_, std::string text, std::string sender, std::string sender_uuid, std::string system_, std::string badge,
	bool account, bool global, int rank) :
	parent(parent_), text_orig(std::move(text)), sender(std::move(sender)), sender_uuid(std::move(sender_uuid)), system(system_), account(account), global(global), rank(rank)
{
	bool has_badge = badge != "null" && !badge.empty();

	this->text = Convert(parent, has_badge);

	system_bm = Cache::SystemOrBlack();
	if (!system.empty()) {
		auto req = AsyncHandler::RequestFile("System", system);
		req->SetGraphicFile(true);
		system_request = req->Bind([this](FileRequestResult* result) {
			if (!result->success) return;
			system_bm = Cache::System(result->file);
			parent->MarkDirty();
		});
		req->Start();
	}

	if (has_badge) {
		auto req = AsyncHandler::RequestFile("../images/badge", badge);
		req->SetGraphicFile(true);
		req->SetParentScope(true);
		req->SetRequestExtension(".png");
		badge_request = req->Bind([this](FileRequestResult* result) {
			if (!result->success) return;
			this->badge = Cache::Badge(result->file);
			this->badge->SetBilinear();
			parent->MarkDirty();
		});
		req->Start();
	}
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

	if (request) return;

	auto req = AsyncHandler::RequestFile("../images/ynomoji", emoji);
	req->SetGraphicFile(true);
	req->SetParentScope(true);

	// Determine the file extension to request based on the emoji's extension type
	auto extension = emojis[emoji].extensions;
	switch (extension) {
		case FileExtensions::png:
			req->SetRequestExtension(".png");
			break;
		case FileExtensions::gif:
			req->SetRequestExtension(".gif");
			break;
		case FileExtensions::webp:
			req->SetRequestExtension(".webp");
			break;
	}

	request = req->Bind([this, extension](FileRequestResult* result) {
		request.reset(); // Clear the request after processing
		if (!result->success) {
			emoji.clear();
			Output::Debug("failed: {}", result->file);
			return;
		}
		switch (extension) {
			case FileExtensions::gif:
				DecodeGif(result->file);
				break;
			case FileExtensions::webp:
				DecodeWebP(result->file);
				break;
			case FileExtensions::png:
				bitmap = Cache::Emoji(result->file);
				bitmap->SetBilinear();
				break;
		}
		if (parent) parent->MarkDirty();
	});
	req->Start();
}

void ChatEmoji::DecodeGif(const std::string& filePath) {
	constexpr int minimum_frame_delay = 20;
	frames.clear();
	frameDelays.clear();
	auto fs = FileFinder::OpenImage("../images/ynomoji", filePath);
	if (!fs) return;

	ImageGif::Decoder dec(fs);
	if (!dec) return;

	ImageOut image{};
	GifTimingInfo timing{};
	while (dec.ReadNext(image, timing)) {
		auto bitmap = Bitmap::Create(image.pixels, image.width, image.height, 0, format_R8G8B8A8_a().format());
		if (!bitmap) {
			if (image.pixels) delete[] image.pixels;
			continue;
		}
		bitmap->SetBilinear();
		frames.push_back(std::move(bitmap));

		if (!timing.delay)
			timing.delay = 100;

		int delay = std::min(minimum_frame_delay, timing.delay);
		frameDelays.push_back(delay);
		loopLength += frameDelays.back();
	}

	currentFrame = 0;
	if (loopLength && frames.size() > 1) {
		lastFrameTime = std::chrono::steady_clock::now();
		int loopDelta = std::chrono::duration_cast<std::chrono::milliseconds>(lastFrameTime.time_since_epoch()).count() % loopLength;
		while (currentFrame < frameDelays.size() && loopDelta >= frameDelays[currentFrame]) {
			loopDelta -= frameDelays[currentFrame];
			currentFrame = (currentFrame + 1) % frames.size();
		}
	}
}

void ChatEmoji::DecodeWebP(const std::string& filePath) {
	frames.clear();
	frameDelays.clear();
	auto fs = FileFinder::OpenImage("../images/ynomoji", filePath);
	if (!fs) return;

	ImageWebP::Decoder dec(fs);
	if (!dec) return;

	ImageOut image{};
	TimingInfo timing{};
	int smear = 0;
	while (dec.ReadNext(image, timing)) {
		auto bitmap = Bitmap::Create(image.pixels, image.width, image.height, 0, format_R8G8B8A8_a().format());
		if (!bitmap) {
			delete[] image.pixels;
			continue;
		}
		bitmap->SetBilinear();
		frames.push_back(std::move(bitmap));

		if (!timing.timestamp)
			timing.timestamp = 100;

		int lastDelay = frameDelays.empty() ? 0 : frameDelays.back();
		if (timing.timestamp - lastDelay < 20) {
			smear += timing.timestamp - lastDelay;
			frameDelays.push_back(20);
		} else {
			frameDelays.push_back(timing.timestamp - lastDelay + smear);
			smear = 0;
		}
		loopLength += frameDelays.back();
	}

	currentFrame = 0;
	if (loopLength && frames.size() > 1) {
		lastFrameTime = std::chrono::steady_clock::now();
		int loopDelta = std::chrono::duration_cast<std::chrono::milliseconds>(lastFrameTime.time_since_epoch()).count() % loopLength;
		while (currentFrame < frameDelays.size() && loopDelta >= frameDelays[currentFrame]) {
			loopDelta -= frameDelays[currentFrame];
			currentFrame = (currentFrame + 1) % frames.size();
		}
	}
}

void ChatEmoji::UpdateAnimation() {
	if (frames.size() == 1) bitmap = frames[0]; // If there's only one frame, just display it
	else if (frames.size() < 2 || frameDelays.empty()) return; // No animation to update

	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();
	if (elapsed >= frameDelays[currentFrame]) {
		// Move to the next frame
		currentFrame = (currentFrame + 1) % frames.size();
		lastFrameTime = now;
		bitmap = frames[currentFrame]; // Update the displayed frame
		if (parent && in_viewport) parent->MarkDirty(); // Mark the parent as dirty to trigger a redraw
	}
}

std::shared_ptr<ChatEmoji> ChatEmoji::GetOrCreate(const std::string& emojiKey, ChatOverlay* parent) {
	// Check if the emoji is already in the cache
	if (auto existingEmoji = emojiCache[emojiKey].lock()) {
		return existingEmoji; // Return the existing shared reference
	}

	// Create a new instance and store it in the cache
	auto newEmoji = std::make_shared<ChatEmoji>(parent, 0, 0, emojiKey);
	emojiCache[emojiKey] = newEmoji; // Store the weak reference
	return newEmoji;
}

ChatScreenshot::ChatScreenshot(ChatOverlay* parent, std::string uuid_, std::string id_, bool temp, bool spoiler) :
	ChatComponent(sizer, ChatComponents::Screenshot), parent(parent), uuid(std::move(uuid_)), id(std::move(id_)), temp(temp), spoiler(spoiler)
{
	uv_queue_work(uv_default_loop(), &task,
	[](uv_work_t* task) {
		#define EP_CONTAINER_OF(ptr, type, member) (type*)((char*)ptr - offsetof(type, member))
		auto comp = EP_CONTAINER_OF(task, ChatScreenshot, task);
		#undef EP_CONTAINER_OF

		std::string endpoint(GMI().ApiEndpoint("screenshots/"));
		if (comp->temp)
			endpoint.append("temp/");
		endpoint.append(fmt::format("{}/{}.png", comp->uuid, comp->id));
		auto resp = cpr::Get(cpr::Url{endpoint});
		Output::Debug("{} {}", resp.status_code, resp.url.c_str());
		if (!(resp.status_code >= 200 && resp.status_code < 300))
			return;
		comp->bitmap = Bitmap::Create((uint8_t*)resp.text.c_str(), resp.text.size(), false);
		comp->parent->MarkDirty();
	},
	[](uv_work_t* task, int) {
	});
}

Point ChatScreenshot::sizer() {
	if (OverlayUtils::LargeScreen())
		return { 192, 144 };
	return { 64, 48 };
}
