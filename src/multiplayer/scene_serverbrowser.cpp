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

#include "scene_serverbrowser.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "audio.h"
#include "audio_midi.h"
#include "audio_secache.h"
#include "bitmap.h"
#include "cache.h"
#include "filefinder.h"
#include "font.h"
#include "game_clock.h"
#include "game_system.h"
#include "graphics.h"
#include "input.h"
#include "main_data.h"
#include "multiplayer/game_multiplayer.h"
#include "output.h"
#include "platform.h"
#include "player.h"
#include "scene_logo.h"

using json = nlohmann::json;

Scene_ServerBrowser::Scene_ServerBrowser() {
	type = Scene::GameBrowser;
}

void Scene_ServerBrowser::Start() {
	Main_Data::game_system = std::make_unique<Game_System>();
	Main_Data::game_system->SetSystemGraphic(
		CACHE_DEFAULT_BITMAP,
		lcf::rpg::System::Stretch_stretch,
		lcf::rpg::System::Font_gothic
	);
	Game_Clock::ResetFrame(Game_Clock::now());

	LoadServers();
	ShowServerList();
}

void Scene_ServerBrowser::Continue(SceneType) {
	Main_Data::game_system->BgmStop();
	std::filesystem::current_path(old_pwd);

	Cache::ClearAll();
	AudioSeCache::Clear();
	MidiDecoder::Reset();
	lcf::Data::Clear();
	Main_Data::Cleanup();

	Player::RestoreBaseResolution();
	Player::game_title = "";
	Player::emscripten_game_name = "";
	Player::server_assets_url = "";
	Player::server_sessions_url = "";

	Font::ResetDefault();

	Main_Data::game_system = std::make_unique<Game_System>();
	Main_Data::game_system->SetSystemGraphic(
		CACHE_DEFAULT_BITMAP,
		lcf::rpg::System::Stretch_stretch,
		lcf::rpg::System::Font_gothic
	);

	games.clear();
	current_server.clear();
	LoadServers();
	ShowServerList();
}

void Scene_ServerBrowser::DrawBackground(Bitmap&) {
	// intentionally blank — use engine default background
}

std::string Scene_ServerBrowser::GetServersFilePath() {
#ifdef _WIN32
	const char* base = std::getenv("APPDATA");
	std::string dir = base ? base : ".";
	dir += "/easyrpg-player";
#else
	const char* base = std::getenv("HOME");
	std::string dir = base ? base : ".";
	dir += "/.config/easyrpg-player";
#endif
	return dir + "/servers.json";
}

void Scene_ServerBrowser::LoadServers() {
	servers.clear();
	std::string path = GetServersFilePath();

	std::ifstream f(path);
	if (!f.is_open()) {
		// Create an empty config so the user knows where to add entries
		std::error_code ec;
		std::filesystem::create_directories(
			std::filesystem::path(path).parent_path(), ec
		);
		std::ofstream out(path);
		if (out.is_open())
			out << "[]\n";
		return;
	}

	json j = json::parse(f, nullptr, false);
	if (j.is_discarded() || !j.is_array()) {
		Output::Warning("servers.json is not a valid JSON array: {}", path);
		return;
	}

	for (const auto& entry : j) {
		if (entry.is_string())
			servers.push_back(entry.get<std::string>());
	}

	if (servers.empty()) {
		servers.push_back("http://localhost:12321");
	}
}

void Scene_ServerBrowser::ShowServerList() {
	state = State::ServerList;

	help_window = std::make_unique<Window_Help>(0, 0, Player::screen_width, 32);
	help_window->SetText("Server Browser");

	status_window = std::make_unique<Window_Help>(
		Player::screen_width / 4,
		Player::screen_height / 2 - 16,
		Player::screen_width / 2,
		32
	);
	status_window->SetText("Fetching...");
	status_window->SetVisible(false);

	std::vector<std::string> items;
	if (servers.empty()) {
		items.push_back("(No servers — edit servers.json to add)");
	} else {
		for (const auto& s : servers)
			items.push_back(s);
	}
	items.push_back("[Back]");

	list_window = std::make_unique<Window_Command>(items, Player::screen_width);
	list_window->SetX(0);
	list_window->SetY(32);
	list_window->SetWidth(Player::screen_width);
	list_window->SetHeight(Player::screen_height - 32);

	if (servers.empty())
		list_window->DisableItem(0);
}

