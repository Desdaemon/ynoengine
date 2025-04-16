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

#ifndef EP_MP_SCENE_NEXUS_H
#define EP_MP_SCENE_NEXUS_H

#include <filesystem>

#include "scene.h"

class Scene_Nexus : public Scene {
public:
	Scene_Nexus();
	void Start() override;
	void Continue(SceneType) override;
	void DrawBackground(Bitmap&) override;
	void vUpdate() override;
private:
	void LaunchGame(std::string_view);
	void InitWebview();
	std::string selected_game;
	std::filesystem::path old_pwd;
};

#endif
