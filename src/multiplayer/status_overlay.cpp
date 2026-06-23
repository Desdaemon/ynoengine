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

#include <chrono>
#include "status_overlay.h"
#include "drawable_mgr.h"
#include "baseui.h"
#include "multiplayer/overlay_utils.h"
#include "multiplayer/game_multiplayer.h"
#include "icons.h"
#include "font.h"
#include "cache.h"
#include "main_data.h"
#include "game_variables.h"
#include "scene.h"

using namespace std::chrono_literals;

StatusOverlay::StatusOverlay() : Drawable(Priority_Overlay, Drawable::Flags::Global | Drawable::Flags::Screenspace) {
}

void StatusOverlay::Draw(Bitmap& dst) {
	auto scene_type = Scene::instance->type;

	auto now = Game_Clock::GetFrameTime();
	bool should_display = now - last_shown < 5s && scene_type != Scene::SceneType::ChatOverlay;

	if (!dirty) {
		if (should_display)
			dst.BlitFast(0, 0, *bitmap, bitmap->GetRect(), Opacity::Opaque());
		return;
	}

	bitmap->Clear();

	Rect rect = bitmap->GetRect();

	int overlay_height = OverlayUtils::LargeScreen() ? 24 : 12;

	// draw background
	bitmap->FillRect({0, 0, rect.width, overlay_height}, Color(0, 0, 0, 150));

	int offset = 0;
	int y = 0;

	auto font = Font::ChatText();
	Font::Style style;
	style.size = overlay_height;
	auto _guard = font->ApplyStyle(style);

	if (GMI().session_connected) {
		offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::connection_3, Color(0, 255, 200, 255), *font);
	} else if (GMI().session_active) {
		offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::connection_2, Color(255, 200, 0, 255), *font);
	} else {
		offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::connection_1, Color(255, 50, 50, 255), *font);
	}

	constexpr Color offwhite { 200, 200, 200, 255 };

	// draw player count
	offset += Icons::RenderIcon(*bitmap, offset, y, Icons::YnoIcons::person, offwhite, *font);
	offset += Text::Draw(*bitmap, offset, y, *font, offwhite, fmt::format("{} ", player_count)).x;

	// TODO: Draw available maps with interaction

	if (!location.empty())
		Text::Draw(*bitmap, rect.width, y, *font, *Cache::SystemOrBlack(), 0, location, Text::Alignment::AlignRight);

	if (should_display)
		dst.BlitFast(0, 0, *bitmap, bitmap->GetRect(), Opacity::Opaque());
	dirty = false;
}

void StatusOverlay::Update() {
	if (!DisplayUi) {
		return;
	}

	if (!bitmap) {
		OnResolutionChange();
	}
}

void StatusOverlay::MarkDirty(bool reset_timer) {
	dirty = true;
	if (reset_timer)
		ResetTimer();
}

void StatusOverlay::OnResolutionChange() {
	if (!bitmap) {
		DrawableMgr::Register(this);
	}

	Rect rect = DisplayUi->GetScreenSurfaceRect();
	bitmap = Bitmap::Create(rect.width, OverlayUtils::ChatTextHeight());
	dirty = true;
}

void StatusOverlay::SetLocation(std::string_view location) {
	this->location = location;
	ResetTimer();
	MarkDirty();
}

void StatusOverlay::SetPlayerCount(int player_count) {
	this->player_count = player_count;
	if (location.empty()) // game just started
		ResetTimer();
	MarkDirty();
}

void StatusOverlay::ResetTimer() {
	last_shown = Game_Clock::GetFrameTime();
}
