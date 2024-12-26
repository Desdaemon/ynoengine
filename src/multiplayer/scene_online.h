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

#ifndef EP_MP_SCENE_ONLINE_H
#define EP_MP_SCENE_ONLINE_H

#include "scene.h"
#include "window_settings.h"
#include "window_help.h"

/** Online settings. */
class Scene_Online : public Scene {
public:
	Scene_Online();
	void Start() override;
	void Refresh() override;
	void vUpdate() override;
private:
	void CreateOptionsWindow();

	void UpdateOptions();

	void SetMode(Window_Settings::UiMode new_mode);

	std::unique_ptr<Window_Settings> options_window;
	std::unique_ptr<Window_Help> help_window;

	Window_Settings::UiMode mode = Window_Settings::eNone;
};

#endif
