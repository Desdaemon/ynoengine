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

    currentFrame = 0;
}

ImageGif::Decoder::~Decoder() noexcept {
	if (gifFile) DGifCloseFile(gifFile, nullptr);
}

bool ImageGif::Decoder::ReadNext(ImageOut& output, GifTimingInfo& timing) {
    memset(&output, 0, sizeof(ImageOut));
    memset(&timing, 0, sizeof(GifTimingInfo));

    if (!gifFile || currentFrame >= gifFile->ImageCount) {
        output.pixels = nullptr;
        return false;
    }

    const SavedImage* frame = &gifFile->SavedImages[currentFrame];
    const GifImageDesc& image = frame->ImageDesc;

    ColorMapObject* colorMap = image.ColorMap ? image.ColorMap : gifFile->SColorMap;
    if (!colorMap) {
        Output::Warning("ImageGif: No color map found for frame {}", currentFrame);
        output.pixels = nullptr;
        return false;
    }

    output.width = image.Width;
    output.height = image.Height;
	output.bpp = 32;
    output.pixels = new uint32_t[image.Width * image.Height];

    const int left = image.Left;
    const int top = image.Top;
    const int width = image.Width;
    const int height = image.Height;
    const int bg = gifFile->SBackGroundColor;

    const GifPixelType* src = frame->RasterBits;
    // const GifPixelType* srcPrev = currentFrame > 0 ? gifFile->SavedImages[currentFrame - 1].RasterBits : nullptr;

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

    size_t srcIndex = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            GifPixelType index = src[srcIndex++];
            if (index == transparentIndex) {
                ((uint32_t*)output.pixels)[(top + y) * output.width + (left + x)] = 0; // Transparent pixel
                continue;
            }
            // if (disposalMethod == 2)
            //     ((uint32_t*)output.pixels)[(top + y) * output.width + (left + x)] = bg;
            if (index < colorMap->ColorCount) {
                const GifColorType& color = colorMap->Colors[index];
                int dstX = left + x;
                int dstY = top + y;
                if (dstX < output.width && dstY < output.height) {
                    ((uint32_t*)output.pixels)[dstY * output.width + dstX] =
                        color.Red | (color.Green << 8) | (color.Blue << 16) | 0xFF000000;
                }
            }
        }
    }

    currentFrame++;
    return true;
}
