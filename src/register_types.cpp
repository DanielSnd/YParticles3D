#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

#ifdef TOOLS_ENABLED
#include "editor/yparticles_gizmo.h"
#include "editor/yparticles_inspector_plugin.h"
#include "editor/yparticles_preview.h"
#endif
#include "yparticles3d.h"

using namespace godot;

static bool _yparticles3d_register_shader_setting(const String &p_name, const String &p_default_path) {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant current_value = project_settings->get_setting(p_name, Variant());
	if (current_value.get_type() == Variant::NIL || String(current_value).is_empty()) {
		Dictionary property_info;
		property_info["name"] = p_name;
		property_info["type"] = Variant::STRING;
		property_info["hint"] = PropertyHint::PROPERTY_HINT_FILE;
		property_info["hint_string"] = "*.gdshader";
		property_info["usage"] = PropertyUsageFlags::PROPERTY_USAGE_DEFAULT | PropertyUsageFlags::PROPERTY_USAGE_STORAGE;
		project_settings->add_property_info(property_info);
		project_settings->set_setting(p_name, p_default_path);
		project_settings->set_initial_value(p_name, p_default_path);
		return true;
	}
	return false;
}

static void _yparticles3d_register_shader_settings() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	bool changed = false;
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/mix", "res://addons/yparticles3d/shaders/YParticleGradientMapMix.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/mix_nearest", "res://addons/yparticles3d/shaders/YParticleGradientMapMixNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/add", "res://addons/yparticles3d/shaders/YParticleGradientMapAdd.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/add_nearest", "res://addons/yparticles3d/shaders/YParticleGradientMapAddNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/subtract", "res://addons/yparticles3d/shaders/YParticleGradientMapSub.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/subtract_nearest", "res://addons/yparticles3d/shaders/YParticleGradientMapSubNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/multiply", "res://addons/yparticles3d/shaders/YParticleGradientMapMult.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/multiply_nearest", "res://addons/yparticles3d/shaders/YParticleGradientMapMultNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/premultiplied_alpha", "res://addons/yparticles3d/shaders/YParticleGradientMapMultAlpha.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/shaders/premultiplied_alpha_nearest", "res://addons/yparticles3d/shaders/YParticleGradientMapMultAlphaNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/mix", "res://addons/yparticles3d/shaders/YParticleTrailMix.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/mix_nearest", "res://addons/yparticles3d/shaders/YParticleTrailMixNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/add", "res://addons/yparticles3d/shaders/YParticleTrailAdd.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/add_nearest", "res://addons/yparticles3d/shaders/YParticleTrailAddNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/subtract", "res://addons/yparticles3d/shaders/YParticleTrailSub.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/subtract_nearest", "res://addons/yparticles3d/shaders/YParticleTrailSubNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/multiply", "res://addons/yparticles3d/shaders/YParticleTrailMult.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/multiply_nearest", "res://addons/yparticles3d/shaders/YParticleTrailMultNearest.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/premultiplied_alpha", "res://addons/yparticles3d/shaders/YParticleTrailPremul.gdshader");
	changed |= _yparticles3d_register_shader_setting("yengine/yparticles3d/trail_shaders/premultiplied_alpha_nearest", "res://addons/yparticles3d/shaders/YParticleTrailPremulNearest.gdshader");
	if (changed) {
		project_settings->save();
	}
}

void gdextension_initialize(ModuleInitializationLevel p_level)
{
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(YParticles3D);
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		_yparticles3d_register_shader_settings();
		GDREGISTER_CLASS(YParticlesPreview);
		GDREGISTER_CLASS(YParticlesGizmoPlugin);
		GDREGISTER_CLASS(YParticlesModuleHeader);
		GDREGISTER_CLASS(YParticlesModeSelector);
		GDREGISTER_CLASS(YParticlesModuleToggleProperty);
		GDREGISTER_CLASS(YParticlesRangeProperty);
		GDREGISTER_CLASS(YParticlesSizeRangeProperty);
		GDREGISTER_CLASS(YParticlesBurstEditor);
		GDREGISTER_CLASS(YParticlesSubEmitterEditor);
		GDREGISTER_CLASS(YParticlesInspectorPlugin);
		GDREGISTER_CLASS(YParticlesEditorPlugin);
		EditorPlugins::add_by_type<YParticlesEditorPlugin>();
	}
#endif
}

void gdextension_terminate(ModuleInitializationLevel p_level)
{
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<YParticlesEditorPlugin>();
	}
#else
	(void)p_level;
#endif
}

extern "C"
{
	GDExtensionBool GDE_EXPORT yparticles3d_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

		init_obj.register_initializer(gdextension_initialize);
		init_obj.register_terminator(gdextension_terminate);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
