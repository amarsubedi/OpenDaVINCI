/**
 * OpenDaVINCI - Portable middleware for distributed components.
 * Copyright (C) 2015 Christian Berger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "lodepng.h"

#include "opendavinci/odcore/wrapper/png/PNG.h"

namespace odcore {
    namespace wrapper {
        namespace png {

            using namespace std;

            PNG::PNG() {}

            PNG::~PNG() {}

            unsigned char* PNG::compress(const uint8_t *rawImageData, const uint32_t &width, const uint32_t &height, const uint32_t &bytesPerPixel, uint32_t &destSize, uint8_t &returnCode) {
                unsigned char* imageData = NULL;

                if ( (width > 0) &&
                     (height > 0) &&
                     (bytesPerPixel > 0) &&
                     (rawImageData != NULL) ) {
                    if (1 == bytesPerPixel) {
                        // Gray scale.
                        const unsigned BIT_DEPTH = 8;
                        returnCode = lodepng_encode_memory(&imageData, (size_t*)(&destSize), rawImageData, width, height, LCT_GREY, BIT_DEPTH);
                    }
                    else if (3 == bytesPerPixel) {
                        // RGB.
                        const unsigned BIT_DEPTH = 8;
                        returnCode = lodepng_encode_memory(&imageData, (size_t*)(&destSize), rawImageData, width, height, LCT_RGB, BIT_DEPTH);
                    }

                    // Check for errors.
                    if (0 != returnCode) {
                        if (imageData != NULL) {
                            ::free(imageData);
                        }
                        imageData = NULL;
                    }

                }

                return imageData;
            }

            unsigned char* PNG::decompress(const unsigned char *src, const uint32_t &srcSize, uint32_t *width, uint32_t *height, uint8_t *actualBytesPerPixel, const uint8_t &requestedBytesPerPixel, uint8_t &returnCode) {
                unsigned char* imageData = NULL;

                if ( (src != NULL) && 
                     (srcSize > 0) && 
                     (requestedBytesPerPixel > 0) ) {
                    if (1 == requestedBytesPerPixel) {
                        // Gray scale.
                        const unsigned BIT_DEPTH = 8;
                        returnCode = lodepng_decode_memory(&imageData, width, height, src, srcSize, LCT_GREY, BIT_DEPTH);
                    }
                    else if (3 == requestedBytesPerPixel) {
                        // RGB.
                        const unsigned BIT_DEPTH = 8;
                        returnCode = lodepng_decode_memory(&imageData, width, height, src, srcSize, LCT_RGB, BIT_DEPTH);
                    }

                    // Check for errors.
                    if (0 == returnCode) {
                        *actualBytesPerPixel = requestedBytesPerPixel;
                    }
                    else {
                        if (imageData != NULL) {
                            ::free(imageData);
                        }
                        actualBytesPerPixel = 0;
                        imageData = NULL;
                    }
                }
                return imageData;
            }

        }
    }
} // odcore::wrapper::jpg

