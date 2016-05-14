/**
 * OpenDaVINCI - Portable middleware for distributed components.
 * Copyright (C) 2008 - 2015 Christian Berger, Bernhard Rumpe
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

#ifndef ODCORE_PNGTESTSUITE_H_
#define ODCORE_PNGTESTSUITE_H_

#include "cxxtest/TestSuite.h"          // for TS_ASSERT, TestSuite

#include "opendavinci/odcore/wrapper/png/PNG.h"

using namespace std;

class PNGTest : public CxxTest::TestSuite {
    public:
        void testCompressDecompress() {
            unsigned width = 512, height = 512, bytesPerPixel = 3;
            unsigned char *image = new unsigned char[width * height * bytesPerPixel];
            for(unsigned y = 0; y < height; y++) {
                for(unsigned x = 0; x < width; x++) {
                    image[bytesPerPixel * width * y + bytesPerPixel * x + 0] = 255 * !(x & y);
                    image[bytesPerPixel * width * y + bytesPerPixel * x + 1] = x ^ y;
                    image[bytesPerPixel * width * y + bytesPerPixel * x + 2] = x | y;
                }
            }

            // Compress data as PNG image.
            uint32_t destSize = 0;
            uint8_t retCode = 0;
            unsigned char *compressedData = odcore::wrapper::png::PNG::compress(image, width, height, bytesPerPixel, destSize, retCode);

            TS_ASSERT(compressedData != NULL);
            TS_ASSERT(retCode == 0);

            // Decompress data from PNG image.
            retCode = 0;
            uint32_t decompressedWidth = 0;
            uint32_t decompressedHeight = 0;
            uint8_t actualBytesPerPixel = 0;
            unsigned char *decompressedData = odcore::wrapper::png::PNG::decompress(compressedData, destSize, &decompressedWidth, &decompressedHeight, &actualBytesPerPixel, bytesPerPixel, retCode);

            TS_ASSERT(retCode == 0);
            TS_ASSERT(decompressedData != NULL);

            // Check decompressed data.
            TS_ASSERT(decompressedWidth == width);
            TS_ASSERT(decompressedHeight == height);
            TS_ASSERT(actualBytesPerPixel == bytesPerPixel);

            for(unsigned y = 0; y < height; y++) {
                for(unsigned x = 0; x < width; x++) {
                    TS_ASSERT(image[bytesPerPixel * width * y + bytesPerPixel * x + 0] == decompressedData[bytesPerPixel * width * y + bytesPerPixel * x + 0]);
                    TS_ASSERT(image[bytesPerPixel * width * y + bytesPerPixel * x + 1] == decompressedData[bytesPerPixel * width * y + bytesPerPixel * x + 1]);
                    TS_ASSERT(image[bytesPerPixel * width * y + bytesPerPixel * x + 2] == decompressedData[bytesPerPixel * width * y + bytesPerPixel * x + 2]);
                }
            }

            free(compressedData);
            free(decompressedData);
            delete [] image;
        }
};

#endif /*ODCORE_PNGTESTSUITE_H_*/
