extends Node

enum State { IDLE, PLAYING, PAUSED }

@onready var video_player: VideoStreamPlayer = $VideoStreamPlayer
@onready var status_label: Label = $UI/StatusLabel
@onready var open_button: Button = $UI/ButtonRow/OpenButton
@onready var play_pause_button: Button = $UI/ButtonRow/PlayPauseButton
@onready var stop_button: Button = $UI/ButtonRow/StopButton
@onready var seek_bar: HSlider = $UI/SeekBar
@onready var file_dialog: FileDialog = $UI/FileDialog

var _state := State.IDLE
var _seeking := false

func _ready() -> void:
	status_label.text = "Open a video file (mp4/mov/...) to play."

	# Start the (native) file dialog at the user's home directory.
	var home := OS.get_environment("HOME")
	if home.is_empty():
		home = OS.get_environment("USERPROFILE") # Windows fallback
	if not home.is_empty():
		file_dialog.current_dir = home

	video_player.finished.connect(_on_finished)
	seek_bar.drag_started.connect(_on_seek_drag_started)
	seek_bar.drag_ended.connect(_on_seek_drag_ended)
	seek_bar.value_changed.connect(_on_seek_value_changed)
	_set_state(State.IDLE)

func _process(_delta: float) -> void:
	if video_player.stream == null:
		return
	var pos := video_player.stream_position
	var len := video_player.get_stream_length()
	status_label.text = "%.2f / %.2f sec" % [pos, len]
	if not _seeking:
		seek_bar.max_value = len
		seek_bar.set_value_no_signal(pos)

func _on_open_button_pressed() -> void:
	file_dialog.popup_centered_ratio(0.7)

func _on_file_dialog_file_selected(path: String) -> void:
	var stream := VideoStreamAVBridge.new()
	stream.file = path
	video_player.stream = stream
	video_player.play()
	_set_state(State.PLAYING)
	status_label.text = "Playing: %s" % path.get_file()

func _on_seek_drag_started() -> void:
	_seeking = true

func _on_seek_value_changed(value: float) -> void:
	if _seeking and video_player.stream != null:
		video_player.stream_position = value

func _on_seek_drag_ended(_value_changed: bool) -> void:
	_seeking = false

func _on_play_pause_button_pressed() -> void:
	match _state:
		State.IDLE:
			if video_player.stream == null:
				return
			var pos := video_player.stream_position
			video_player.play()
			if pos > 0.0:
				video_player.stream_position = pos
			_set_state(State.PLAYING)
		State.PLAYING:
			video_player.paused = true
			_set_state(State.PAUSED)
		State.PAUSED:
			video_player.paused = false
			_set_state(State.PLAYING)

func _on_stop_button_pressed() -> void:
	video_player.stop()
	_set_state(State.IDLE)
	status_label.text = "Stopped"

func _on_finished() -> void:
	_set_state(State.IDLE)
	status_label.text = "Finished"

func _set_state(state: State) -> void:
	_state = state
	var has_stream := video_player.stream != null
	match state:
		State.IDLE:
			play_pause_button.text = "Play"
			play_pause_button.disabled = not has_stream
			stop_button.disabled = true
			seek_bar.editable = has_stream
		State.PLAYING:
			play_pause_button.text = "Pause"
			play_pause_button.disabled = false
			stop_button.disabled = false
			seek_bar.editable = true
		State.PAUSED:
			play_pause_button.text = "Resume"
			play_pause_button.disabled = false
			stop_button.disabled = false
			seek_bar.editable = true
