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

#include "image_webp.h"
#include <webp/demux.h>
#include "output.h"

ImageWebP::Decoder::~Decoder() noexcept {
    if (decoder) WebPAnimDecoderDelete(decoder);
    WebPDataClear(&data);
}

ImageWebP::Decoder::Decoder(Filesystem_Stream::InputStream& is) noexcept {
    // is.read(reinterpret_cast<char*>(&dec.data.bytes));
    // WebPMalloc
    data.size = is.GetSize();
    data.bytes = (uint8_t*)WebPMalloc(data.size + 1);
    ((char*)data.bytes)[data.size] = 0;
    is.read((char*)data.bytes, data.size);
    if (is.fail()) return;

    if (!WebPGetInfo(data.bytes, data.size, nullptr, nullptr)) {
        Output::Warning("ImageWebP: {} is not webp", is.GetName());
        return;
    }

    decoder = WebPAnimDecoderNew(&data, nullptr);
    if (!decoder) {
        Output::Warning("ImageWebP: Failed to create decoder for {}", is.GetName());
        return;
    }

    if (!WebPAnimDecoderGetInfo(decoder, &animData)) {
        if (decoder) WebPAnimDecoderDelete(decoder);
        decoder = nullptr;
        Output::Warning("ImageWebP: Failed to get animation info for {}", is.GetName());
        return;
    }
}


bool ImageWebP::Decoder::ReadNext(ImageOut& output, TimingInfo& timing) {
    if (!WebPAnimDecoderHasMoreFrames(decoder)) {
        output.pixels = nullptr;
        return false;
    }

    uint8_t* pixels;
    if (!WebPAnimDecoderGetNext(decoder, &pixels, &timing.timestamp)) {
        Output::Warning("ImageWebP: Failed to decode next frame");
        return false;
    }
    output.pixels = new uint32_t[animData.canvas_width * animData.canvas_height];
    memcpy(output.pixels, pixels, animData.canvas_width * animData.canvas_height * sizeof(uint32_t));
    output.height = animData.canvas_height;
    output.width = animData.canvas_width;

    return true;
}