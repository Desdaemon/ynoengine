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

#ifndef EP_ICONS_H
#define EP_ICONS_H

#include "font.h"
#include "bitmap.h"
#include "color.h"

namespace Icons {
	enum class YnoIcons : char {
		connection_3,
		connection_2,
		connection_1,
		megaphone,
		person,
		compass,
		globe,
		map,
		updown,
		group,
		crown,
		wrench,
		shield,
	};

	int RenderIcon(Bitmap& dst, int x, int y, YnoIcons icon_index, const Color& color, const Font& font);
}

#endif
