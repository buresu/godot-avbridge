#include "video_stream_playback_avbridge.hpp"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

using namespace godot;

VideoStreamPlaybackAVBridge::VideoStreamPlaybackAVBridge() {
    _texture.instantiate();
}

VideoStreamPlaybackAVBridge::~VideoStreamPlaybackAVBridge() {
    _close_decoder();
}

void VideoStreamPlaybackAVBridge::_bind_methods() {}

void VideoStreamPlaybackAVBridge::set_file(const String &p_file) {
    _file_path = p_file;
    // Open eagerly so _get_channels()/_get_mix_rate() report real values
    // before the VideoStreamPlayer configures its audio bus on play().
    _open_decoder();
}

bool VideoStreamPlaybackAVBridge::_open_decoder() {
    if (_decoder_open) {
        return true;
    }
    if (_file_path.is_empty()) {
        return false;
    }

    // avbridge opens a real filesystem path; resolve res:// / user:// to one.
    String global = ProjectSettings::get_singleton()->globalize_path(_file_path);

    avb_decode_options opts = avb_decode_options_default();
    opts.enable_video = 1;
    opts.enable_audio = 1;
    opts.video_format = AVB_PIXEL_FORMAT_RGBA8;

    avb_result res = avb_decoder_open(&_decoder, global.utf8().get_data(), &opts);
    if (res != AVB_OK) {
        const char *err = _decoder ? avb_decoder_get_last_error(_decoder) : "unknown";
        UtilityFunctions::printerr("[avbridge] open failed (", avb_result_string(res),
                                   "): ", err ? err : "unknown");
        _close_decoder();
        return false;
    }

    avb_decoder_get_media_info(_decoder, &_info);
    if (!_info.video.available) {
        UtilityFunctions::printerr("[avbridge] no video stream in: ", _file_path);
        _close_decoder();
        return false;
    }

    if (_info.audio.available) {
        _channels = _info.audio.channels;
        _mix_rate = _info.audio.sample_rate;
    }

    _decoder_open = true;
    _decode_next_frame();
    return true;
}

void VideoStreamPlaybackAVBridge::_close_decoder() {
    if (_decoder) {
        avb_decoder_close(_decoder);
        _decoder = nullptr;
    }
    _decoder_open        = false;
    _info                = avb_media_info{};
    _have_next           = false;
    _next_image          = Ref<Image>();
    _texture_initialized = false;
    _channels            = 0;
    _mix_rate            = 0;
    _reset_audio();
}

void VideoStreamPlaybackAVBridge::_reset_audio() {
    _audio_pending.clear();
    _audio_pending_pos = 0;
    _audio_frames_read = 0;
}

// Decode interleaved float audio and feed it to the engine mixer, keeping a
// small lookahead beyond the current playback time. Frames the mixer can't yet
// accept are retained in _audio_pending and retried on the next call, so none
// are dropped.
void VideoStreamPlaybackAVBridge::_mix_audio(double p_time) {
    if (!_decoder_open || _channels <= 0 || _mix_rate <= 0) {
        return;
    }

    constexpr double AUDIO_LOOKAHEAD_SEC = 0.25;
    constexpr int    BLOCK_FRAMES        = 4096;

    while (true) {
        // 1. Flush whatever is already buffered into the mixer.
        int pending_frames = (int)(_audio_pending.size() / _channels) - _audio_pending_pos;
        if (pending_frames > 0) {
            int mixed = mix_audio(pending_frames, _audio_pending, _audio_pending_pos);
            if (mixed <= 0) {
                return; // Mixer full; keep the remainder for next time.
            }
            _audio_pending_pos += mixed;
            if (mixed < pending_frames) {
                return; // Partially accepted: mixer is full.
            }
        }

        // Fully drained: reset the buffer.
        _audio_pending.clear();
        _audio_pending_pos = 0;

        // 2. Stop once we've decoded enough to stay ahead of playback.
        double supplied = (double)_audio_frames_read / (double)_mix_rate;
        if (supplied >= p_time + AUDIO_LOOKAHEAD_SEC) {
            return;
        }

        // 3. Decode another block.
        _audio_pending.resize(BLOCK_FRAMES * _channels);
        int got = avb_decoder_read_audio_f32(_decoder, _audio_pending.ptrw(), BLOCK_FRAMES);
        if (got <= 0) {
            _audio_pending.clear();
            return; // End of audio stream.
        }
        _audio_pending.resize(got * _channels);
        _audio_frames_read += got;
    }
}

