/**
 * OpenDaVINCI - Portable middleware for distributed components.
 * Copyright (C) 2008 - 2015 Christian Berger
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

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "opendavinci/odcore/base/module/AbstractCIDModule.h"
#include "opendavinci/odcore/base/Lock.h"
#include "opendavinci/odcore/base/Serializable.h"
#include "opendavinci/odcore/base/Thread.h"
#include "opendavinci/odcore/data/TimeStamp.h"
#include "opendavinci/odcore/opendavinci.h"
#include "opendavinci/odcore/wrapper/png/PNG.h"
#include "opendavinci/odcore/wrapper/SharedMemory.h"
#include "opendavinci/odcore/wrapper/SharedMemoryFactory.h"
#include "opendavinci/odtools/player/PlayerCache.h"
#include "opendavinci/generated/odcore/data/SharedData.h"
#include "opendavinci/generated/odcore/data/buffer/MemorySegment.h"
#include "opendavinci/generated/odcore/data/image/SharedImage.h"
#include "opendavinci/generated/odcore/data/image/SharedImagePNG.h"
#include "opendavinci/generated/odcore/data/SharedPointCloud.h"

namespace odtools {
    namespace player {

        using namespace std;
        using namespace odcore;
        using namespace odcore::base;
        using namespace odcore::data;
        using namespace odtools;

        PlayerCache::PlayerCache(const uint32_t size, const uint32_t sizeMemorySegments, const bool &autoRewind, std::shared_ptr<istream> in, std::shared_ptr<istream> inSharedMemoryFile) :
            m_cacheSize(size),
            m_autoRewind(autoRewind),
            m_in(in),
            m_inSharedMemoryFile(inSharedMemoryFile),
            m_queue(),
            m_recBuffer(),
            m_memBuffer(),
            m_mapOfMemories(),
            m_bufferIn(),
            m_bufferOut(),
            m_sharedPointers(),
            m_modifyCacheMutex() {
            m_cacheSize = (m_cacheSize < 3) ? 3 : m_cacheSize;
            m_queue.clear();

            m_recBuffer.clear();
            m_memBuffer.clear();

            CLOG1 << "PlayerCache: preparing buffer...";
            for(uint16_t id = 0; id < m_cacheSize; id++) {
                odcore::data::buffer::MemorySegment ms;
                ms.setSize(sizeMemorySegments);
                ms.setIdentifier(id);
                void *ptr = ::malloc(ms.getSize());
                m_mapOfMemories[ms.getIdentifier()] = static_cast<char*>(ptr);

                Container c(ms);
                m_bufferIn.enter(c);
            }
            CLOG1 << "done." << endl;

            // Start over.
            clearQueueRewindInputStreams();
        }

        PlayerCache::~PlayerCache() {
            m_queue.clear();

            CLOG1 << "PlayerCache: cleaning up buffers..." << endl;

            for(map<uint32_t, char*>::iterator it = m_mapOfMemories.begin();
                it != m_mapOfMemories.end(); ++it) {
                CLOG1 << "Cleaning memory segment " << it->first << endl;
                ::free(it->second);
            }

            m_bufferIn.clear();
            m_bufferOut.clear();

            CLOG1 << "done." << endl;
        }

        Container PlayerCache::getNextContainer() {
            Container c;

            if (getNumberOfEntries() > 0) {
                c = m_queue.leave();
            }

            return c;
        }

        uint32_t PlayerCache::getNumberOfEntries() const {
            return m_queue.getSize();
        }

        void PlayerCache::rewindInputStreams() {
            // Start from beginning.
            m_in->clear();

            // Seek to the beginning of the input stream.
            m_in->seekg(ios::beg);

            // If a memory dump file was found, reset it as well.
            if (m_inSharedMemoryFile.get()) {
                m_inSharedMemoryFile->clear();
                m_inSharedMemoryFile->seekg(ios::beg);
            }

            // After rewinding, fill the cache again using the internal method.
            updateCacheInternal();
        }

        void PlayerCache::clearQueueRewindInputStreams() {
            Lock l(m_modifyCacheMutex);

            m_queue.clear();

            // Put all memory segments from m_bufferOut back to m_bufferIn for re-use.
            while (!m_bufferOut.isEmpty()) {
                Container c = m_bufferOut.leave();
                m_bufferIn.enter(c);
            }

            rewindInputStreams();
        }

        void PlayerCache::updateCache() {
            // Do only fill cache if not in currently rewinding.
            Lock l(m_modifyCacheMutex);

            updateCacheInternal();
        }

        void PlayerCache::updateCacheInternal() {
            // Read further data ONLY IFF the cache has free slots AND the buffer for shared memory segments is not completely filled.
            const uint32_t numberOfEntries = m_queue.getSize();
            while ( (numberOfEntries < m_cacheSize) && (!m_bufferIn.isEmpty()) ) {
                bool bufferFilled = fillCache();
                if (!bufferFilled) {
                    if (m_autoRewind) {
                        // Rewind streams without clearing the current cache.
                        rewindInputStreams();
                    }
                    break;
                }
            }
        }

        void PlayerCache::beforeStop() {}

        void PlayerCache::run() {
            serviceReady();

            while (isRunning()) {
                updateCache();

                Thread::usleepFor(1000);
            }
            CLOG1 << "PlayerCache: No more data to cache." << endl;
        }

        bool PlayerCache::fillCache() {
            Container fromRecFile;
            Container fromMemFile;

            bool readFromRecFile = false;
            bool readFromMemFile = false;

            // Get next datum from m_rec:
            if (!m_recBuffer.isEmpty()) {
                // Check if a previously delayed entry (due to younger time stamps) need to be processed.
                fromRecFile = m_recBuffer.pop();
                readFromRecFile = true;
            }
            else {
                // Try to read directly from file.
                if (m_in->good()) {
                    (*m_in) >> fromRecFile;
                    if (m_in->gcount() > 0) {
                        readFromRecFile = true;
                    }
                }
            }

            // Get next datum from m_mem:
            if (!m_memBuffer.isEmpty()) {
                // Check if a previously delayed entry (due to younger time stamps) need to be processed.
                fromMemFile = m_memBuffer.pop();
                readFromMemFile = true;
            }
            else {
                if ( (m_inSharedMemoryFile.get()) && (m_inSharedMemoryFile->good()) ) {
                    *m_inSharedMemoryFile >> fromMemFile;

                    if (m_inSharedMemoryFile->gcount() > 0) {
                        putRawMemoryDataIntoBuffer(fromMemFile);
                        readFromMemFile = true;
                    }
                }
            }

            // Multiplexing if both, a .rec and a .mem file were found.
            if (readFromRecFile && readFromMemFile) {
                // Compare timestamps from both Containers, put the older one into the queue and the younger one into the mem/recBuffer.
                if (fromRecFile.getReceivedTimeStamp() < fromMemFile.getReceivedTimeStamp()) {
                    m_queue.enter(fromRecFile);
                    m_memBuffer.push(fromMemFile);
                }
                else {
                    m_recBuffer.push(fromRecFile);
                    m_queue.enter(fromMemFile);
                }
            }

            if (readFromRecFile && !readFromMemFile) {
                m_queue.enter(fromRecFile);
            }

            if (!readFromRecFile && readFromMemFile) {
                m_queue.enter(fromMemFile);
            }

            if (!readFromRecFile && !readFromMemFile) {
                // Data could not be read - neither from the rec file nor from the mem file. 
            }

            return (readFromRecFile || readFromMemFile);
        }

        void PlayerCache::putRawMemoryDataIntoBuffer(Container &header) {
            if (!m_bufferIn.isEmpty()) {
                string nameOfSharedMemory = "";
                uint32_t size = 0;

                // This instance is only used if we encounter a PNG compressed image.
                odcore::data::image::SharedImagePNG siPNG;

                // Do we need to handle uncompressed shared images?
                if (header.getDataType() == odcore::data::image::SharedImage::ID()) {
                    odcore::data::image::SharedImage si = header.getData<odcore::data::image::SharedImage>();

                    nameOfSharedMemory = si.getName();
                    size = si.getSize();

                    // For old recordings containing SharedImage, the attribute size is calculated
                    // "on-the-fly". The following two lines set the size attribute in the generated
                    // data structure here.
                    size = (size > 0) ? size : (si.getWidth() * si.getHeight() * si.getBytesPerPixel());
                    si.setSize(size);
                }
                else if (header.getDataType() == odcore::data::SharedData::ID()) {
                    odcore::data::SharedData sd = header.getData<odcore::data::SharedData>();

                    nameOfSharedMemory = sd.getName();
                    size = sd.getSize();
                }
                else if (header.getDataType() == odcore::data::SharedPointCloud::ID()) {
                    odcore::data::SharedPointCloud spc = header.getData<odcore::data::SharedPointCloud>();

                    nameOfSharedMemory = spc.getName();
                    size = spc.getSize();
                }
                else if (header.getDataType() == odcore::data::image::SharedImagePNG::ID()) {
                    siPNG = header.getData<odcore::data::image::SharedImagePNG>();

                    nameOfSharedMemory = siPNG.getName();
                    size = siPNG.getSize();
                }

                // Check, whether a shared memory was already created for this SharedX; otherwise, create it and save it for later.
                map<string, std::shared_ptr<odcore::wrapper::SharedMemory> >::iterator it = m_sharedPointers.find(nameOfSharedMemory);
                if (it == m_sharedPointers.end()) {
                    std::shared_ptr<odcore::wrapper::SharedMemory> sp = odcore::wrapper::SharedMemoryFactory::createSharedMemory(nameOfSharedMemory, size);
                    m_sharedPointers[nameOfSharedMemory] = sp;
                }

                // Get current position in stream.
                int32_t curr = m_inSharedMemoryFile->tellg();

                // Get pointer to next available memory segment from the buffer.
                Container c = m_bufferIn.leave();
                odcore::data::buffer::MemorySegment ms = c.getData<odcore::data::buffer::MemorySegment>();

                // Get pointer to memory where to store the data.
                char *ptrToMemory = m_mapOfMemories[ms.getIdentifier()];

                // Flag to indicate whether copying of the data succeeded.
                bool copied = false;

                // Check if we need to handle a PNG compressed image that needs to be replaced by a regular SharedImage.
                if (header.getDataType() == odcore::data::image::SharedImagePNG::ID()) {
                    const uint32_t SIZE_OF_COMPRESSED_DATA = siPNG.getSizeInBytesForPNG();

                    // Check if we have enough space to hold the data before continuing.
                    if (SIZE_OF_COMPRESSED_DATA <= ms.getSize()) {
                        // Seek to the current position in the stream.
                        m_inSharedMemoryFile->seekg(curr);

                        // Read the data into the buffer (due to performance, we simple use the pre-allocated memory twice).
                        m_inSharedMemoryFile->read(ptrToMemory, SIZE_OF_COMPRESSED_DATA);

                        // Decompress PNG data (will create a fresh buffer).
                        uint8_t returnCode = 0;
                        uint32_t decompressedWidth = 0;
                        uint32_t decompressedHeight = 0;
                        uint8_t actualBytesPerPixel = 0;
                        unsigned char *decompressedData = odcore::wrapper::png::PNG::decompress(reinterpret_cast<unsigned char*>(ptrToMemory), SIZE_OF_COMPRESSED_DATA, &decompressedWidth, &decompressedHeight, &actualBytesPerPixel, siPNG.getBytesPerPixel(), returnCode);

                        // Check if decompression succeeded.
                        if ( (0 == returnCode) &&
                             (siPNG.getWidth() == decompressedWidth) &&
                             (siPNG.getHeight() == decompressedHeight) &&
                             (siPNG.getBytesPerPixel() == actualBytesPerPixel) &&
                             (NULL != decompressedData) ) {
                            // Create replacement data structure for SharedImagePNG.
                            odcore::data::image::SharedImage si;
                                si.setName(siPNG.getName());
                                si.setSize(siPNG.getSize());
                                si.setWidth(siPNG.getWidth());
                                si.setHeight(siPNG.getHeight());
                                si.setBytesPerPixel(siPNG.getBytesPerPixel());

                            if (si.getSize() <= ms.getSize()) {
                                // Copy data from decompressed image into MemorySegment data structure.
                                ::memcpy(ptrToMemory, decompressedData, si.getSize());

                                // Store meta data.
                                ms.setHeader(si);

                                // Store the consumed size of the MemorySegment.
                                ms.setConsumedSize(si.getSize());

                                // Save meta information.
                                c = Container(ms);

                                copied = true;
                            }
                        }

                        // Release memory acquired during the decompressing phase.
                        if (NULL != decompressedData) {
                            ::free(decompressedData);
                        }
                    }
                }
                else {
                    // Seek to the current position in the stream.
                    m_inSharedMemoryFile->seekg(curr);

                    // Read the data into the buffer.
                    m_inSharedMemoryFile->read(ptrToMemory, size);

                    // Store meta data.
                    ms.setHeader(header);

                    // Store the consumed size of the MemorySegment.
                    ms.setConsumedSize(size);

                    // Save meta information.
                    c = Container(ms);

                    copied = true;
                }

                if (copied) {
                    // Put the processed element into the output buffer.
                    m_bufferOut.enter(c);
                }
                else {
cerr << "Failed to decode" << endl;
                    // Something failed, recycle memory.
                    m_bufferIn.enter(c);
                }

            }
        }

        void PlayerCache::copyMemoryToSharedMemory(odcore::data::Container &container) {
            // The m_bufferOut should never run empty.
            if (!m_bufferOut.isEmpty()) {
                string nameOfSharedMemory = "";

                if (container.getDataType() == odcore::data::image::SharedImage::ID()) {
                    odcore::data::image::SharedImage si = container.getData<odcore::data::image::SharedImage>();
                    nameOfSharedMemory = si.getName();
                }
                else if (container.getDataType() == odcore::data::SharedData::ID()) {
                    odcore::data::SharedData sd = container.getData<odcore::data::SharedData>();
                    nameOfSharedMemory = sd.getName();
                }
                else if (container.getDataType() == odcore::data::SharedPointCloud::ID()) {
                    odcore::data::SharedPointCloud spc = container.getData<odcore::data::SharedPointCloud>();
                    nameOfSharedMemory = spc.getName();
                }

                // Check, if a shared memory exists for this container.
                map<string, std::shared_ptr<odcore::wrapper::SharedMemory> >::iterator it = m_sharedPointers.find(nameOfSharedMemory);
                if (it != m_sharedPointers.end()) {
                    // Get next entry to process from output queue.
                    Container c = m_bufferOut.leave();
                    odcore::data::buffer::MemorySegment ms = c.getData<odcore::data::buffer::MemorySegment>();

                    // Get pointer to memory with the data.
                    char *src = m_mapOfMemories[ms.getIdentifier()];

                    // Get shared memory based on "header" informaton.
                    std::shared_ptr<odcore::wrapper::SharedMemory> sp = it->second;

                    // memcpy src to shared memory.
                    ::memcpy(sp->getSharedMemory(), src, ms.getConsumedSize());

                    // Reset meta information.
                    ms.setConsumedSize(0);

                    // Save meta information.
                    c = Container(ms);

                    // After processing, return the processed memory segment to input queue.
                    m_bufferIn.enter(c);
                }
            }
        }

    } // player
} // tools
