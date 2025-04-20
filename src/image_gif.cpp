#include "image_gif.h"
#include <gif_lib.h>
#include "output.h"

static int read_data(GifFileType* gifFile, GifByteType* buffer, int size) {
    auto* bufp = reinterpret_cast<Filesystem_Stream::InputStream*>(gifFile->UserData);
    if (bufp && *bufp) {
        bufp->read(reinterpret_cast<char*>(buffer), size);
        if (bufp->fail()) return 0; // Read failed
        return size; // Return number of bytes read
    }
    return 0; // No data available
}

ImageGif::Decoder::Decoder(Filesystem_Stream::InputStream& is) noexcept : ImageGif::Decoder() {
	int error = D_GIF_SUCCEEDED;
    gifFile = DGifOpen(&is, read_data, &error);

    if (!gifFile || error != D_GIF_SUCCEEDED) {
        Output::Warning("ImageGif: Failed to open GIF file: {} ({})", is.GetName(), GifErrorString(error));
        return;
    }

    if (DGifSlurp(gifFile) != GIF_OK) {
        Output::Warning("ImageGif: Failed to read GIF data: {} ({})", is.GetName());
        return;
    }

    scratch.resize(gifFile->SWidth * gifFile->SHeight);
    std::fill(scratch.begin(), scratch.end(), gifFile->SBackGroundColor | 0xFF000000);

    currentFrame = 0;
}

ImageGif::Decoder::~Decoder() noexcept {
	if (gifFile) DGifCloseFile(gifFile, nullptr);
}

bool ImageGif::Decoder::ReadNext(ImageOut& output, GifTimingInfo& timing) {
    if (!gifFile || currentFrame >= gifFile->ImageCount) {
        output.pixels = nullptr;
        return false;
    }

    const SavedImage* frame = &gifFile->SavedImages[currentFrame];
    const GifImageDesc& frameInfo = frame->ImageDesc;

    ColorMapObject* colorMap = frameInfo.ColorMap ? frameInfo.ColorMap : gifFile->SColorMap;
    if (!colorMap) {
        Output::Warning("ImageGif: No color map found for frame {}", currentFrame);
        output.pixels = nullptr;
        return false;
    }

    const int fullWidth = gifFile->SWidth;
    const int fullHeight = gifFile->SHeight;
    output.width = fullWidth;
    output.height = fullHeight;
	output.bpp = 0;

    const int left = frameInfo.Left;
    const int top = frameInfo.Top;
    const int width = frameInfo.Width;
    const int height = frameInfo.Height;
    const int bg = gifFile->SBackGroundColor;
    timing.delay = 0;

    const GifPixelType* src = frame->RasterBits;

    int transparentIndex = -1;
    int disposalMethod = 0;

    for (int i = 0; i < frame->ExtensionBlockCount; ++i) {
        if (frame->ExtensionBlocks[i].Function == GRAPHICS_EXT_FUNC_CODE) {
            uint8_t fields = frame->ExtensionBlocks[i].Bytes[0];
            disposalMethod = (fields & 0x1C) >> 2; // 3 bits for disposal method
            bool transparency = (fields & 0x01) != 0; // 1 bit for transparency
            if (transparency) {
                transparentIndex = frame->ExtensionBlocks[i].Bytes[3]; // Transparency index
            }
            timing.delay = ((int)frame->ExtensionBlocks[i].Bytes[1] | ((int)frame->ExtensionBlocks[i].Bytes[2] << 8)) * 10; // Convert to milliseconds
            break;
        }
    }

    if (disposalMethod == 2) {
        std::fill(scratch.begin(), scratch.end(), bg | 0xFF000000); // Fill with background color
    }

    size_t srcIndex = 0;
    for (int y = top; y < top + height; ++y) {
        for (int x = left; x < left + width; ++x) {
            GifPixelType index = src[srcIndex++];
            if (index == transparentIndex) {
                if (disposalMethod != 1)
                    scratch[y * fullWidth + x] = 0; // Transparent pixel
            } else if (index < colorMap->ColorCount) {
                const GifColorType& color = colorMap->Colors[index];
                scratch[y * fullWidth + x] =
                    color.Red | (color.Green << 8) | (color.Blue << 16) | 0xFF000000;
            }
        }
    }

    output.pixels = new uint32_t[fullWidth * fullHeight];
    memcpy(output.pixels, scratch.data(), fullWidth * fullHeight * sizeof(uint32_t));

    currentFrame++;
    return true;
}
