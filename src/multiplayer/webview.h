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

#ifndef EP_MP_WEBVIEW_EXPORT_H
#define EP_MP_WEBVIEW_EXPORT_H

// include this file in place of webview/webview.h
// for compatibility with X11 types

#define Drawable XDrawable
#define Font XFont
#define Window XID
#define None XNone

#include <webview/webview.h>
#include <SDL_syswm.h>

#undef Drawable
#undef Font
#undef Window
#undef None

#endif
