// Generates the S6-E1 JPEG fixtures with libjpeg-turbo: baseline, progressive, grayscale, CMYK,
// and the EXIF orientation variants (1, 3, 6, 8, invalid 9, and a contradictory pair).
//
// Usage: cuexis_jpeg_fixtures <output-directory>

#include <stdio.h> // jpeglib.h needs FILE before it is included

#include <jpeglib.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr int imageWidth = 4;
constexpr int imageHeight = 2;

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

// A fixed 4x2 pattern with a distinct value in every pixel and channel.
[[nodiscard]] auto pattern() -> std::array<Rgb, imageWidth * imageHeight> {
    std::array<Rgb, imageWidth * imageHeight> pixels{};
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            const auto index = static_cast<std::size_t>(y * imageWidth + x);
            pixels[index] = Rgb{static_cast<std::uint8_t>(20 + 50 * x),
                                static_cast<std::uint8_t>(200 - 40 * y),
                                static_cast<std::uint8_t>(10 + 30 * (x + y))};
        }
    }
    return pixels;
}

// A minimal TIFF/Exif APP1 payload holding one orientation tag. When secondOrientation is not
// zero the IFD holds two conflicting orientation entries.
[[nodiscard]] auto exifApp1(std::uint16_t orientation, std::uint16_t secondOrientation)
    -> std::vector<std::uint8_t> {
    const std::uint16_t entries = secondOrientation == 0 ? 1 : 2;
    const std::uint32_t ifdBytes = 2 + 12 * entries + 4;

    std::vector<std::uint8_t> out;
    out.resize(6 + 8 + ifdBytes, 0);
    std::memcpy(out.data(), "Exif\0\0", 6);
    auto* tiff = out.data() + 6;
    // Big-endian ("MM") TIFF header with the first IFD right after it.
    tiff[0] = 'M';
    tiff[1] = 'M';
    tiff[2] = 0;
    tiff[3] = 42;
    const auto writeU16 = [](std::uint8_t* at, std::uint16_t value) {
        at[0] = static_cast<std::uint8_t>(value >> 8U);
        at[1] = static_cast<std::uint8_t>(value & 0xFFU);
    };
    const auto writeU32 = [](std::uint8_t* at, std::uint32_t value) {
        at[0] = static_cast<std::uint8_t>(value >> 24U);
        at[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        at[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        at[3] = static_cast<std::uint8_t>(value & 0xFFU);
    };
    writeU32(tiff + 4, 8);
    auto* ifd = tiff + 8;
    writeU16(ifd, entries);
    auto* entry = ifd + 2;
    // Tag 0x0112 (Orientation), type 3 (SHORT), count 1, value inline.
    writeU16(entry, 0x0112);
    writeU16(entry + 2, 3);
    writeU32(entry + 4, 1);
    writeU16(entry + 8, orientation);
    writeU16(entry + 10, 0);
    if (secondOrientation != 0) {
        writeU16(entry + 12, 0x0112);
        writeU16(entry + 14, 3);
        writeU32(entry + 16, 1);
        writeU16(entry + 20, secondOrientation);
        writeU16(entry + 22, 0);
    }
    writeU32(ifd + 2 + 12 * entries, 0);
    return out;
}

struct ErrorManager {
    jpeg_error_mgr base{};
};

void fail(j_common_ptr info) {
    char message[JMSG_LENGTH_MAX]{};
    (*info->err->format_message)(info, message);
    std::fprintf(stderr, "libjpeg: %s\n", message);
    std::exit(1);
}

void writeJpeg(const std::filesystem::path& path, int colorSpace, int outputColorSpace,
               bool progressive, const std::vector<std::uint8_t>& app1) {
    const auto pixels = pattern();
    std::vector<std::uint8_t> image;
    image.reserve(pixels.size() * 4);
    for (const auto& pixel : pixels) {
        if (colorSpace == JCS_GRAYSCALE) {
            image.push_back(pixel.g);
        } else if (colorSpace == JCS_CMYK) {
            // Device CMYK with no colour management; the profile rejects this.
            image.push_back(static_cast<std::uint8_t>(255 - pixel.r));
            image.push_back(static_cast<std::uint8_t>(255 - pixel.g));
            image.push_back(static_cast<std::uint8_t>(255 - pixel.b));
            image.push_back(0);
        } else {
            image.push_back(pixel.r);
            image.push_back(pixel.g);
            image.push_back(pixel.b);
        }
    }

    FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path.string().c_str());
        std::exit(1);
    }

    jpeg_compress_struct info{};
    ErrorManager manager{};
    info.err = jpeg_std_error(&manager.base);
    manager.base.error_exit = fail;
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);
    info.image_width = imageWidth;
    info.image_height = imageHeight;
    info.input_components = colorSpace == JCS_GRAYSCALE ? 1 : (colorSpace == JCS_CMYK ? 4 : 3);
    info.in_color_space = static_cast<J_COLOR_SPACE>(colorSpace);
    jpeg_set_defaults(&info);
    // Grayscale stays grayscale; RGB input keeps the default YCbCr output, which is what an
    // ordinary JPEG holds. The four-component cases ask for CMYK or YCCK explicitly.
    jpeg_set_colorspace(&info, static_cast<J_COLOR_SPACE>(outputColorSpace));
    if (colorSpace == JCS_CMYK) {
        info.write_Adobe_marker = TRUE;
    }
    jpeg_set_quality(&info, 90, TRUE);
    info.smoothing_factor = 0;
    if (progressive) {
        jpeg_simple_progression(&info);
    }
    jpeg_start_compress(&info, TRUE);
    if (!app1.empty()) {
        jpeg_write_marker(&info, JPEG_APP0 + 1, app1.data(),
                          static_cast<unsigned int>(app1.size()));
    }
    const auto stride = static_cast<std::size_t>(info.input_components * imageWidth);
    while (info.next_scanline < info.image_height) {
        JSAMPROW row = image.data() + static_cast<std::size_t>(info.next_scanline) * stride;
        jpeg_write_scanlines(&info, &row, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::fclose(file);
    std::printf("%s\n", path.filename().string().c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: cuexis_jpeg_fixtures <output-directory>\n");
        return 1;
    }
    const std::filesystem::path out{argv[1]};
    std::filesystem::create_directories(out);

    writeJpeg(out / "baseline.jpg", JCS_RGB, JCS_YCbCr, false, {});
    writeJpeg(out / "progressive.jpg", JCS_RGB, JCS_YCbCr, true, {});
    writeJpeg(out / "gray.jpg", JCS_GRAYSCALE, JCS_GRAYSCALE, false, {});
    writeJpeg(out / "cmyk.jpg", JCS_CMYK, JCS_CMYK, false, {});
    writeJpeg(out / "ycck.jpg", JCS_CMYK, JCS_YCCK, false, {});
    writeJpeg(out / "orientation1.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(1, 0));
    writeJpeg(out / "orientation3.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(3, 0));
    writeJpeg(out / "orientation6.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(6, 0));
    writeJpeg(out / "orientation8.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(8, 0));
    writeJpeg(out / "orientation_invalid.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(9, 0));
    writeJpeg(out / "orientation_conflict.jpg", JCS_RGB, JCS_YCbCr, false, exifApp1(6, 8));
    return 0;
}
