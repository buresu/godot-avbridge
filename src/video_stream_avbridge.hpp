#pragma once

#include <godot_cpp/classes/video_stream.hpp>
#include <godot_cpp/classes/video_stream_playback.hpp>

namespace godot {

class VideoStreamAVBridge : public VideoStream {
    GDCLASS(VideoStreamAVBridge, VideoStream)

public:
    Ref<VideoStreamPlayback> _instantiate_playback() override;

protected:
    static void _bind_methods();
};

} // namespace godot
