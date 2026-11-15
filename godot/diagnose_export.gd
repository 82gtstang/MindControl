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
	var exporter := EditorExport
	print("Export presets count: ", exporter.get_export_preset_count())
	for i in range(exporter.get_export_preset_count()):
		var preset := exporter.get_export_preset(i)
		print("Preset ", i, ": ", preset.get_name())
		var platform := preset.get_platform()
		print("Platform: ", platform.get_name())
		print("Platform methods: ", Array(platform.get_method_list()).filter(func(m): return "valid" in m.name or "warning" in m.name or "error" in m.name))
		print("Valid export configuration: ", platform.has_valid_export_configuration(preset, true))
		print("Valid project configuration: ", platform.has_valid_project_configuration(preset))
		for option in platform.get_export_options():
			var name: String = option.get("name", "")
			var warning: String = platform.get_export_option_warning(preset, name)
			if not warning.is_empty():
				print("OPTION WARNING ", name, ": ", warning)
	print("=== END DIAGNOSTICS ===")
	quit()
