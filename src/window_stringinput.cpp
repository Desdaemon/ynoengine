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

#include "window_stringinput.h"
#include "input.h"
#include "bitmap.h"

Window_StringInput::Window_StringInput(StringView initial_value, int ix, int iy, int iwidth, int iheight) :
	Window_Selectable(ix, iy, iwidth, iheight), value(initial_value)
{
	SetContents(Bitmap::Create(width - 16, height - 16));
	// Above the message window
	SetZ(Priority_Window + 150);
	opacity = 0;
	active = false;

	Refresh();
}

void Window_StringInput::Refresh() {
	contents->Clear();
	contents->TextDraw(0, 2, Font::ColorDefault, value);
	contents->FillRect(GetCursorRect(), Color(255, 255, 255, 200));
}

void Window_StringInput::Update() {
	Window_Selectable::Update();
	if (!active) return;

	bool dirty = true;

	constexpr int offset = U'A' - Input::Keys::A;
	constexpr int uppercase = U'a' - U'A';
	bool shift = Input::IsRawKeyPressed(Input::Keys::LSHIFT) || Input::IsRawKeyPressed(Input::Keys::RSHIFT);
	static std::map<Input::Keys::InputKey, char> symbols{
		{Input::Keys::N0, '0'},
		{Input::Keys::N1, '1'},
		{Input::Keys::N2, '2'},
		{Input::Keys::N3, '3'},
		{Input::Keys::N4, '4'},
		{Input::Keys::N5, '5'},
		{Input::Keys::N6, '6'},
		{Input::Keys::N7, '7'},
		{Input::Keys::N8, '8'},
		{Input::Keys::N9, '9'},
		{Input::Keys::SPACE, ' '},
	};
	for (int letter = Input::Keys::A; letter <= Input::Keys::Z; ++letter) {
		if (Input::IsRawKeyTriggered((Input::Keys::InputKey)letter)) {
			value.push_back(letter + offset + (shift ? 0 : uppercase));
		}
	}
	if (Input::IsRawKeyTriggered(Input::Keys::BACKSPACE)) {
		value.clear();
	}
	Refresh();
}
