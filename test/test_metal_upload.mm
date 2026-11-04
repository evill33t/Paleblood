/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime/graphics_metal.h"
#import <Foundation/Foundation.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__arm64__) || defined(__aarch64__)
static NSString *const compiled_architecture = @"arm64";
#elif defined(__x86_64__)
static NSString *const compiled_architecture = @"x86_64";
#else
static NSString *const compiled_architecture = @"unknown";
#endif

// Distinct channels and rows make a swizzle, dropped alpha or vertical flip observable.
static std::array<uint8_t, 4> pixel(size_t x, size_t y)
{
    return {static_cast<uint8_t>(11 + 17 * x + 3 * y),
            static_cast<uint8_t>(97 + 5 * x + 41 * y),
            static_cast<uint8_t>(201 - 13 * x - 7 * y),
            static_cast<uint8_t>(255 - 19 * x - 11 * y)};
}

int main(void)
{
    @autoreleasepool {
        NSMutableArray<NSString *> *failures = [NSMutableArray array];
        unsigned checks = 0, uploads = 0, clears = 0, pixels_checked = 0;
        const auto check = [&](bool good, const char *message) {
            checks++;
            if (!good) [failures addObject:[NSString stringWithUTF8String:message]];
        };
        char error[256] = {};
        rt_metal_offscreen *context = nullptr;
        const uint8_t marker[4] = {0, 255, 128, 0};
        check(rt_metal_offscreen_upload(nullptr, marker, 4, 4, nullptr, 0) == RT_METAL_INVALID_ARGUMENT,
              "null upload context rejected");
        const rt_metal_status created = rt_metal_offscreen_create(7, 5, &context, error, sizeof error);
        check(created == RT_METAL_OK && context, "real shared Metal texture created");
        NSString *device = context ? [NSString stringWithUTF8String:rt_metal_offscreen_device_name(context)] : @"unavailable";
        NSString *creation_error = [NSString stringWithUTF8String:error];
        if (context) {
            constexpr size_t width = 7, height = 5, row = width * 4;
            constexpr size_t source_stride = row + 3, source_needed = (height - 1) * source_stride + row;
            constexpr size_t stride = row + 13, guard = 9, needed = (height - 1) * stride + row;
            std::vector<uint8_t> output(guard + stride * height + guard, 0xa5);
            const auto untouched = output;
            const auto read_pattern = [&]() {
                output.assign(output.size(), 0xa5);
                const rt_metal_status read = rt_metal_offscreen_read(context, output.data() + guard, stride, needed, error, sizeof error);
                check(read == RT_METAL_OK, "uploaded texture read through Metal");
                if (read != RT_METAL_OK) return;
                bool pixels = true, padding = true;
                for (size_t y = 0; y < height; y++) {
                    for (size_t x = 0; x < width; x++) {
                        const auto expected = pixel(x, y);
                        pixels &= std::memcmp(output.data() + guard + y * stride + x * 4, expected.data(), 4) == 0;
                        pixels_checked++;
                    }
                    for (size_t x = row; x < stride; x++) padding &= output[guard + y * stride + x] == 0xa5;
                }
                for (size_t x = 0; x < guard; x++) padding &= output[x] == 0xa5 && output[output.size() - 1 - x] == 0xa5;
                check(pixels, "every uploaded channel and row matches the independent pixel pattern");
                check(padding, "readback padding and surrounding canaries preserved");
            };
            check(rt_metal_offscreen_upload(context, nullptr, source_stride, source_needed, error, sizeof error) == RT_METAL_INVALID_ARGUMENT,
                  "null source rejected before any upload");
            check(rt_metal_offscreen_upload(context, marker, row - 1, source_needed, error, sizeof error) == RT_METAL_INVALID_ARGUMENT,
                  "short stride rejected before reading source");
            check(rt_metal_offscreen_read(context, output.data() + guard, stride, needed, error, sizeof error) == RT_METAL_NOT_READY && output == untouched,
                  "invalid first upload leaves texture unreadable and destination unchanged");

            // The final source pixel abuts an inaccessible page; padding after it is absent.
            const long page_size = sysconf(_SC_PAGESIZE);
            check(page_size > 0 && static_cast<size_t>(page_size) >= source_needed, "page can contain the exact source extent");
            if (page_size > 0 && static_cast<size_t>(page_size) >= source_needed) {
                const size_t page = static_cast<size_t>(page_size);
                void *mapping = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
                check(mapping != MAP_FAILED, "guarded source mapping allocated");
                if (mapping != MAP_FAILED) {
                    auto *source = static_cast<uint8_t *>(mapping) + page - source_needed;
                    std::memset(source, 0x6d, source_needed);
                    for (size_t y = 0; y < height; y++) {
                        for (size_t x = 0; x < width; x++) {
                            const auto value = pixel(x, y);
                            std::memcpy(source + y * source_stride + x * 4, value.data(), 4);
                        }
                    }
                    const std::vector<uint8_t> original(source, source + source_needed);
                    const bool protected_source = mprotect(static_cast<uint8_t *>(mapping) + page, page, PROT_NONE) == 0 &&
                                                  mprotect(mapping, page, PROT_READ) == 0;
                    check(protected_source, "source is read-only with no accessible trailing padding");
                    if (protected_source) {
                        const auto status = rt_metal_offscreen_upload(context, source, source_stride, source_needed, error, sizeof error);
                        check(status == RT_METAL_OK, "strided frame uploads without a prior GPU clear");
                        if (status == RT_METAL_OK) uploads++;
                        check(std::memcmp(source, original.data(), source_needed) == 0, "upload preserves every source byte including padding");
                    }
                    check(munmap(mapping, page * 2) == 0, "source mapping released immediately after upload returns");
                    read_pattern();
                }
            }

            // All rejected updates must retain the preceding successful frame.
            check(rt_metal_offscreen_upload(context, marker, source_stride, source_needed - 1, error, sizeof error) == RT_METAL_INVALID_ARGUMENT,
                  "short capacity rejected before reading source");
            check(rt_metal_offscreen_upload(context, marker, SIZE_MAX, SIZE_MAX, error, sizeof error) == RT_METAL_INVALID_ARGUMENT,
                  "stride arithmetic overflow rejected before reading source");
            check(rt_metal_offscreen_upload(context, nullptr, source_stride, source_needed, error, sizeof error) == RT_METAL_INVALID_ARGUMENT,
                  "null source preserves the preceding frame");
            char tiny_error[5] = {'x', 'x', 'x', 'x', 'z'};
            check(rt_metal_offscreen_upload(context, marker, 0, 0, tiny_error, 4) == RT_METAL_INVALID_ARGUMENT &&
                  tiny_error[3] == '\0' && tiny_error[4] == 'z', "diagnostic buffer bounded and nul terminated");
            read_pattern();

            const auto cleared = rt_metal_offscreen_clear(context, marker, 5000, error, sizeof error);
            check(cleared == RT_METAL_OK, "GPU clear safely follows CPU upload");
            if (cleared == RT_METAL_OK) clears++;
            std::vector<uint8_t> tight(row * height);
            check(rt_metal_offscreen_read(context, tight.data(), row, tight.size(), error, sizeof error) == RT_METAL_OK,
                  "GPU overwrite read back after upload");
            bool clear_pixels = true;
            for (size_t i = 0; i < width * height; i++) clear_pixels &= std::memcmp(tight.data() + i * 4, marker, 4) == 0;
            check(clear_pixels, "GPU clear overwrites every uploaded pixel");
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    const auto value = pixel(x, y);
                    std::memcpy(tight.data() + y * row + x * 4, value.data(), 4);
                }
            }
            const auto uploaded = rt_metal_offscreen_upload(context, tight.data(), row, tight.size(), error, sizeof error);
            check(uploaded == RT_METAL_OK, "tight CPU upload safely follows completed GPU clear");
            if (uploaded == RT_METAL_OK) uploads++;
            tight.assign(tight.size(), 0);
            read_pattern();
            rt_metal_offscreen_destroy(&context);
            check(context == nullptr, "uploaded texture lifecycle destroyed cleanly");

            check(rt_metal_offscreen_create(1, 1, &context, error, sizeof error) == RT_METAL_OK && context,
                  "single-pixel upload context created");
            if (context) {
                const auto single = rt_metal_offscreen_upload(context, marker, SIZE_MAX, sizeof marker, nullptr, 0);
                check(single == RT_METAL_OK, "single row requires only its pixels regardless of unused stride");
                if (single == RT_METAL_OK) uploads++;
                uint8_t actual[4] = {};
                check(rt_metal_offscreen_read(context, actual, sizeof actual, sizeof actual, nullptr, 0) == RT_METAL_OK &&
                      std::memcmp(actual, marker, sizeof actual) == 0, "zero and full-range RGBA bytes preserved");
                pixels_checked++;
                rt_metal_offscreen_destroy(&context);
            }
        }
        NSDictionary *record = @{@"schema":@1, @"status":failures.count ? @"failed" : @"pass",
            @"device":device ?: @"unavailable", @"architecture":compiled_architecture,
            @"checks":@(checks), @"CPU_texture_uploads":@(uploads), @"GPU_completed_clear_submissions":@(clears),
            @"pixels_checked":@(pixels_checked), @"failures":failures, @"creation_error":creation_error ?: @"",
            @"window_or_guest_execution":@NO, @"hardware_timeout_device_loss_OOM_paths_forced":@NO};
        NSData *json = [NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:nullptr];
        std::fwrite(json.bytes, 1, json.length, stdout); std::putchar('\n');
        return failures.count ? 1 : 0;
    }
}
