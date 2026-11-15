extends SceneTree

func _initialize() -> void:
	print("=== ANDROID EXPORT DIAGNOSTICS ===")
	print("ANDROID_HOME: ", OS.get_environment("ANDROID_HOME"))
	print("JAVA_HOME: ", OS.get_environment("JAVA_HOME"))
	print("Godot version: ", Engine.get_version_info())
	var sdk := OS.get_environment("ANDROID_HOME")
	for path in [
		sdk + "/platform-tools/adb",
		sdk + "/build-tools/35.0.0/apksigner",
		sdk + "/build-tools/35.0.0/aapt2",
		sdk + "/platforms/android-35/android.jar",
		"/tmp/mindcontrol-debug.keystore",
		OS.get_environment("HOME") + "/.local/share/godot/export_templates/4.4.1.stable/android_debug.apk",
		OS.get_environment("HOME") + "/.local/share/godot/export_templates/4.4.1.stable/android_release.apk"
	]:
		print("CHECK ", path, ": ", FileAccess.file_exists(path))
	print("Editor settings SDK: ", EditorInterface.get_editor_settings().get_setting("export/android/android_sdk_path"))
	print("Editor settings Java: ", EditorInterface.get_editor_settings().get_setting("export/android/java_sdk_path"))
	print("=== GODOT EXPORT VALIDATION ===")
	print("EditorExport is not exposed to GDScript; checking preset file instead.")
	var cfg := ConfigFile.new()
	var err := cfg.load("res://export_presets.cfg")
	print("Preset parse status: ", error_string(err))
	if err == OK:
		for section in cfg.get_sections():
			print("SECTION ", section)
			for key in cfg.get_section_keys(section):
				print("  ", key, " = ", cfg.get_value(section, key))
	print("=== END DIAGNOSTICS ===")
	quit()
