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

#ifndef SOUND_H
#define SOUND_H

#include <vector>
#include <list>

#include "sound_driver.h"

#include "generic/managed.h"
#include "generic/identifiable.h"

#include "signals/signal.h"

#include "asset.h"
#include "loadable.h"

#include "types.h"

namespace smlt {

class StageNode;
class Source;
class SourceInstance;

class Sound :
    public RefCounted<Sound>,
    public generic::Identifiable<SoundID>,
    public Asset,
    public Loadable,
    public ChainNameable<Sound> {

public:
    Sound(SoundID id, AssetManager* asset_manager, SoundDriver* sound_driver);

    uint32_t sample_rate() const { return sample_rate_; }
    void set_sample_rate(uint32_t rate) { sample_rate_ = rate; }

    AudioDataFormat format() { return format_; }
    void set_format(AudioDataFormat format) { format_ = format; }

    std::size_t buffer_size() const { return buffer_size_; }
    void set_buffer_size(std::size_t size) { buffer_size_ = size; }

    uint8_t channels() const { return channels_; }
    void set_channels(uint8_t ch) { channels_ = ch; }

    std::shared_ptr<std::istream>& input_stream() { return sound_data_; }
    void set_input_stream(std::shared_ptr<std::istream> stream) {
        sound_data_ = stream;

        /* Calculate the length (remaining from wherever the stream is) */
        int pos = stream->tellg();
        stream->seekg(0, std::ios_base::end);
        int end = stream->tellg();
        stream->seekg(pos, std::ios_base::beg);
        stream_length_ = end - pos;
    }

    std::size_t stream_length() const {
        return stream_length_;
    }

    void set_source_init_function(std::function<void (SourceInstance&)> func) { init_source_ = func; }

    SoundDriver* _driver() const { return driver_; }
private:
    void init_source(SourceInstance& source);

    std::function<void (SourceInstance&)> init_source_;

    SoundDriver* driver_ = nullptr;
    std::shared_ptr<std::istream> sound_data_;

    uint32_t sample_rate_ = 0;
    AudioDataFormat format_;
    uint8_t channels_ = 0;
    std::size_t buffer_size_ = 0;
    std::size_t stream_length_ = 0;

    friend class Source;
    friend class SourceInstance;
};

typedef std::function<int32_t (AudioBufferID)> StreamFunc;

class Source;

enum AudioRepeat {
    AUDIO_REPEAT_NONE,
    AUDIO_REPEAT_FOREVER
};

enum DistanceModel {
    DISTANCE_MODEL_POSITIONAL,
    DISTANCE_MODEL_AMBIENT
};

typedef std::size_t SourceInstanceID;

class SourceInstance:
    public RefCounted<SourceInstance> {

    friend class Source;

private:
    static SourceInstanceID counter_;

    SourceInstanceID id_;

    Source& parent_;

    AudioSourceID source_;
    std::vector<AudioBufferID> buffers_;
    std::weak_ptr<Sound> sound_;
    StreamFunc stream_func_;

    AudioRepeat loop_stream_;
    bool is_dead_;

    /* This is used to calculate the velocity */
    smlt::Vec3 previous_position_;
    bool first_update_ = true;
public:
    SourceInstance(Source& parent, std::weak_ptr<Sound> sound, AudioRepeat loop_stream, DistanceModel model=DISTANCE_MODEL_POSITIONAL);
    virtual ~SourceInstance();

    SourceInstanceID id() const {
        return id_;
    }

    void start();
    void update(float dt);
    void stop();

    bool is_playing() const;

    /* Set the stream function for filling buffers. A -1 return
     * means the sound has been destroyed */
    void set_stream_func(StreamFunc func) { stream_func_ = func; }

    bool is_dead() const { return is_dead_; }
};

class Source {
public:
    Source(Window* window);
    Source(Stage* stage, StageNode* this_as_node, SoundDriver *driver);
    virtual ~Source();

    SourceInstanceID play_sound(SoundID sound_id, AudioRepeat repeat=AUDIO_REPEAT_NONE);
    bool stop_sound(SourceInstanceID sound_id);

    /* The number of sounds this source is currently playing */
    uint8_t playing_sound_count() const;

    /* The number of sounds that have finished, but aren't yet
     * destroyed */
    uint8_t played_sound_count() const;

    bool is_sound_playing() const;

    void update_source(float dt);

    sig::signal<void ()>& signal_stream_finished() { return signal_stream_finished_; }

    void set_gain(RangeValue<0, 1> gain);
    void set_pitch(RangeValue<0, 1> pitch);
    void set_reference_distance(float dist);

private:
    SoundDriver* _sound_driver() const;

    Stage* stage_ = nullptr;
    Window* window_ = nullptr;
    SoundDriver* driver_ = nullptr;
    StageNode* node_ = nullptr;

    std::list<SourceInstance::ptr> instances_;
    sig::signal<void ()> signal_stream_finished_;

    friend class Sound;
    friend class SourceInstance;
};

}
#endif // SOUND_H
