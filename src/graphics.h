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

#ifndef EP_GRAPHICS_H
#define EP_GRAPHICS_H

// Headers
#include <vector>
#include "bitmap.h"
#include "drawable.h"
#include "drawable_list.h"
#include "game_clock.h"
#include "baseui.h"

class MessageOverlay;
class Scene;
#ifdef PLAYER_YNO
class ChatOverlay;
class StatusOverlay;
#endif

/**
 * Graphics namespace.
 * Handles screen drawing.
 */
namespace Graphics {
	/**
	 * Initializes Graphics.
	 */
	void Init();

	/**
	 * Disposes Graphics.
	 */
	void Quit();

	/**
	 * Updates the screen.
	 */
	void Update();

	void Draw(BaseUi& ui);

	void LocalDraw(Bitmap& dst, Bitmap& dst_screen, Drawable::Z_t min_z, Drawable::Z_t max_z);

	std::shared_ptr<Scene> UpdateSceneCallback();

	/**
	 * Returns a handle to the message overlay.
	 * Only used by Output to put messages.
	 *
	 * @return message overlay
	 */
	MessageOverlay& GetMessageOverlay();

	ChatOverlay& GetChatOverlay();

	StatusOverlay& GetStatusOverlay();
}

#endif
