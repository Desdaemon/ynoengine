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

#ifndef EP_EMSCRIPTEN_INTERFACE_H
#define EP_EMSCRIPTEN_INTERFACE_H

#include <string>
#include <array>

class Emscripten_Interface {
public:
	static bool DownloadSavegame(int slot);
	static void UploadSavegame(int slot);
    static void UploadSoundfont();
    static void UploadFont();
    static void RefreshScene();
	static void TakeScreenshot(bool is_auto_screenshot = false);
	static void Reset();

	static std::array<int, 2> GetPlayerCoords();
	static void SetLanguage(std::string lang);
	static void SessionReady();
	static void TogglePlayerSounds();
	static void ToggleMute();
	static void SetSoundVolume(int volume);
	static void SetMusicVolume(int volume);
	static void SetNametagMode(int mode);
	static void SetSessionToken(std::string t);
	static bool ResetCanvas();

	static void PreloadFile(std::string dir, std::string path, bool graphic);
};

class Emscripten_Interface_Private {
public:
	static bool UploadSavegameStep2(int slot, int buffer_addr, int size);
    static bool UploadFontStep2(std::string name, int buffer_addr, int size);
    static bool UploadSoundfontStep2(std::string name, int buffer_addr, int size);
};

#endif