Ref<Image> VideoStreamPlaybackAVBridge::_frame_to_image(const avb_video_frame &p_frame) const {
    const int w = p_frame.width;
    const int h = p_frame.height;
    if (w <= 0 || h <= 0 || !p_frame.plane_data[0]) {
        return {};
    }

    PackedByteArray buf;
    buf.resize(w * h * 4);
    uint8_t       *dst    = buf.ptrw();
    const uint8_t *src    = p_frame.plane_data[0];
    const int      stride = p_frame.plane_stride[0];
    const int      row    = w * 4;

    if (stride == row) {
        memcpy(dst, src, (size_t)row * h);
    } else {
        for (int y = 0; y < h; ++y) {
            memcpy(dst + (size_t)y * row, src + (size_t)y * stride, row);
        }
    }

    return Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, buf);
}

void VideoStreamPlaybackAVBridge::_present_frame(const Ref<Image> &p_image) {
    if (p_image.is_null()) {
        return;
    }
    if (!_texture_initialized) {
        _texture->set_image(p_image);
        _texture_initialized = true;
    } else {
        _texture->update(p_image);
    }
}

void VideoStreamPlaybackAVBridge::_decode_next_frame() {
    _have_next = false;
    if (!_decoder_open) {
        return;
    }

    avb_video_frame frame{};
    avb_result      res = avb_decoder_read_video_frame(_decoder, &frame);
    if (res != AVB_OK) {
        return; // AVB_ERROR_EOF or a decode error: leave _have_next false.
    }

    _next_image = _frame_to_image(frame);
    _next_pts   = frame.pts_sec;
    _have_next  = _next_image.is_valid();

    avb_decoder_release_video_frame(_decoder, &frame);
}

void VideoStreamPlaybackAVBridge::_play() {
    _playing = true;
    _paused  = false;
    _time    = 0.0;
    if (_decoder_open) {
        avb_decoder_seek(_decoder, 0.0);
        _texture_initialized = false;
        _reset_audio();
        _decode_next_frame();
    }
}

void VideoStreamPlaybackAVBridge::_stop() {
    _playing = false;
    _paused  = false;
    _time    = 0.0;
    _close_decoder();
}

bool VideoStreamPlaybackAVBridge::_is_playing() const {
    return _playing;
}

void VideoStreamPlaybackAVBridge::_set_paused(bool p_paused) {
    _paused = p_paused;
}

bool VideoStreamPlaybackAVBridge::_is_paused() const {
    return _paused;
}

double VideoStreamPlaybackAVBridge::_get_length() const {
    if (!_decoder_open) {
        return 0.0;
    }
    if (_info.video.duration_sec > 0.0) {
        return _info.video.duration_sec;
    }
    return _info.duration_sec;
}

double VideoStreamPlaybackAVBridge::_get_playback_position() const {
    return _time;
}

void VideoStreamPlaybackAVBridge::_seek(double p_time) {
    if (!_decoder_open) {
        return;
    }
    if (avb_decoder_seek(_decoder, p_time) != AVB_OK) {
        return;
    }
    _time = p_time;
    _reset_audio();
    _audio_frames_read = (int64_t)(p_time * _mix_rate);
    _decode_next_frame();
    // Show the seeked frame right away so scrubbing updates the image even
    // while paused (when _update() does nothing).
    if (_have_next) {
        _present_frame(_next_image);
    }
}

void VideoStreamPlaybackAVBridge::_update(double p_delta) {
    if (!_playing || _paused) {
        return;
    }
    if (!_decoder_open) {
        if (!_open_decoder()) {
            _playing = false;
            return;
        }
    }

    _time += p_delta;

    _mix_audio(_time);

    while (_have_next && _next_pts <= _time) {
        _present_frame(_next_image);
        _decode_next_frame();
    }

    // End of stream: stop so VideoStreamPlayer emits "finished". (The clip does
    // not loop; replay by calling play() again.)
    if (!_have_next) {
        _playing = false;
        _time    = _get_length();
    }
}

int VideoStreamPlaybackAVBridge::_get_channels() const {
    return _channels;
}

int VideoStreamPlaybackAVBridge::_get_mix_rate() const {
    return _mix_rate;
}

Ref<Texture2D> VideoStreamPlaybackAVBridge::_get_texture() const {
    return _texture;
}
