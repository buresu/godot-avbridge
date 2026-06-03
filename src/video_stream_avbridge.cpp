#include "video_stream_avbridge.hpp"
#include "video_stream_playback_avbridge.hpp"

using namespace godot;

void VideoStreamAVBridge::_bind_methods() {}

Ref<VideoStreamPlayback> VideoStreamAVBridge::_instantiate_playback() {
    Ref<VideoStreamPlaybackAVBridge> pb;
    pb.instantiate();
    pb->set_file(get_file());
    return pb;
}
