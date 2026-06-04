#pragma once

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/video_stream_playback.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <avbridge.h>

namespace godot {

class VideoStreamPlaybackAVBridge : public VideoStreamPlayback {
    GDCLASS(VideoStreamPlaybackAVBridge, VideoStreamPlayback)

public:
    VideoStreamPlaybackAVBridge();
    ~VideoStreamPlaybackAVBridge() override;

    void set_file(const String &p_file);

    void   _play() override;
    void   _stop() override;
    bool   _is_playing() const override;
    void   _set_paused(bool p_paused) override;
    bool   _is_paused() const override;
    double _get_length() const override;
    double _get_playback_position() const override;
    void   _seek(double p_time) override;
    void   _set_audio_track(int p_idx) override;
    void   _update(double p_delta) override;
    int    _get_channels() const override;
    int    _get_mix_rate() const override;
    Ref<Texture2D> _get_texture() const override;

protected:
    static void _bind_methods();

private:
    String _file_path;

    avb_decoder   *_decoder = nullptr;
    avb_media_info _info{};
    bool           _decoder_open = false;

    // Kept alive for the decoder's lifetime when decoding through Godot's
    // virtual filesystem (res:// inside an exported .pck, where no real OS path
    // exists). The avbridge I/O callbacks read through this handle.
    Ref<FileAccess> _file_access;

    bool   _playing = false;
    bool   _paused  = false;
    double _time    = 0.0;
    int    _audio_track = 0; // logical audio track index (VideoStreamPlayer.audio_track)

    // One decoded video frame buffered ahead; shown once playback time
    // reaches its presentation timestamp.
    bool       _have_next = false;
    double     _next_pts  = 0.0;
    Ref<Image> _next_image;

    Ref<ImageTexture> _texture;
    bool              _texture_initialized = false;

    // Audio. Decoded interleaved float frames not yet accepted by the engine
    // mixer are held in _audio_pending (starting at frame _audio_pending_pos);
    // _audio_frames_read counts frames pulled from the decoder, used to gate
    // how far ahead of playback time we decode.
    int                  _channels = 0;
    int                  _mix_rate = 0;
    PackedFloat32Array   _audio_pending;
    int                  _audio_pending_pos = 0; // frames already mixed
    int64_t              _audio_frames_read = 0;

    bool       _open_decoder();
    void       _close_decoder();
    void       _decode_next_frame();
    void       _mix_audio(double p_time);
    void       _reset_audio();
    void       _present_frame(const Ref<Image> &p_image);
    Ref<Image> _frame_to_image(const avb_video_frame &p_frame) const;
};

} // namespace godot
