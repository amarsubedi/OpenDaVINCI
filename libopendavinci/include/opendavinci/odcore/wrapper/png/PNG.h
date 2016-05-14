/**
 * OpenDaVINCI - Portable middleware for distributed components.
 * Copyright (C) 2016 Christian Berger
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

#ifndef OPENDAVINCI_CORE_WRAPPER_PNG_PNG_H_
#define OPENDAVINCI_CORE_WRAPPER_PNG_PNG_H_

#include "opendavinci/odcore/opendavinci.h"

namespace odcore {
    namespace wrapper {
        namespace png {

            /**
             * This class provides static methods to compress and decompress
             * data in PNG format.
             */
            class OPENDAVINCI_API PNG {
                private:
                    /**
                     * "Forbidden" copy constructor. Goal: The compiler should warn
                     * already at compile time for unwanted bugs caused by any misuse
                     * of the copy constructor.
                     */
                    PNG(const PNG &);

                    /**
                     * "Forbidden" assignment operator. Goal: The compiler should warn
                     * already at compile time for unwanted bugs caused by any misuse
                     * of the assignment operator.
                     */
                    PNG& operator=(const PNG &);

                    PNG();
                    
                public:
                    virtual ~PNG();

                    /**
                     * This method compresses raw image data using a PNG compressor.
                     *
                     * @param destSize Size of destination buffer that will be set to the actual amount of bytes used thereof.
                     * @param width Raw image's width.
                     * @param height Raw image's height.
                     * @param bytesPerPixel Raw image's bytes per pixel (channels).
                     * @param rawImageDate Raw image data.
                     * @param returnCode Return code from PNG encoder.
                     * @return Pointer to the compressed image that MUST be released by the callee or NULL in the case of an error.
                     */
                    static unsigned char* compress(const uint8_t *rawImageData, const uint32_t &width, const uint32_t &height, const uint32_t &bytesPerPixel, uint32_t &destSize, uint8_t &returnCode);

                    /**
                     * This method decompresses a PNG compressed image.
                     *
                     * @param src Pointer to a buffer containing the compressed image data.
                     * @param srcSize Size of buffer.
                     * @param width Decompressed image's width.
                     * @param height Decompressed image's height.
                     * @param actualBytesPerPixel Decompressed image's bytes per pixel (channels).
                     * @param requestedBytesPerPixel Expected decompressed image's bytes per pixel (channels).
                     * @param returnCode Return code from PNG decoder.
                     * @return Pointer to the decompressed image that MUST be released by the callee or NULL in the case of an error.
                     */
                    static unsigned char* decompress(const unsigned char *src, const uint32_t &srcSize, uint32_t *width, uint32_t *height, uint8_t *actualBytesPerPixel, const uint8_t &requestedBytesPerPixel, uint8_t &returnCode);
            };

        }
    }
} // odcore::wrapper::png

#endif /*OPENDAVINCI_CORE_WRAPPER_PNG_PNG_H_*/
