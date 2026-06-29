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

#ifndef EP_MP_SCENE_SERVERBROWSER_H
#define EP_MP_SCENE_SERVERBROWSER_H

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include "scene.h"
#include "window_command.h"
#include "window_help.h"

/**
 * Server browser scene for PLAYER_MP builds.
 *
 * Shows a list of configured servers. When one is selected it fetches
 * /_rpgmk_index.json from that server, which lists available games along
 * with their assets base URL and sessions WebSocket URL. Selecting a game
 * sets Player::emscripten_game_name, Player::server_assets_url, and
 * Player::server_sessions_url before launching via Scene_Logo.
 *
 * Server list is persisted to $HOME/.config/easyrpg-player/servers.json
 * (or %APPDATA%/easyrpg-player/servers.json on Windows) as a JSON array
 * of URL strings.
 *
 * Expected /_rpgmk_index.json format:
 * {
 *   "games": [
 *     {
 *       "name":     "mygame",
 *       "title":    "My Game Title",
 *       "assets":   "https://myserver.com/data/mygame/",
 *       "sessions": "wss://myserver.com/mygame/"
 *     }
 *   ]
 * }
 */
class Scene_ServerBrowser : public Scene {
public:
	Scene_ServerBrowser();

	void Start() override;
	void Continue(SceneType prev_scene) override;
	void DrawBackground(Bitmap& dst) override;
	void vUpdate() override;

private:
	struct GameEntry {
		std::string name;
		std::string title;
		std::string assets;
		std::string sessions;
	};

	enum class State {
		ServerList,
		Fetching,
		GameList,
	};

	static std::string GetServersFilePath();
	void LoadServers();

	void ShowServerList();
	void ShowGameList();

	void UpdateServerList();
	void UpdateGameList();

	void BeginFetch(const std::string& server_url);
	void LaunchGame(const GameEntry& entry);

	std::vector<std::string> servers;
	std::vector<GameEntry> games;
	std::string current_server;

	std::unique_ptr<Window_Help> help_window;
	std::unique_ptr<Window_Command> list_window;
	std::unique_ptr<Window_Help> status_window;

	State state = State::ServerList;
	std::future<std::vector<GameEntry>> fetch_future;
	std::filesystem::path old_pwd;
};

#endif
