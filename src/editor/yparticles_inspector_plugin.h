#ifndef YPARTICLES_INSPECTOR_PLUGIN_H
#define YPARTICLES_INSPECTOR_PLUGIN_H

#ifdef TOOLS_ENABLED

#include "../yparticles3d.h"
#include "yparticles_gizmo.h"
#include "yparticles_preview.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/editor_inspector_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_property.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>

using namespace godot;

class YParticlesModuleHeader : public HBoxContainer {
	GDCLASS(YParticlesModuleHeader, HBoxContainer);

	String section_key;
	String property_name;
	YParticles3D *particles = nullptr;
	Button *fold_button = nullptr;
	CheckBox *toggle = nullptr;
	Label *title = nullptr;
	bool has_toggle = false;

	void _toggle_fold();
	void _toggle_enabled(bool p_pressed);
	void _refresh();

protected:
	static void _bind_methods();

public:
	void setup(YParticles3D *p_particles, const String &p_section_key, const String &p_title, const String &p_property_name = String());
	static bool is_section_expanded(const String &p_section_key);

	YParticlesModuleHeader();
};

class YParticlesModeSelector : public EditorProperty {
	GDCLASS(YParticlesModeSelector, EditorProperty);

	YParticles3D *particles = nullptr;
	String mode_property;
	String secondary_mode_property;
	MenuButton *button = nullptr;
	Vector<String> compact_labels;
	Vector<int> primary_values;
	Vector<int> secondary_values;

	void _mode_selected(int p_index);
	void _refresh();

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	void setup(YParticles3D *p_particles, const String &p_label, const String &p_mode_property, const Vector<String> &p_labels);
	void setup_multi(YParticles3D *p_particles, const String &p_label, const String &p_mode_property, const String &p_secondary_mode_property, const Vector<String> &p_labels, const Vector<int> &p_primary_values, const Vector<int> &p_secondary_values);

	YParticlesModeSelector();
};

class YParticlesModuleToggleProperty : public EditorProperty {
	GDCLASS(YParticlesModuleToggleProperty, EditorProperty);

	YParticles3D *particles = nullptr;
	CheckBox *checkbox = nullptr;
	Label *state_label = nullptr;

	void _toggled(bool p_pressed);
	void _refresh();

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	void setup(YParticles3D *p_particles, const String &p_label);

	YParticlesModuleToggleProperty();
};

class YParticlesRangeProperty : public EditorProperty {
	GDCLASS(YParticlesRangeProperty, EditorProperty);

	SpinBox *min_spin = nullptr;
	SpinBox *max_spin = nullptr;
	bool updating = false;

	void _min_changed(double p_value);
	void _max_changed(double p_value);

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	void setup(const String &p_label, double p_step = 0.01, double p_min = -1000000.0, double p_max = 1000000.0);

	YParticlesRangeProperty();
};

class YParticlesSizeRangeProperty : public EditorProperty {
	GDCLASS(YParticlesSizeRangeProperty, EditorProperty);

	SpinBox *min_x_spin = nullptr;
	SpinBox *min_y_spin = nullptr;
	SpinBox *max_x_spin = nullptr;
	SpinBox *max_y_spin = nullptr;
	bool updating = false;

	void _value_changed(double p_value);
	void _emit_current();

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	void setup(const String &p_label, double p_step = 0.01);

	YParticlesSizeRangeProperty();
};

class YParticlesBurstEditor : public EditorProperty {
	GDCLASS(YParticlesBurstEditor, EditorProperty);

	VBoxContainer *root = nullptr;
	HBoxContainer *header_row = nullptr;
	VBoxContainer *rows = nullptr;
	Button *fold_button = nullptr;
	Button *add_button = nullptr;
	bool expanded = false;

	Array _get_bursts();
	void _set_bursts(const Array &p_bursts);
	void _refresh();
	void _toggle_fold();
	void _add_pressed();
	void _remove_pressed(int p_index);
	void _set_burst_value(const Variant &p_value, const String &p_key, int p_index);
	void _set_count_mode(int p_selected, int p_index);
	void _set_cycle_mode(int p_selected, int p_index);

	PanelContainer *_make_cell(bool p_last = false);
	HBoxContainer *_make_row(int p_index, const Array &p_bursts);

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	YParticlesBurstEditor();
};

class YParticlesSubEmitterEditor : public EditorProperty {
	GDCLASS(YParticlesSubEmitterEditor, EditorProperty);

	VBoxContainer *root = nullptr;
	HBoxContainer *header_row = nullptr;
	VBoxContainer *rows = nullptr;
	Button *fold_button = nullptr;
	Button *add_button = nullptr;
	bool expanded = false;

	Array _get_entries();
	void _set_entries(const Array &p_entries);
	void _refresh();
	void _toggle_fold();
	void _add_pressed();
	void _remove_pressed(int p_index);
	void _set_entry_path(const String &p_value, int p_index);
	void _pick_entry_path(int p_index);
	void _node_path_selected(const NodePath &p_path, int p_index);
	void _set_entry_event(int p_selected, int p_index);
	void _set_entry_probability(double p_value, int p_index);
	void _toggle_inherit_flag(int p_id, int p_index);

	PanelContainer *_make_cell(bool p_last = false);
	HBoxContainer *_make_row(int p_index, const Array &p_entries);

protected:
	static void _bind_methods();

public:
	virtual void _update_property() override;
	YParticlesSubEmitterEditor();
};

class YParticlesInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(YParticlesInspectorPlugin, EditorInspectorPlugin);

	struct SectionInfo {
		String key;
		String title;
		String first_property;
		String toggle_property;
		HashSet<String> properties;
	};

	struct ModeGroup {
		String key;
		String label;
		String mode_property;
		String secondary_mode_property;
		String first_property;
		Vector<String> labels;
		Vector<HashSet<String>> modes;
		Vector<int> primary_values;
		Vector<int> secondary_values;
	};

	YParticles3D *current_particles = nullptr;
	HashMap<String, SectionInfo> sections;
	HashMap<String, String> property_to_section;
	HashMap<String, ModeGroup> mode_groups;
	HashMap<String, String> property_to_mode_group;

	void _reset_state();
	void _build_sections();
	void _build_mode_groups();
	bool _is_section_visible(const String &p_section_key) const;
	bool _passes_property_conditions(const String &p_property) const;

protected:
	static void _bind_methods();

public:
	virtual bool _can_handle(Object *p_object) const override;
	virtual void _parse_begin(Object *p_object) override;
	virtual bool _parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void _parse_end(Object *p_object) override;
};

class YParticlesEditorPlugin : public EditorPlugin {
	GDCLASS(YParticlesEditorPlugin, EditorPlugin);

	Ref<YParticlesInspectorPlugin> inspector_plugin;
	Ref<YParticlesGizmoPlugin> gizmo_plugin;
	YParticlesPreview *preview = nullptr;
	String current_main_screen_name;
	bool has_instantiated_viewport_preview = false;

	void _on_main_screen_changed(const String &p_screen_name);
	void _on_editor_selection_changed();
	YParticles3D *_find_particles_or_null(const TypedArray<Node> &p_nodes) const;
	Control *_find_3d_viewport_control(Node *p_node) const;
	Node *_find_node_by_class(Node *p_node, const String &p_class_name) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	YParticlesEditorPlugin();
	~YParticlesEditorPlugin();
};

#endif // TOOLS_ENABLED

#endif // YPARTICLES_INSPECTOR_PLUGIN_H