void Scene_ServerBrowser::ShowGameList() {
	state = State::GameList;

	std::string title = "Games on: " + current_server;
	help_window = std::make_unique<Window_Help>(0, 0, Player::screen_width, 32);
	help_window->SetText(title);

	std::vector<std::string> items;
	for (const auto& g : games)
		items.push_back(g.title.empty() ? g.name : g.title);
	items.push_back("[Back]");

	list_window = std::make_unique<Window_Command>(items, Player::screen_width);
	list_window->SetX(0);
	list_window->SetY(32);
	list_window->SetWidth(Player::screen_width);
	list_window->SetHeight(Player::screen_height - 32);
}

void Scene_ServerBrowser::vUpdate() {
	if (state == State::Fetching) {
		if (fetch_future.valid() &&
			fetch_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			games = fetch_future.get();
			status_window->SetVisible(false);
			if (games.empty()) {
				Output::Warning("No games returned from server: {}", current_server);
				ShowServerList();
			} else {
				ShowGameList();
			}
		}
		return;
	}

	if (list_window)
		list_window->Update();

	if (state == State::ServerList)
		UpdateServerList();
	else if (state == State::GameList)
		UpdateGameList();
}

void Scene_ServerBrowser::UpdateServerList() {
	if (Input::IsTriggered(Input::CANCEL)) {
		Scene::Pop();
		return;
	}

	if (!Input::IsTriggered(Input::DECISION))
		return;

	int idx = list_window->GetIndex();
	// The last item is always "[Back]"
	int back_idx = servers.empty() ? 1 : static_cast<int>(servers.size());

	if (servers.empty() || idx == back_idx) {
		Scene::Pop();
		return;
	}

	current_server = servers[idx];
	BeginFetch(current_server);
}

void Scene_ServerBrowser::UpdateGameList() {
	if (Input::IsTriggered(Input::CANCEL)) {
		ShowServerList();
		return;
	}

	if (!Input::IsTriggered(Input::DECISION))
		return;

	int idx = list_window->GetIndex();
	int back_idx = static_cast<int>(games.size());

	if (idx == back_idx) {
		ShowServerList();
		return;
	}

	LaunchGame(games[idx]);
}

void Scene_ServerBrowser::BeginFetch(const std::string& server_url) {
	state = State::Fetching;
	if (status_window) {
		status_window->SetText("Fetching...");
		status_window->SetVisible(true);
	}

	std::string url = server_url;
	if (!url.empty() && url.back() == '/')
		url.pop_back();
	url += "/_rpgmk_index.json";

	fetch_future = std::async(std::launch::async, [url]() -> std::vector<GameEntry> {
		auto resp = cpr::Get(cpr::Url{url});
		if (!(resp.status_code >= 200 && resp.status_code < 300)) {
			Output::Warning("Failed to fetch {}: HTTP {}", url, resp.status_code);
			return {};
		}

		auto j = json::parse(resp.text, nullptr, false);
		if (j.is_discarded() || !j.contains("games") || !j["games"].is_array()) {
			Output::Warning("Invalid /_rpgmk_index.json from {}", url);
			return {};
		}

		std::vector<GameEntry> result;
		for (const auto& g : j["games"]) {
			if (!g.is_object() || !g.contains("name") || !g["name"].is_string())
				continue;
			GameEntry entry;
			entry.name     = g["name"].get<std::string>();
			entry.title    = g.value("title",    entry.name);
			entry.assets   = g.value("assets",   std::string{});
			entry.sessions = g.value("sessions", std::string{});
			result.push_back(std::move(entry));
		}
		return result;
	});
}

void Scene_ServerBrowser::LaunchGame(const GameEntry& entry) {
	Player::emscripten_game_name = entry.name;
	Player::server_assets_url    = entry.assets;
	Player::server_sessions_url  = entry.sessions;

#ifdef _WIN32
	const char* base = std::getenv("APPDATA");
	std::string userhome = base ? base : ".";
	userhome += "/ynoproject";
#else
	const char* base = std::getenv("HOME");
	std::string userhome = base ? base : ".";
	userhome += "/.local/ynoproject";
#endif

	userhome = FileFinder::MakeCanonical(
		FileFinder::MakePath(userhome, entry.name)
	);
	Platform::File(userhome).MakeDirectory(true);

	old_pwd = std::filesystem::current_path();
	std::filesystem::current_path(userhome);

	GMI().SyncSaveFile();
	Game_Clock::ResetFrame(Game_Clock::now());
	Scene::Push(std::make_shared<Scene_Logo>());
}
