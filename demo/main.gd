extends Node

@onready var video_player: VideoStreamPlayer = $VideoStreamPlayer
@onready var status_label: Label = $UI/StatusLabel
@onready var open_button: Button = $UI/OpenButton
@onready var file_dialog: FileDialog = $UI/FileDialog

func _ready() -> void:
	status_label.text = "Open a video file (mp4/mov/...) to play."

	# Start the (native) file dialog at the user's home directory.
	var home := OS.get_environment("HOME")
	if home.is_empty():
		home = OS.get_environment("USERPROFILE") # Windows fallback
	if not home.is_empty():
		file_dialog.current_dir = home

func _on_open_button_pressed() -> void:
	file_dialog.popup_centered_ratio(0.7)

func _on_file_dialog_file_selected(path: String) -> void:
	video_player.stop()

	var stream := VideoStreamAVBridge.new()
	stream.file = path
	video_player.stream = stream
	video_player.play()

	status_label.text = "Playing: %s" % path.get_file()
