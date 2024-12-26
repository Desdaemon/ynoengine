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

#include "scene_online.h"
#include "main_data.h"
#include "game_system.h"

Scene_Online::Scene_Online() {
	type = SceneType::Settings;
}

void Scene_Online::Start() {
	CreateOptionsWindow();

	options_window->Push(Window_Settings::eOnlineAccount);
}

void Scene_Online::Refresh() {

}

void Scene_Online::vUpdate() {
	auto opt_mode = options_window->GetMode();

	if (Input::IsTriggered(Input::CANCEL)) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Cancel));

		options_window->Pop();
		SetMode(options_window->GetMode());
		if (mode == Window_Settings::eNone)
			Scene::Pop();
	}

	UpdateOptions();
}

void Scene_Online::CreateOptionsWindow() {

}

void Scene_Online::UpdateOptions() {
	options_window->Update();
}

void Scene_Online::SetMode(Window_Settings::UiMode new_mode) {
	if (new_mode == mode)
		return;
	mode = new_mode;
}
