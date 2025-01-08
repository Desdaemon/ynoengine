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
#include "baseui.h"
#include "cache.h"
#include "output.h"

Window_StringInput::Window_StringInput(StringView initial_value, int ix, int iy, int iwidth, int iheight) :
	Window_Selectable(ix, iy, iwidth, iheight), value(initial_value)
{
	SetContents(Bitmap::Create(width - 16, height - 16));
	// Above the message window
	SetZ(Priority_Window + 150);
	opacity = 0;
	active = false;

	Rect rect{ ix, iy + 2, width - 16, height - 16 };
	DisplayUi->BeginTextCapture(&rect);

	Refresh();
}

Window_StringInput::~Window_StringInput() {
	Window_Selectable::~Window_Selectable();
	DisplayUi->EndTextCapture();
}

void Window_StringInput::SetSecret(bool secret) {
	this->secret = secret;
	Refresh();
}

void Window_StringInput::Refresh() {
	contents->Clear();
	int offset = 0;
	const int y = 2;
	if (!secret)
		offset += contents->TextDraw(0, y, Font::ColorDefault, value).x;
	else {
		std::string masked(value.size(), '*');
		offset += contents->TextDraw(0, y, Font::ColorDefault, masked).x;
	}
	if (Input::composition.active) {
		offset += Input::RenderTextComposition(*contents, offset, y, this->font.get());
	}
	contents->FillRect(GetCursorRect(), Color(255, 255, 255, 200));
}

void Window_StringInput::Update() {
	Window_Selectable::Update();
	if (!active) return;

	bool dirty = false;

	//StringView input(Input::text_input.data());
	if (!Input::text_input.empty()) {
		value.append(Input::text_input);
		dirty = true;
		Input::text_input.clear();
	} else if (Input::composition.active) {
		dirty = true;
	}

	if (Input::IsRawKeyTriggered(Input::Keys::BACKSPACE) && !value.empty()) {
		value.clear();
		dirty = true;
	}
	if (dirty)
		Refresh();
}
