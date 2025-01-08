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

#include "icons.h"
#include "ynoicons.h"
#include "output.h"

namespace {
	/** formatted in the same way as an exfont */
	BitmapRef ynoicons = nullptr;
	BitmapRef glyph_bm = nullptr;

	void InitializeYnoIcons() {
		ynoicons = Bitmap::Create(resources_ynoicons_png, sizeof(resources_ynoicons_png));
		if (!ynoicons)
			Output::Error("bug: invalid ynoicons");
		glyph_bm = Bitmap::Create(12, 12);
	}
}

int Icons::RenderIcon(Bitmap& dst, int x, int y, YnoIcons icon_index, const Color& color, const Font& font) {
	if (!ynoicons)
		InitializeYnoIcons();

	constexpr int dim = 12;
	Rect icon_rect{ ((int)icon_index % 13) * dim, ((int)icon_index / 13) * dim, dim, dim};
	glyph_bm->Clear();
	glyph_bm->BlitFast(0, 0, *ynoicons, icon_rect, Opacity::Opaque());

	// TODO: support for system theming a la fonts
	double zoom = font.GetScaleRatio();
	int dim_zoom = ceilf(zoom * dim);
	Rect dst_rect{ x, y, dim_zoom, dim_zoom };
	dst.MaskedBlit(dst_rect, *glyph_bm, 0, 0, color, 1 / zoom, 1 / zoom);
	return dim_zoom;
}
