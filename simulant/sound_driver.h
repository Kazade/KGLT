/* *   Copyright (c) 2011-2017 Luke Benstead https://simulant-engine.appspot.com
 *
 *     This file is part of Simulant.
 *
 *     Simulant is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Simulant is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU Lesser General Public License for more details.
 *
 *     You should have received a copy of the GNU Lesser General Public License
 *     along with Simulant.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "generic/property.h"

namespace smlt {

typedef uint32_t AudioSourceID;
typedef uint32_t AudioBufferID;


enum AudioSourceState {
    AUDIO_SOURCE_STATE_PLAYING,
    AUDIO_SOURCE_STATE_PAUSED,
    AUDIO_SOURCE_STATE_STOPPED
};

enum AudioDataFormat {
    AUDIO_DATA_FORMAT_MONO8,
    AUDIO_DATA_FORMAT_MONO16,
    AUDIO_DATA_FORMAT_STEREO8,
    AUDIO_DATA_FORMAT_STEREO16
};


class Window;

/* Basically a hacky abstraction over OpenAL with the thinking that the only other drivers will be:
 *
 * - Dreamcast
 * - Dummy
 *
 * If that ceases to be the case for whatever reason, then we should probably design a nicer API for this. Perhaps.
 */
class SoundDriver {
public:
    SoundDriver(Window* window):
        window_(window) {}

    virtual ~SoundDriver() {}

    virtual bool startup() = 0;
    virtual void shutdown() = 0;

    virtual std::vector<AudioSourceID> generate_sources(uint32_t count) = 0;
    virtual std::vector<AudioBufferID> generate_buffers(uint32_t count) = 0;

    virtual void delete_buffers(const std::vector<AudioBufferID>& buffers) = 0;
    virtual void delete_sources(const std::vector<AudioSourceID>& sources) = 0;

    virtual void play_source(AudioSourceID source_id) = 0;
    virtual void stop_source(AudioSourceID source_id) = 0;

    virtual void queue_buffers_to_source(AudioSourceID source, uint32_t count, const std::vector<AudioBufferID>& buffers) = 0;
    virtual std::vector<AudioBufferID> unqueue_buffers_from_source(AudioSourceID source, uint32_t count) = 0;
    virtual void upload_buffer_data(AudioBufferID buffer, AudioDataFormat format, int16_t* data, uint32_t size, uint32_t frequency) = 0;

    virtual AudioSourceState source_state(AudioSourceID source) = 0;
    virtual int32_t source_buffers_processed_count(AudioSourceID source) const = 0;

    Property<SoundDriver, Window> window = {this, &SoundDriver::window_};

private:
    Window* window_ = nullptr;
};


}
