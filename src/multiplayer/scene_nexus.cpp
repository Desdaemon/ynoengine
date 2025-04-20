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

#include <nlohmann/json.hpp>

#include "scene_nexus.h"
#include "baseui.h"
#include "platform/sdl/sdl2_ui.h"
#include "player.h"
#include "scene_title.h"
#include "scene_logo.h"
#include "game_clock.h"
#include "main_data.h"
#include "cache.h"
#include "audio.h"
#include "game_system.h"
#include "platform.h"
#include "output.h"
#include "multiplayer/game_multiplayer.h"
#include "graphics.h"

using json = nlohmann::json;

Scene_Nexus::Scene_Nexus() {
	type = Scene::GameBrowser;
}

void Scene_Nexus::Start() {
	Main_Data::game_system = std::make_unique<Game_System>();
	Main_Data::game_system->SetSystemGraphic(CACHE_DEFAULT_BITMAP, lcf::rpg::System::Stretch_stretch, lcf::rpg::System::Font_gothic);
	Game_Clock::ResetFrame(Game_Clock::now());
	InitWebview();
}

void Scene_Nexus::DrawBackground(Bitmap&) {
	// draw nothing
}

void Scene_Nexus::Continue(SceneType) {
	Main_Data::game_system->BgmStop();
	std::filesystem::current_path(old_pwd);

	Cache::ClearAll();
	AudioSeCache::Clear();
	MidiDecoder::Reset();
	lcf::Data::Clear();
	Main_Data::Cleanup();

	// Restore the base resolution
	Player::RestoreBaseResolution();

	Player::game_title = "";
	Player::emscripten_game_name = "";
	selected_game = "";

	Font::ResetDefault();

	Main_Data::game_system = std::make_unique<Game_System>();
	Main_Data::game_system->SetSystemGraphic(CACHE_DEFAULT_BITMAP, lcf::rpg::System::Stretch_stretch, lcf::rpg::System::Font_gothic);

	InitWebview();
}

void Scene_Nexus::InitWebview() {
	DisplayUi->SetWebviewLayout(BaseUi::WebviewLayout::Expanded);
	auto& webview = ((Sdl2Ui*)DisplayUi.get())->GetWebview();
	webview.dispatch([&] {
		webview.unbind("webviewLaunchGame");
		webview.bind("webviewLaunchGame", [this](const std::string& args) -> std::string {
			json args_ = json::parse(args);
			selected_game = args_[0];
			return "null";
		});
		webview.navigate("https://ynoproject.net");
	});
}

void Scene_Nexus::vUpdate() {
	if (!selected_game.empty()) {
		LaunchGame(selected_game);
		selected_game = "";
		return;
	}
}

void Scene_Nexus::LaunchGame(std::string_view game) {
#ifdef _WIN32
	std::string userhome(std::getenv("APPDATA"));
	userhome.append("/ynoproject");
#else
	std::string userhome(std::getenv("HOME"));
	userhome.append("/.local/ynoproject");
#endif

	auto game_ = ToString(game);
	Player::emscripten_game_name = game_;
	userhome = FileFinder::MakeCanonical(FileFinder::MakePath(userhome, game_));
	Platform::File(userhome).MakeDirectory(true);

	// since FileFinder::Game can't really work with absolute paths, that leaves
	// using relative paths while changing our PWD.
	old_pwd = std::filesystem::current_path();
	std::filesystem::current_path(userhome);

	auto& webview = ((Sdl2Ui*)DisplayUi.get())->GetWebview();
	webview.dispatch([&webview] {
		// webview.navigate(fmt::format("https://localhost:8028"));
		webview.navigate("https://playtest.ynoproject.net/dev");
	});
	DisplayUi->Dispatch(BaseUi::Intent::ToggleWebview); // hide the webview on entering the game
	DisplayUi->SetWebviewLayout(BaseUi::WebviewLayout::Sidebar);

	GMI().SyncSaveFile();
	Game_Clock::ResetFrame(Game_Clock::now());
	Scene::Push(std::make_shared<Scene_Logo>());
}
