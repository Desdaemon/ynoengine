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

#ifndef EP_WINDOW_STRINGINPUT_H
#define EP_WINDOW_STRINGINPUT_H

#include "window_selectable.h"

class Window_StringInput : public Window_Selectable {
public:
	Window_StringInput(StringView initial_value, int ix, int iy, int iwidth = 320, int iheight = 80);

	void Refresh();
	void Update() override;
	std::string value;
};

#endif
