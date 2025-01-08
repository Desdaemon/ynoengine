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

#include "drawable.h"
#include "bitmap.h"
#include "game_clock.h"

/** Shows player count, connection status, current world etc. */
class StatusOverlay : public Drawable {
public:
	StatusOverlay();
	void Draw(Bitmap&) override;
	void OnResolutionChange() override;

	void Update();
	void MarkDirty(bool reset_timer = false);
	void SetLocation(std::string_view loc);
	void SetPlayerCount(int player_count);
	void ResetTimer();
private:
	bool dirty = true;
	BitmapRef bitmap = nullptr;
	std::string location = "";
	int player_count = 0;
	Game_Clock::time_point last_shown;
};
