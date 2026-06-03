# godot-avbridge

Godot video/audio playback addon via GDExtension, powered by
[libavbridge](https://github.com/buresu/libavbridge).

libavbridge wraps the platform-native media stack (Media Foundation on Windows,
AVFoundation on macOS, GStreamer on Linux, optional FFmpeg everywhere), so this
addon plays common formats (H.264/HEVC/VP9 video, AAC/Opus audio in
mp4/mov/webm/mkv) without bundling codec libraries.

Unlike a raw-codec addon, libavbridge already performs demuxing, decoding,
pixel-format conversion and audio resampling — so this project needs **no**
extra `libyuv`/`minimp4`-style helpers.

## Usage

```gdscript
var stream := VideoStreamAVBridge.new()
stream.file = "res://video.mp4"   # or an absolute / user:// path
$VideoStreamPlayer.stream = stream
$VideoStreamPlayer.play()
```

`VideoStreamAVBridge` decodes video to `RGBA8` and feeds audio to the
`VideoStreamPlayer` automatically.

## Build

```
git clone --recursive https://github.com/buresu/godot-avbridge.git
cd godot-avbridge
mkdir build && cd build

# Linux / macOS
cmake -DCMAKE_BUILD_TYPE=[Debug|Release] ..
cmake --build . --target install

# Windows
cmake -G "Visual Studio 18 2026" ..
cmake --build . --config [Debug|Release] --target install
```

The build links libavbridge statically into a single shared object. The
GStreamer / FFmpeg runtime libraries are loaded dynamically at play time and
must be installed on the target system (see the libavbridge README).

## Runtime dependencies (Linux)

GStreamer 1.x with the `base` and `good` plugin sets (plus `bad`/`libav` for
extra codecs).

## License

MIT License
