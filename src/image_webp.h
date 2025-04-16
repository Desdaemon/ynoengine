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

#ifndef EP_IMAGE_WEBP_H
#define EP_IMAGE_WEBP_H

#include <cstdint>
#include <optional>
#include "bitmap.h"
#include "filesystem_stream.h"
#include <webp/demux.h>

struct TimingInfo {
    int timestamp;
};

namespace ImageWebP {
    struct Decoder { 
        ~Decoder() noexcept;

        bool ReadNext(ImageOut& output, TimingInfo& timing);
        static std::optional<Decoder> Create(Filesystem_Stream::InputStream& is) noexcept;
    private:
        explicit Decoder() noexcept = default;
        WebPAnimDecoder* decoder;
        WebPData data;
        WebPAnimInfo animData;
    };
}

#endif