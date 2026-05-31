#ifdef TOOLS_ENABLED

#include "yparticles_inspector_plugin.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/popup_menu.hpp>

using namespace godot;

static float _yparticles_editor_scale() {
	EditorInterface *editor_interface = EditorInterface::get_singleton();
	return editor_interface != nullptr ? editor_interface->get_editor_scale() : 1.0f;
}

static Variant _yparticles_get_edited_property_value(EditorProperty *p_property) {
	Object *edited_object = p_property->get_edited_object();
	if (edited_object == nullptr) {
		return Variant();
	}
	return edited_object->get(p_property->get_edited_property());
}

#define EDSCALE _yparticles_editor_scale()

static Dictionary _yparticles_make_default_burst_entry() {
	Dictionary burst;
	burst["time"] = 0.0;
	burst["count_mode"] = 0;
	burst["min_count"] = 10;
	burst["max_count"] = 10;
	burst["cycle_mode"] = 0;
	burst["min_cycles"] = 1;
	burst["max_cycles"] = 1;
	burst["interval"] = 0.0;
	burst["probability"] = 1.0;
	return burst;
}

static Array _yparticles_normalize_bursts_for_editor(const Array &p_bursts) {
	Array normalized;
	if (p_bursts.is_empty()) {
		return normalized;
	}
	if (p_bursts[0].get_type() == Variant::DICTIONARY) {
		for (int i = 0; i < p_bursts.size(); i++) {
			if (p_bursts[i].get_type() == Variant::DICTIONARY) {
				normalized.append(((Dictionary)p_bursts[i]).duplicate(true));
			}
		}
		return normalized;
	}
	for (int i = 0; i + 8 < p_bursts.size(); i += 9) {
		Dictionary burst = _yparticles_make_default_burst_entry();
		burst["time"] = p_bursts[i];
		burst["count_mode"] = p_bursts[i + 1];
		burst["min_count"] = p_bursts[i + 2];
		burst["max_count"] = p_bursts[i + 3];
		burst["cycle_mode"] = p_bursts[i + 4];
		burst["min_cycles"] = p_bursts[i + 5];
		burst["max_cycles"] = p_bursts[i + 6];
		burst["interval"] = p_bursts[i + 7];
		burst["probability"] = p_bursts[i + 8];
		normalized.append(burst);
	}
	return normalized;
}

static HashMap<String, bool> &_yparticles_collapse_states() {
	static HashMap<String, bool> collapse_states;
	return collapse_states;
}

void YParticlesModuleHeader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_toggle_fold"), &YParticlesModuleHeader::_toggle_fold);
	ClassDB::bind_method(D_METHOD("_toggle_enabled", "pressed"), &YParticlesModuleHeader::_toggle_enabled);
}

YParticlesModuleHeader::YParticlesModuleHeader() {
	set_h_size_flags(SIZE_EXPAND_FILL);

	fold_button = memnew(Button);
	fold_button->set_flat(true);
	fold_button->set_focus_mode(FOCUS_NONE);
	fold_button->connect("pressed", callable_mp(this, &YParticlesModuleHeader::_toggle_fold));
	add_child(fold_button);

	toggle = memnew(CheckBox);
	toggle->set_focus_mode(FOCUS_NONE);
	toggle->connect("toggled", callable_mp(this, &YParticlesModuleHeader::_toggle_enabled));
	add_child(toggle);

	title = memnew(Label);
	title->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(title);
}

void YParticlesModuleHeader::setup(YParticles3D *p_particles, const String &p_section_key, const String &p_title, const String &p_property_name) {
	particles = p_particles;
	section_key = p_section_key;
	property_name = p_property_name;
	title->set_text(p_title);
	has_toggle = !property_name.is_empty();
	toggle->set_visible(has_toggle);

	HashMap<String, bool> &collapse_states = _yparticles_collapse_states();
	if (!collapse_states.has(section_key)) {
		collapse_states.insert(section_key, true);
	}

	_refresh();
}

bool YParticlesModuleHeader::is_section_expanded(const String &p_section_key) {
	HashMap<String, bool> &collapse_states = _yparticles_collapse_states();
	if (!collapse_states.has(p_section_key)) {
		return true;
	}
	return collapse_states[p_section_key];
}

void YParticlesModuleHeader::_toggle_fold() {
	HashMap<String, bool> &collapse_states = _yparticles_collapse_states();
	collapse_states[section_key] = !is_section_expanded(section_key);
	if (particles != nullptr) {
		particles->notify_property_list_changed();
	}
	_refresh();
}

void YParticlesModuleHeader::_toggle_enabled(bool p_pressed) {
	if (particles == nullptr || !has_toggle) {
		return;
	}

	particles->set(property_name, p_pressed);
	if (p_pressed) {
		HashMap<String, bool> &collapse_states = _yparticles_collapse_states();
		collapse_states[section_key] = true;
	}
	particles->notify_property_list_changed();
	_refresh();
}

void YParticlesModuleHeader::_refresh() {
	fold_button->set_text(is_section_expanded(section_key) ? "v" : ">");
	if (has_toggle && particles != nullptr) {
		toggle->set_pressed_no_signal((bool)particles->get(property_name));
		title->set_modulate(toggle->is_pressed() ? Color(1, 1, 1) : Color(1, 1, 1, 0.45));
	} else {
		title->set_modulate(Color(1, 1, 1));
	}
}

void YParticlesModeSelector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_mode_selected", "index"), &YParticlesModeSelector::_mode_selected);
}

YParticlesModeSelector::YParticlesModeSelector() {
	button = memnew(MenuButton);
	button->set_h_size_flags(SIZE_SHRINK_END);
	button->set_flat(true);
	button->set_custom_minimum_size(Size2(110 * EDSCALE, 0));
	button->get_popup()->connect("id_pressed", callable_mp(this, &YParticlesModeSelector::_mode_selected));
	add_child(button);
}

void YParticlesModeSelector::setup(YParticles3D *p_particles, const String &p_label, const String &p_mode_property, const Vector<String> &p_labels) {
	Vector<int> default_primary;
	Vector<int> default_secondary;
	default_primary.resize(p_labels.size());
	default_secondary.resize(p_labels.size());
	for (int i = 0; i < p_labels.size(); i++) {
		default_primary.write[i] = i;
		default_secondary.write[i] = 0;
	}
	setup_multi(p_particles, p_label, p_mode_property, String(), p_labels, default_primary, default_secondary);
}

void YParticlesModeSelector::setup_multi(YParticles3D *p_particles, const String &p_label, const String &p_mode_property, const String &p_secondary_mode_property, const Vector<String> &p_labels, const Vector<int> &p_primary_values, const Vector<int> &p_secondary_values) {
	particles = p_particles;
	mode_property = p_mode_property;
	secondary_mode_property = p_secondary_mode_property;
	set_label(p_label);
	button->get_popup()->clear();
	compact_labels.clear();
	primary_values = p_primary_values;
	secondary_values = p_secondary_values;
	for (int i = 0; i < p_labels.size(); i++) {
		button->get_popup()->add_item(p_labels[i], i);
		String compact = p_labels[i];
		if (compact == "Constant") {
			compact = "Const";
		} else if (compact == "Random") {
			compact = "Rand";
		} else if (compact == "Curve") {
			compact = "Curve";
		} else if (compact == "Square Random") {
			compact = "Square";
		}
		compact_labels.push_back(compact);
	}
	_refresh();
}

void YParticlesModeSelector::_mode_selected(int p_index) {
	if (particles == nullptr) {
		return;
	}
	if (p_index < 0 || p_index >= primary_values.size()) {
		return;
	}
	particles->set(mode_property, primary_values[p_index]);
	if (!secondary_mode_property.is_empty() && p_index < secondary_values.size()) {
		particles->set(secondary_mode_property, (bool)secondary_values[p_index]);
	}
	particles->notify_property_list_changed();
	_refresh();
}

void YParticlesModeSelector::_refresh() {
	if (particles == nullptr || button == nullptr) {
		return;
	}
	const int primary = (int)particles->get(mode_property);
	const int secondary = secondary_mode_property.is_empty() ? 0 : ((bool)particles->get(secondary_mode_property) ? 1 : 0);
	int selected = -1;
	for (int i = 0; i < primary_values.size(); i++) {
		const int expected_secondary = i < secondary_values.size() ? secondary_values[i] : 0;
		if (primary_values[i] == primary && expected_secondary == secondary) {
			selected = i;
			break;
		}
	}
	if (selected < 0 && primary >= 0 && primary < compact_labels.size() && secondary_mode_property.is_empty()) {
		selected = primary;
	}
	if (selected >= 0 && selected < compact_labels.size()) {
		button->set_text(compact_labels[selected] + String(" v"));
	}
	PopupMenu *popup = button->get_popup();
	for (int i = 0; i < popup->get_item_count(); i++) {
		popup->set_item_checked(i, i == selected);
	}
}

void YParticlesModeSelector::_update_property() {
	_refresh();
}

void YParticlesModuleToggleProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_toggled", "pressed"), &YParticlesModuleToggleProperty::_toggled);
}

YParticlesModuleToggleProperty::YParticlesModuleToggleProperty() {
	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(row);

	checkbox = memnew(CheckBox);
	checkbox->set_text("");
	checkbox->connect("toggled", callable_mp(this, &YParticlesModuleToggleProperty::_toggled));
	row->add_child(checkbox);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(SIZE_EXPAND_FILL);
	row->add_child(spacer);

	state_label = memnew(Label);
	row->add_child(state_label);
}

void YParticlesModuleToggleProperty::setup(YParticles3D *p_particles, const String &p_label) {
	particles = p_particles;
	set_label(p_label);
	_refresh();
}

void YParticlesModuleToggleProperty::_toggled(bool p_pressed) {
	if (particles == nullptr) {
		return;
	}
	emit_changed(get_edited_property(), p_pressed);
	_refresh();
}

void YParticlesModuleToggleProperty::_refresh() {
	if (particles == nullptr || checkbox == nullptr || state_label == nullptr) {
		return;
	}
	const bool enabled = (bool)particles->get(get_edited_property());
	checkbox->set_pressed_no_signal(enabled);
	state_label->set_text(enabled ? "On" : "Off");
	state_label->set_modulate(enabled ? Color(1, 1, 1, 0.9f) : Color(1, 1, 1, 0.5f));
}

void YParticlesModuleToggleProperty::_update_property() {
	_refresh();
}

void YParticlesRangeProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_min_changed", "value"), &YParticlesRangeProperty::_min_changed);
	ClassDB::bind_method(D_METHOD("_max_changed", "value"), &YParticlesRangeProperty::_max_changed);
}

YParticlesRangeProperty::YParticlesRangeProperty() {
	HBoxContainer *root = memnew(HBoxContainer);
	root->add_theme_constant_override("separation", 6 * EDSCALE);
	add_child(root);

	Label *min_label = memnew(Label);
	min_label->set_text("Min");
	min_label->set_custom_minimum_size(Size2(26 * EDSCALE, 0));
	root->add_child(min_label);
	min_spin = memnew(SpinBox);
	min_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	min_spin->connect("value_changed", callable_mp(this, &YParticlesRangeProperty::_min_changed));
	root->add_child(min_spin);
	Label *max_label = memnew(Label);
	max_label->set_text("Max");
	max_label->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	root->add_child(max_label);
	max_spin = memnew(SpinBox);
	max_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	max_spin->connect("value_changed", callable_mp(this, &YParticlesRangeProperty::_max_changed));
	root->add_child(max_spin);
}

void YParticlesRangeProperty::setup(const String &p_label, double p_step, double p_min, double p_max) {
	set_label(p_label);
	min_spin->set_step(p_step);
	max_spin->set_step(p_step);
	min_spin->set_min(p_min);
	min_spin->set_max(p_max);
	max_spin->set_min(p_min);
	max_spin->set_max(p_max);
}

void YParticlesRangeProperty::_min_changed(double p_value) {
	if (updating) {
		return;
	}
	Vector2 value = _yparticles_get_edited_property_value(this);
	value.x = p_value;
	emit_changed(get_edited_property(), value);
}

void YParticlesRangeProperty::_max_changed(double p_value) {
	if (updating) {
		return;
	}
	Vector2 value = _yparticles_get_edited_property_value(this);
	value.y = p_value;
	emit_changed(get_edited_property(), value);
}

void YParticlesRangeProperty::_update_property() {
	if (get_edited_object() == nullptr) {
		return;
	}
	const Variant edited = _yparticles_get_edited_property_value(this);
	if (edited.get_type() != Variant::VECTOR2) {
		return;
	}
	const Vector2 value = edited;
	updating = true;
	min_spin->set_value(value.x);
	max_spin->set_value(value.y);
	updating = false;
}

void YParticlesSizeRangeProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_value_changed", "value"), &YParticlesSizeRangeProperty::_value_changed);
}

YParticlesSizeRangeProperty::YParticlesSizeRangeProperty() {
	VBoxContainer *root = memnew(VBoxContainer);
	root->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(root);

	HBoxContainer *min_row = memnew(HBoxContainer);
	min_row->add_theme_constant_override("separation", 4 * EDSCALE);
	root->add_child(min_row);
	Label *min_label = memnew(Label);
	min_label->set_text("Min");
	min_label->set_custom_minimum_size(Size2(26 * EDSCALE, 0));
	min_row->add_child(min_label);
	Label *min_x_label = memnew(Label);
	min_x_label->set_text("X");
	min_x_label->set_modulate(Color(0.85f, 0.45f, 0.55f));
	min_row->add_child(min_x_label);
	min_x_spin = memnew(SpinBox);
	min_x_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	min_x_spin->connect("value_changed", callable_mp(this, &YParticlesSizeRangeProperty::_value_changed));
	min_row->add_child(min_x_spin);
	Label *min_y_label = memnew(Label);
	min_y_label->set_text("Y");
	min_y_label->set_modulate(Color(0.65f, 0.8f, 0.4f));
	min_row->add_child(min_y_label);
	min_y_spin = memnew(SpinBox);
	min_y_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	min_y_spin->connect("value_changed", callable_mp(this, &YParticlesSizeRangeProperty::_value_changed));
	min_row->add_child(min_y_spin);

	HBoxContainer *max_row = memnew(HBoxContainer);
	max_row->add_theme_constant_override("separation", 4 * EDSCALE);
	root->add_child(max_row);
	Label *max_label = memnew(Label);
	max_label->set_text("Max");
	max_label->set_custom_minimum_size(Size2(26 * EDSCALE, 0));
	max_row->add_child(max_label);
	Label *max_x_label = memnew(Label);
	max_x_label->set_text("X");
	max_x_label->set_modulate(Color(0.85f, 0.45f, 0.55f));
	max_row->add_child(max_x_label);
	max_x_spin = memnew(SpinBox);
	max_x_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	max_x_spin->connect("value_changed", callable_mp(this, &YParticlesSizeRangeProperty::_value_changed));
	max_row->add_child(max_x_spin);
	Label *max_y_label = memnew(Label);
	max_y_label->set_text("Y");
	max_y_label->set_modulate(Color(0.65f, 0.8f, 0.4f));
	max_row->add_child(max_y_label);
	max_y_spin = memnew(SpinBox);
	max_y_spin->set_h_size_flags(SIZE_EXPAND_FILL);
	max_y_spin->connect("value_changed", callable_mp(this, &YParticlesSizeRangeProperty::_value_changed));
	max_row->add_child(max_y_spin);
}

void YParticlesSizeRangeProperty::setup(const String &p_label, double p_step) {
	set_label(p_label);
	min_x_spin->set_step(p_step);
	min_y_spin->set_step(p_step);
	max_x_spin->set_step(p_step);
	max_y_spin->set_step(p_step);
}

void YParticlesSizeRangeProperty::_emit_current() {
	Vector4 value(min_x_spin->get_value(), min_y_spin->get_value(), max_x_spin->get_value(), max_y_spin->get_value());
	emit_changed(get_edited_property(), value);
}

void YParticlesSizeRangeProperty::_value_changed(double p_value) {
	(void)p_value;
	if (updating) {
		return;
	}
	_emit_current();
}

void YParticlesSizeRangeProperty::_update_property() {
	if (get_edited_object() == nullptr) {
		return;
	}
	const Variant edited = _yparticles_get_edited_property_value(this);
	if (edited.get_type() != Variant::VECTOR4) {
		return;
	}
	const Vector4 value = edited;
	updating = true;
	min_x_spin->set_value(value.x);
	min_y_spin->set_value(value.y);
	max_x_spin->set_value(value.z);
	max_y_spin->set_value(value.w);
	updating = false;
}

void YParticlesBurstEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_toggle_fold"), &YParticlesBurstEditor::_toggle_fold);
	ClassDB::bind_method(D_METHOD("_add_pressed"), &YParticlesBurstEditor::_add_pressed);
	ClassDB::bind_method(D_METHOD("_remove_pressed", "index"), &YParticlesBurstEditor::_remove_pressed);
	ClassDB::bind_method(D_METHOD("_set_burst_value", "value", "key", "index"), &YParticlesBurstEditor::_set_burst_value);
	ClassDB::bind_method(D_METHOD("_set_count_mode", "selected", "index"), &YParticlesBurstEditor::_set_count_mode);
	ClassDB::bind_method(D_METHOD("_set_cycle_mode", "selected", "index"), &YParticlesBurstEditor::_set_cycle_mode);
}

YParticlesBurstEditor::YParticlesBurstEditor() {
	root = memnew(VBoxContainer);
	add_child(root);

	fold_button = memnew(Button);
	fold_button->set_flat(true);
	fold_button->set_text(">");
	fold_button->connect("pressed", callable_mp(this, &YParticlesBurstEditor::_toggle_fold));
	root->add_child(fold_button);

	header_row = memnew(HBoxContainer);
	header_row->add_theme_constant_override("separation", 0);
	root->add_child(header_row);

	const char *headers[] = { "Time", "Count", "Cycles", "Interval", "Probability", "" };
	for (int i = 0; i < 6; i++) {
		PanelContainer *cell = memnew(PanelContainer);
		if (i == 5) {
			cell->set_custom_minimum_size(Size2(34 * EDSCALE, 0));
		} else {
			cell->set_h_size_flags(SIZE_EXPAND_FILL);
		}
		MarginContainer *margin = memnew(MarginContainer);
		margin->add_theme_constant_override("margin_left", 4);
		margin->add_theme_constant_override("margin_right", 4);
		margin->add_theme_constant_override("margin_top", 2);
		margin->add_theme_constant_override("margin_bottom", 2);
		cell->add_child(margin);
		Label *label = memnew(Label);
		label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		label->set_text(headers[i]);
		label->set_h_size_flags(SIZE_EXPAND_FILL);
		margin->add_child(label);
		header_row->add_child(cell);
	}

	rows = memnew(VBoxContainer);
	rows->add_theme_constant_override("separation", 0);
	root->add_child(rows);

	add_button = memnew(Button);
	add_button->set_text("Add Burst");
	add_button->connect("pressed", callable_mp(this, &YParticlesBurstEditor::_add_pressed));
	root->add_child(add_button);

	_refresh();
}

Array YParticlesBurstEditor::_get_bursts() {
	Object *edited = get_edited_object();
	if (edited == nullptr) {
		return Array();
	}
	return _yparticles_normalize_bursts_for_editor(edited->get(get_edited_property()));
}

void YParticlesBurstEditor::_set_bursts(const Array &p_bursts) {
	emit_changed(get_edited_property(), p_bursts);
}

PanelContainer *YParticlesBurstEditor::_make_cell(bool p_last) {
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_h_size_flags(SIZE_EXPAND_FILL);
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 4);
	margin->add_theme_constant_override("margin_right", 4);
	margin->add_theme_constant_override("margin_top", 2);
	margin->add_theme_constant_override("margin_bottom", 2);
	panel->add_child(margin);
	if (p_last) {
		panel->set_custom_minimum_size(Size2(34 * EDSCALE, 0));
	}
	return panel;
}

HBoxContainer *YParticlesBurstEditor::_make_row(int p_index, const Array &p_bursts) {
	if (p_index < 0 || p_index >= p_bursts.size() || p_bursts[p_index].get_type() != Variant::DICTIONARY) {
		return memnew(HBoxContainer);
	}
	const Dictionary burst = p_bursts[p_index];
	HBoxContainer *row = memnew(HBoxContainer);
	row->add_theme_constant_override("separation", 0);

	{
		PanelContainer *cell = _make_cell();
		SpinBox *box = memnew(SpinBox);
		box->set_step(0.001);
		box->set_value((double)burst.get("time", 0.0));
		box->set_h_size_flags(SIZE_EXPAND_FILL);
		box->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String("time"), p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(box);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell();
		HBoxContainer *box = memnew(HBoxContainer);
		SpinBox *min_spin = memnew(SpinBox);
		min_spin->set_min(1);
		min_spin->set_value((int)burst.get("min_count", 10));
		min_spin->set_h_size_flags(SIZE_EXPAND_FILL);
		min_spin->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String("min_count"), p_index));
		box->add_child(min_spin);
		if ((int)burst.get("count_mode", 0) == 1) {
			SpinBox *max_spin = memnew(SpinBox);
			max_spin->set_min(1);
			max_spin->set_value((int)burst.get("max_count", 10));
			max_spin->set_h_size_flags(SIZE_EXPAND_FILL);
			max_spin->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String("max_count"), p_index));
			box->add_child(max_spin);
		}
		OptionButton *mode = memnew(OptionButton);
		mode->set_flat(true);
		mode->add_item("Const", 0);
		mode->add_item("Random", 1);
		mode->select((int)burst.get("count_mode", 0));
		mode->set_fit_to_longest_item(false);
		mode->set_tooltip_text("Constant or Random");
		mode->connect("item_selected", callable_mp(this, &YParticlesBurstEditor::_set_count_mode).bind(p_index));
		box->add_child(mode);
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(box);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell();
		HBoxContainer *box = memnew(HBoxContainer);
		SpinBox *min_spin = memnew(SpinBox);
		min_spin->set_min(1);
		min_spin->set_value((int)burst.get("min_cycles", 1));
		min_spin->set_h_size_flags(SIZE_EXPAND_FILL);
		min_spin->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String("min_cycles"), p_index));
		box->add_child(min_spin);
		if ((int)burst.get("cycle_mode", 0) == 1) {
			SpinBox *max_spin = memnew(SpinBox);
			max_spin->set_min(1);
			max_spin->set_value((int)burst.get("max_cycles", 1));
			max_spin->set_h_size_flags(SIZE_EXPAND_FILL);
			max_spin->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String("max_cycles"), p_index));
			box->add_child(max_spin);
		}
		OptionButton *mode = memnew(OptionButton);
		mode->set_flat(true);
		mode->add_item("Const", 0);
		mode->add_item("Random", 1);
		mode->select((int)burst.get("cycle_mode", 0));
		mode->set_fit_to_longest_item(false);
		mode->set_tooltip_text("Constant or Random");
		mode->connect("item_selected", callable_mp(this, &YParticlesBurstEditor::_set_cycle_mode).bind(p_index));
		box->add_child(mode);
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(box);
		row->add_child(cell);
	}

	const char *scalar_keys[] = { "interval", "probability" };
	for (int offset = 0; offset < 2; offset++) {
		PanelContainer *cell = _make_cell();
		SpinBox *box = memnew(SpinBox);
		if (offset == 1) {
			box->set_min(0);
			box->set_max(1);
			box->set_step(0.01);
		} else {
			box->set_min(0);
			box->set_step(0.001);
		}
		box->set_value((double)burst.get(scalar_keys[offset], offset == 0 ? 0.0 : 1.0));
		box->set_h_size_flags(SIZE_EXPAND_FILL);
		box->connect("value_changed", callable_mp(this, &YParticlesBurstEditor::_set_burst_value).bind(String(scalar_keys[offset]), p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(box);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell(true);
		Button *remove = memnew(Button);
		remove->set_text("X");
		remove->set_modulate(Color(0.8, 0.3, 0.3));
		remove->connect("pressed", callable_mp(this, &YParticlesBurstEditor::_remove_pressed).bind(p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(remove);
		row->add_child(cell);
	}

	return row;
}

void YParticlesBurstEditor::_refresh() {
	if (fold_button != nullptr) {
		fold_button->set_text(expanded ? "v" : ">");
	}
	if (header_row != nullptr) {
		header_row->set_visible(expanded);
	}
	if (rows != nullptr) {
		rows->set_visible(expanded);
	}
	if (add_button != nullptr) {
		add_button->set_visible(expanded);
	}

	if (!expanded || rows == nullptr) {
		return;
	}

	for (int i = 0; i < rows->get_child_count(); i++) {
		rows->get_child(i)->queue_free();
	}

	Array bursts = _get_bursts();
	for (int i = 0; i < bursts.size(); i++) {
		rows->add_child(_make_row(i, bursts));
	}
}

void YParticlesBurstEditor::_toggle_fold() {
	expanded = !expanded;
	_refresh();
}

void YParticlesBurstEditor::_add_pressed() {
	Array bursts = _get_bursts();
	bursts.append(_yparticles_make_default_burst_entry());
	_set_bursts(bursts);
	expanded = true;
	_refresh();
}

void YParticlesBurstEditor::_remove_pressed(int p_index) {
	Array bursts = _get_bursts();
	bursts.remove_at(p_index);
	_set_bursts(bursts);
	_refresh();
}

void YParticlesBurstEditor::_set_burst_value(const Variant &p_value, const String &p_key, int p_index) {
	Array bursts = _get_bursts();
	if (p_index < 0 || p_index >= bursts.size()) {
		return;
	}
	Dictionary burst = bursts[p_index];
	burst[p_key] = p_value;
	bursts[p_index] = burst;
	_set_bursts(bursts);
}

void YParticlesBurstEditor::_set_count_mode(int p_selected, int p_index) {
	Array bursts = _get_bursts();
	Dictionary burst = bursts[p_index];
	burst["count_mode"] = p_selected;
	if (p_selected == 0) {
		burst["max_count"] = burst.get("min_count", 10);
	}
	bursts[p_index] = burst;
	_set_bursts(bursts);
	_refresh();
}

void YParticlesBurstEditor::_set_cycle_mode(int p_selected, int p_index) {
	Array bursts = _get_bursts();
	Dictionary burst = bursts[p_index];
	burst["cycle_mode"] = p_selected;
	if (p_selected == 0) {
		burst["max_cycles"] = burst.get("min_cycles", 1);
	}
	bursts[p_index] = burst;
	_set_bursts(bursts);
	_refresh();
}

void YParticlesBurstEditor::_update_property() {
	_refresh();
}

void YParticlesSubEmitterEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_toggle_fold"), &YParticlesSubEmitterEditor::_toggle_fold);
	ClassDB::bind_method(D_METHOD("_add_pressed"), &YParticlesSubEmitterEditor::_add_pressed);
	ClassDB::bind_method(D_METHOD("_remove_pressed", "index"), &YParticlesSubEmitterEditor::_remove_pressed);
	ClassDB::bind_method(D_METHOD("_set_entry_path", "value", "index"), &YParticlesSubEmitterEditor::_set_entry_path);
	ClassDB::bind_method(D_METHOD("_pick_entry_path", "index"), &YParticlesSubEmitterEditor::_pick_entry_path);
	ClassDB::bind_method(D_METHOD("_node_path_selected", "path", "index"), &YParticlesSubEmitterEditor::_node_path_selected);
	ClassDB::bind_method(D_METHOD("_set_entry_event", "selected", "index"), &YParticlesSubEmitterEditor::_set_entry_event);
	ClassDB::bind_method(D_METHOD("_set_entry_probability", "value", "index"), &YParticlesSubEmitterEditor::_set_entry_probability);
	ClassDB::bind_method(D_METHOD("_toggle_inherit_flag", "id", "index"), &YParticlesSubEmitterEditor::_toggle_inherit_flag);
}

YParticlesSubEmitterEditor::YParticlesSubEmitterEditor() {
	root = memnew(VBoxContainer);
	add_child(root);

	fold_button = memnew(Button);
	fold_button->set_flat(true);
	fold_button->set_text(">");
	fold_button->connect("pressed", callable_mp(this, &YParticlesSubEmitterEditor::_toggle_fold));
	root->add_child(fold_button);

	header_row = memnew(HBoxContainer);
	header_row->add_theme_constant_override("separation", 0);
	root->add_child(header_row);

	const char *headers[] = { "Path", "Event", "Probability", "Inherit", "" };
	for (int i = 0; i < 5; i++) {
		PanelContainer *cell = memnew(PanelContainer);
		if (i == 4) {
			cell->set_custom_minimum_size(Size2(34 * EDSCALE, 0));
		} else {
			cell->set_h_size_flags(i == 0 ? SIZE_EXPAND_FILL : SIZE_SHRINK_CENTER);
			if (i == 0) {
				cell->set_h_size_flags(SIZE_EXPAND_FILL);
			} else {
				cell->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
			}
		}
		MarginContainer *margin = memnew(MarginContainer);
		margin->add_theme_constant_override("margin_left", 4);
		margin->add_theme_constant_override("margin_right", 4);
		margin->add_theme_constant_override("margin_top", 2);
		margin->add_theme_constant_override("margin_bottom", 2);
		cell->add_child(margin);
		Label *label = memnew(Label);
		label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		label->set_text(headers[i]);
		label->set_h_size_flags(SIZE_EXPAND_FILL);
		margin->add_child(label);
		header_row->add_child(cell);
	}

	rows = memnew(VBoxContainer);
	rows->add_theme_constant_override("separation", 0);
	root->add_child(rows);

	add_button = memnew(Button);
	add_button->set_text("Add Sub-Emitter");
	add_button->connect("pressed", callable_mp(this, &YParticlesSubEmitterEditor::_add_pressed));
	root->add_child(add_button);

	_refresh();
}

Array YParticlesSubEmitterEditor::_get_entries() {
	Object *edited = get_edited_object();
	if (edited == nullptr) {
		return Array();
	}
	return edited->get(get_edited_property());
}

void YParticlesSubEmitterEditor::_set_entries(const Array &p_entries) {
	emit_changed(get_edited_property(), p_entries);
}

PanelContainer *YParticlesSubEmitterEditor::_make_cell(bool p_last) {
	PanelContainer *panel = memnew(PanelContainer);
	if (p_last) {
		panel->set_custom_minimum_size(Size2(34 * EDSCALE, 0));
	} else {
		panel->set_h_size_flags(SIZE_EXPAND_FILL);
	}
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 4);
	margin->add_theme_constant_override("margin_right", 4);
	margin->add_theme_constant_override("margin_top", 2);
	margin->add_theme_constant_override("margin_bottom", 2);
	panel->add_child(margin);
	return panel;
}

HBoxContainer *YParticlesSubEmitterEditor::_make_row(int p_index, const Array &p_entries) {
	static const char *event_names[] = { "Birth", "Collision", "Death" };

	Dictionary entry = p_entries[p_index];
	HBoxContainer *row = memnew(HBoxContainer);
	row->add_theme_constant_override("separation", 0);

	{
		PanelContainer *cell = _make_cell();
		Button *path_button = memnew(Button);
		path_button->set_h_size_flags(SIZE_EXPAND_FILL);
		path_button->set_text(String(entry.get("path", NodePath())));
		if (path_button->get_text().is_empty()) {
			path_button->set_text("Assign...");
		}
		path_button->set_tooltip_text("Select YParticles3D node");
		path_button->connect("pressed", callable_mp(this, &YParticlesSubEmitterEditor::_pick_entry_path).bind(p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(path_button);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell();
		OptionButton *event_button = memnew(OptionButton);
		for (int i = 0; i < 3; i++) {
			event_button->add_item(event_names[i], i);
		}
		event_button->select(CLAMP((int)entry.get("event", 0), 0, 2));
		event_button->set_h_size_flags(SIZE_EXPAND_FILL);
		event_button->connect("item_selected", callable_mp(this, &YParticlesSubEmitterEditor::_set_entry_event).bind(p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(event_button);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell();
		SpinBox *box = memnew(SpinBox);
		box->set_min(0.0);
		box->set_max(1.0);
		box->set_step(0.01);
		box->set_value((double)entry.get("probability", 1.0f));
		box->set_h_size_flags(SIZE_EXPAND_FILL);
		box->connect("value_changed", callable_mp(this, &YParticlesSubEmitterEditor::_set_entry_probability).bind(p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(box);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell();
		MenuButton *inherit_button = memnew(MenuButton);
		PopupMenu *popup = inherit_button->get_popup();
		popup->add_check_item("Nothing", 0);
		popup->add_check_item("Everything", 1);
		popup->add_check_item("Color", 2);
		popup->add_check_item("Size", 3);
		popup->add_check_item("Rotation", 4);
		popup->add_check_item("Lifetime", 5);
		popup->add_check_item("Duration", 6);
		popup->connect("id_pressed", callable_mp(this, &YParticlesSubEmitterEditor::_toggle_inherit_flag).bind(p_index));
		const int inherit_flags = (int)entry.get("inherit", 0);
		popup->set_item_checked(popup->get_item_index(0), inherit_flags == YParticles3D::SUB_EMITTER_INHERIT_NOTHING);
		popup->set_item_checked(popup->get_item_index(1), inherit_flags == YParticles3D::SUB_EMITTER_INHERIT_EVERYTHING);
		popup->set_item_checked(popup->get_item_index(2), (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_COLOR) != 0);
		popup->set_item_checked(popup->get_item_index(3), (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_SIZE) != 0);
		popup->set_item_checked(popup->get_item_index(4), (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_ROTATION) != 0);
		popup->set_item_checked(popup->get_item_index(5), (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_LIFETIME) != 0);
		popup->set_item_checked(popup->get_item_index(6), (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_DURATION) != 0);

		String label = "Nothing";
		if (inherit_flags == YParticles3D::SUB_EMITTER_INHERIT_EVERYTHING) {
			label = "Everything";
		} else if (inherit_flags != 0) {
			Vector<String> parts;
			if (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_COLOR) {
				parts.push_back("Color");
			}
			if (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_SIZE) {
				parts.push_back("Size");
			}
			if (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_ROTATION) {
				parts.push_back("Rotation");
			}
			if (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_LIFETIME) {
				parts.push_back("Lifetime");
			}
			if (inherit_flags & YParticles3D::SUB_EMITTER_INHERIT_DURATION) {
				parts.push_back("Duration");
			}
			PackedStringArray packed_parts;
			for (int part_i = 0; part_i < parts.size(); part_i++) {
				packed_parts.append(parts[part_i]);
			}
			label = String(", ").join(packed_parts);
		}
		inherit_button->set_text(label);
		inherit_button->set_h_size_flags(SIZE_EXPAND_FILL);
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(inherit_button);
		row->add_child(cell);
	}

	{
		PanelContainer *cell = _make_cell(true);
		Button *remove = memnew(Button);
		remove->set_text("X");
		remove->set_modulate(Color(0.8, 0.3, 0.3));
		remove->connect("pressed", callable_mp(this, &YParticlesSubEmitterEditor::_remove_pressed).bind(p_index));
		Object::cast_to<MarginContainer>(cell->get_child(0))->add_child(remove);
		row->add_child(cell);
	}

	return row;
}

void YParticlesSubEmitterEditor::_refresh() {
	if (fold_button != nullptr) {
		fold_button->set_text(expanded ? "v" : ">");
	}
	if (header_row != nullptr) {
		header_row->set_visible(expanded);
	}
	if (rows != nullptr) {
		rows->set_visible(expanded);
	}
	if (add_button != nullptr) {
		add_button->set_visible(expanded);
	}
	if (!expanded || rows == nullptr) {
		return;
	}
	for (int i = 0; i < rows->get_child_count(); i++) {
		rows->get_child(i)->queue_free();
	}
	Array entries = _get_entries();
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].get_type() == Variant::DICTIONARY) {
			rows->add_child(_make_row(i, entries));
		}
	}
}

void YParticlesSubEmitterEditor::_toggle_fold() {
	expanded = !expanded;
	_refresh();
}

void YParticlesSubEmitterEditor::_add_pressed() {
	Array entries = _get_entries();
	Dictionary entry;
	entry["path"] = NodePath();
	entry["event"] = (int)YParticles3D::SUB_EMITTER_CONDITION_BIRTH;
	entry["probability"] = 1.0f;
	entry["inherit"] = (int)YParticles3D::SUB_EMITTER_INHERIT_NOTHING;
	entries.append(entry);
	_set_entries(entries);
	expanded = true;
	_refresh();
}

void YParticlesSubEmitterEditor::_remove_pressed(int p_index) {
	Array entries = _get_entries();
	if (p_index < 0 || p_index >= entries.size()) {
		return;
	}
	entries.remove_at(p_index);
	_set_entries(entries);
	_refresh();
}

void YParticlesSubEmitterEditor::_set_entry_path(const String &p_value, int p_index) {
	Array entries = _get_entries();
	if (p_index < 0 || p_index >= entries.size() || entries[p_index].get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary entry = entries[p_index];
	entry["path"] = NodePath(p_value);
	entries[p_index] = entry;
	_set_entries(entries);
}

void YParticlesSubEmitterEditor::_pick_entry_path(int p_index) {
	TypedArray<StringName> valid_types;
	valid_types.push_back(StringName("YParticles3D"));
	Node *current_node = nullptr;
	Array entries = _get_entries();
	if (p_index >= 0 && p_index < entries.size() && entries[p_index].get_type() == Variant::DICTIONARY) {
		Dictionary entry = entries[p_index];
		const NodePath path = entry.get("path", NodePath());
		Node *edited_root = get_tree()->get_edited_scene_root();
		if (edited_root != nullptr && !path.is_empty()) {
			current_node = edited_root->get_node_or_null(path);
		}
	}
	EditorInterface::get_singleton()->popup_node_selector(callable_mp(this, &YParticlesSubEmitterEditor::_node_path_selected).bind(p_index), valid_types, current_node);
}

void YParticlesSubEmitterEditor::_node_path_selected(const NodePath &p_path, int p_index) {
	_set_entry_path(String(p_path), p_index);
	_refresh();
}

void YParticlesSubEmitterEditor::_set_entry_event(int p_selected, int p_index) {
	Array entries = _get_entries();
	if (p_index < 0 || p_index >= entries.size() || entries[p_index].get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary entry = entries[p_index];
	entry["event"] = CLAMP(p_selected, 0, 2);
	entries[p_index] = entry;
	_set_entries(entries);
}

void YParticlesSubEmitterEditor::_set_entry_probability(double p_value, int p_index) {
	Array entries = _get_entries();
	if (p_index < 0 || p_index >= entries.size() || entries[p_index].get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary entry = entries[p_index];
	entry["probability"] = CLAMP((float)p_value, 0.0f, 1.0f);
	entries[p_index] = entry;
	_set_entries(entries);
}

void YParticlesSubEmitterEditor::_toggle_inherit_flag(int p_id, int p_index) {
	Array entries = _get_entries();
	if (p_index < 0 || p_index >= entries.size() || entries[p_index].get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary entry = entries[p_index];
	int inherit_flags = (int)entry.get("inherit", 0);
	switch (p_id) {
		case 0:
			inherit_flags = YParticles3D::SUB_EMITTER_INHERIT_NOTHING;
			break;
		case 1:
			inherit_flags = YParticles3D::SUB_EMITTER_INHERIT_EVERYTHING;
			break;
		case 2:
			inherit_flags ^= YParticles3D::SUB_EMITTER_INHERIT_COLOR;
			break;
		case 3:
			inherit_flags ^= YParticles3D::SUB_EMITTER_INHERIT_SIZE;
			break;
		case 4:
			inherit_flags ^= YParticles3D::SUB_EMITTER_INHERIT_ROTATION;
			break;
		case 5:
			inherit_flags ^= YParticles3D::SUB_EMITTER_INHERIT_LIFETIME;
			break;
		case 6:
			inherit_flags ^= YParticles3D::SUB_EMITTER_INHERIT_DURATION;
			break;
	}
	if (inherit_flags != YParticles3D::SUB_EMITTER_INHERIT_EVERYTHING) {
		inherit_flags &= YParticles3D::SUB_EMITTER_INHERIT_EVERYTHING;
	}
	if (inherit_flags == 0) {
		inherit_flags = YParticles3D::SUB_EMITTER_INHERIT_NOTHING;
	}
	entry["inherit"] = inherit_flags;
	entries[p_index] = entry;
	_set_entries(entries);
	_refresh();
}

void YParticlesSubEmitterEditor::_update_property() {
	_refresh();
}

void YParticlesInspectorPlugin::_bind_methods() {
}

void YParticlesInspectorPlugin::_reset_state() {
	current_particles = nullptr;
	sections.clear();
	property_to_section.clear();
	mode_groups.clear();
	property_to_mode_group.clear();
}

void YParticlesInspectorPlugin::_build_sections() {
	Vector<SectionInfo> section_list;

	SectionInfo main;
	main.key = "main";
	main.title = "Main";
	main.first_property = "duration";
	main.properties.insert("duration");
	main.properties.insert("start_lifetime_mode");
	main.properties.insert("start_lifetime_constant");
	main.properties.insert("start_lifetime_random");
	main.properties.insert("start_lifetime_curve");
	main.properties.insert("start_speed_mode");
	main.properties.insert("start_speed_constant");
	main.properties.insert("start_speed_random");
	main.properties.insert("start_size_mode");
	main.properties.insert("start_size_constant");
	main.properties.insert("start_size_random");
	main.properties.insert("start_size_curve");
	main.properties.insert("start_size_curve_min");
	main.properties.insert("start_size_curve_max");
	main.properties.insert("start_size_square_random");
	main.properties.insert("start_size_constant_3d");
	main.properties.insert("start_size_random_min_3d");
	main.properties.insert("start_size_random_max_3d");
	main.properties.insert("start_size_x_curve");
	main.properties.insert("start_size_y_curve");
	main.properties.insert("start_size_z_curve");
	main.properties.insert("start_size_x_curve_min");
	main.properties.insert("start_size_y_curve_min");
	main.properties.insert("start_size_z_curve_min");
	main.properties.insert("start_rotation_degrees_mode");
	main.properties.insert("start_rotation_degrees_constant");
	main.properties.insert("start_rotation_degrees_random");
	main.properties.insert("start_rotation_degrees_curve");
	main.properties.insert("start_rotation_degrees_constant_3d");
	main.properties.insert("start_rotation_degrees_random_min_3d");
	main.properties.insert("start_rotation_degrees_random_max_3d");
	main.properties.insert("start_rotation_degrees_x_curve");
	main.properties.insert("start_rotation_degrees_y_curve");
	main.properties.insert("start_rotation_degrees_z_curve");
	main.properties.insert("gravity");
	main.properties.insert("use_world_space");
	section_list.push_back(main);

	SectionInfo play;
	play.key = "play";
	play.title = "Play Behavior";
	play.first_property = "play_on_start";
	play.properties.insert("play_on_start");
	play.properties.insert("loop");
	play.properties.insert("play_in_reverse");
	play.properties.insert("start_delay");
	play.properties.insert("start_delay_percentage");
	play.properties.insert("destroy_on_finish");
	play.properties.insert("debugging");
	play.properties.insert("playback_speed");
	play.properties.insert("paused");
	play.properties.insert("simulation_time");
	section_list.push_back(play);

	SectionInfo emission;
	emission.key = "emission";
	emission.title = "Emission";
	emission.first_property = "emitting";
	emission.toggle_property = "emitting";
	emission.properties.insert("emitting");
	emission.properties.insert("max_particles");
	emission.properties.insert("max_emissions_per_frame");
	emission.properties.insert("rate_over_time_mode");
	emission.properties.insert("rate_over_time");
	emission.properties.insert("rate_over_time_curve");
	emission.properties.insert("rate_over_distance");
	emission.properties.insert("bursts");
	section_list.push_back(emission);

	SectionInfo shape;
	shape.key = "shape";
	shape.title = "Shape";
	shape.first_property = "enable_shape";
	shape.toggle_property = "enable_shape";
	shape.properties.insert("enable_shape");
	shape.properties.insert("shape_type");
	shape.properties.insert("radius");
	shape.properties.insert("radius_thickness");
	shape.properties.insert("angle");
	shape.properties.insert("box_extents");
	shape.properties.insert("emission_mesh");
	shape.properties.insert("emission_mesh_scale");
	shape.properties.insert("random_direction");
	shape.properties.insert("spherize_direction");
	shape.properties.insert("emit_from");
	shape.properties.insert("shape_length");
	shape.properties.insert("arc_degrees");
	shape.properties.insert("arc_mode");
	shape.properties.insert("arc_spread");
	shape.properties.insert("arc_speed_mode");
	shape.properties.insert("arc_speed_constant");
	shape.properties.insert("arc_speed_curve");
	shape.properties.insert("direction_in_world_space");
	shape.properties.insert("invert_direction");
	shape.properties.insert("position_offset");
	shape.properties.insert("rotation_offset");
	section_list.push_back(shape);

	SectionInfo size_over_lifetime;
	size_over_lifetime.key = "size_over_lifetime";
	size_over_lifetime.title = "Size Over Lifetime";
	size_over_lifetime.first_property = "enable_size_over_lifetime";
	size_over_lifetime.toggle_property = "enable_size_over_lifetime";
	size_over_lifetime.properties.insert("enable_size_over_lifetime");
	size_over_lifetime.properties.insert("size_over_lifetime_use_two_curves");
	size_over_lifetime.properties.insert("size_over_lifetime");
	size_over_lifetime.properties.insert("size_over_lifetime_min");
	size_over_lifetime.properties.insert("width_over_lifetime");
	size_over_lifetime.properties.insert("height_over_lifetime");
	size_over_lifetime.properties.insert("depth_over_lifetime");
	section_list.push_back(size_over_lifetime);

	SectionInfo velocity;
	velocity.key = "velocity_over_lifetime";
	velocity.title = "Velocity Over Lifetime";
	velocity.first_property = "enable_velocity_over_lifetime";
	velocity.toggle_property = "enable_velocity_over_lifetime";
	velocity.properties.insert("enable_velocity_over_lifetime");
	velocity.properties.insert("velocity_over_lifetime_mode");
	velocity.properties.insert("velocity_over_lifetime_use_two_curves");
	velocity.properties.insert("velocity_over_lifetime");
	velocity.properties.insert("velocity_over_lifetime_min");
	velocity.properties.insert("velocity_over_lifetime_x");
	velocity.properties.insert("velocity_over_lifetime_x_min");
	velocity.properties.insert("velocity_over_lifetime_y");
	velocity.properties.insert("velocity_over_lifetime_y_min");
	velocity.properties.insert("velocity_over_lifetime_z");
	velocity.properties.insert("velocity_over_lifetime_z_min");
	velocity.properties.insert("offset_over_lifetime");
	velocity.properties.insert("velocity_in_world_space");
	section_list.push_back(velocity);

	SectionInfo force;
	force.key = "force_over_lifetime";
	force.title = "Force Over Lifetime";
	force.first_property = "enable_force_over_lifetime";
	force.toggle_property = "enable_force_over_lifetime";
	force.properties.insert("enable_force_over_lifetime");
	force.properties.insert("force_over_lifetime_mode");
	force.properties.insert("force_over_lifetime");
	force.properties.insert("force_over_lifetime_x");
	force.properties.insert("force_over_lifetime_y");
	force.properties.insert("force_over_lifetime_z");
	force.properties.insert("force_over_lifetime_constant");
	force.properties.insert("force_over_lifetime_random_min");
	force.properties.insert("force_over_lifetime_random_max");
	force.properties.insert("force_in_world_space");
	section_list.push_back(force);

	SectionInfo noise;
	noise.key = "noise";
	noise.title = "Noise";
	noise.first_property = "enable_noise";
	noise.toggle_property = "enable_noise";
	noise.properties.insert("enable_noise");
	noise.properties.insert("noise_strength");
	noise.properties.insert("noise_strength_mode");
	noise.properties.insert("noise_strength_curve");
	noise.properties.insert("noise_strength_x");
	noise.properties.insert("noise_strength_y");
	noise.properties.insert("noise_strength_z");
	noise.properties.insert("noise_scale");
	noise.properties.insert("noise_scroll_speed");
	noise.properties.insert("noise_position_amount");
	noise.properties.insert("noise_rotation_amount");
	noise.properties.insert("noise_size_amount");
	noise.properties.insert("noise_octaves");
	noise.properties.insert("noise_lacunarity");
	section_list.push_back(noise);

	SectionInfo attractor;
	attractor.key = "attractor";
	attractor.title = "Attractor";
	attractor.first_property = "enable_attractor";
	attractor.toggle_property = "enable_attractor";
	attractor.properties.insert("enable_attractor");
	attractor.properties.insert("attraction_target_mode");
	attractor.properties.insert("attractor_position");
	attractor.properties.insert("attraction_target");
	attractor.properties.insert("attraction_over_lifetime");
	section_list.push_back(attractor);

	SectionInfo limit_velocity;
	limit_velocity.key = "limit_velocity_over_lifetime";
	limit_velocity.title = "Limit Velocity Over Lifetime";
	limit_velocity.first_property = "enable_limit_velocity_over_lifetime";
	limit_velocity.toggle_property = "enable_limit_velocity_over_lifetime";
	limit_velocity.properties.insert("enable_limit_velocity_over_lifetime");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_mode");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_curve");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_axis");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_x_curve");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_y_curve");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_speed_z_curve");
	limit_velocity.properties.insert("limit_velocity_over_lifetime_dampen");
	section_list.push_back(limit_velocity);

	SectionInfo collision;
	collision.key = "collision";
	collision.title = "Collision";
	collision.first_property = "enable_collision";
	collision.toggle_property = "enable_collision";
	collision.properties.insert("enable_collision");
	collision.properties.insert("collision_layer");
	collision.properties.insert("collision_radius_scale");
	collision.properties.insert("collision_dampen");
	collision.properties.insert("collision_bounce");
	collision.properties.insert("collision_lifetime_loss");
	collision.properties.insert("collision_min_kill_speed");
	collision.properties.insert("collision_quality");
	collision.properties.insert("collision_voxel_size");
	section_list.push_back(collision);

	SectionInfo sub_emitters;
	sub_emitters.key = "sub_emitters";
	sub_emitters.title = "Sub Emitters";
	sub_emitters.first_property = "enable_sub_emitters";
	sub_emitters.toggle_property = "enable_sub_emitters";
	sub_emitters.properties.insert("enable_sub_emitters");
	sub_emitters.properties.insert("sub_emitters");
	section_list.push_back(sub_emitters);

	SectionInfo rotation;
	rotation.key = "rotation_over_lifetime";
	rotation.title = "Rotation Over Lifetime";
	rotation.first_property = "enable_rotation_over_lifetime";
	rotation.toggle_property = "enable_rotation_over_lifetime";
	rotation.properties.insert("enable_rotation_over_lifetime");
	rotation.properties.insert("rotation_over_lifetime");
	rotation.properties.insert("rotation_over_lifetime_axis");
	rotation.properties.insert("orbit_over_lifetime");
	rotation.properties.insert("orbit_around_axis");
	section_list.push_back(rotation);

	SectionInfo rotation_by_speed;
	rotation_by_speed.key = "rotation_by_speed";
	rotation_by_speed.title = "Rotation By Speed";
	rotation_by_speed.first_property = "enable_rotation_by_speed";
	rotation_by_speed.toggle_property = "enable_rotation_by_speed";
	rotation_by_speed.properties.insert("enable_rotation_by_speed");
	rotation_by_speed.properties.insert("rotation_by_speed_mode");
	rotation_by_speed.properties.insert("rotation_by_speed");
	rotation_by_speed.properties.insert("rotation_by_speed_x");
	rotation_by_speed.properties.insert("rotation_by_speed_y");
	rotation_by_speed.properties.insert("rotation_by_speed_z");
	rotation_by_speed.properties.insert("rotation_by_speed_range");
	section_list.push_back(rotation_by_speed);

	SectionInfo inherit_velocity;
	inherit_velocity.key = "inherit_velocity";
	inherit_velocity.title = "Inherit Velocity";
	inherit_velocity.first_property = "enable_inherit_velocity";
	inherit_velocity.toggle_property = "enable_inherit_velocity";
	inherit_velocity.properties.insert("enable_inherit_velocity");
	inherit_velocity.properties.insert("inherit_velocity_mode");
	inherit_velocity.properties.insert("inherit_velocity_multiplier");
	inherit_velocity.properties.insert("inherit_velocity_curve");
	section_list.push_back(inherit_velocity);

	SectionInfo color;
	color.key = "color_over_lifetime";
	color.title = "Color Over Lifetime";
	color.first_property = "enable_color_over_lifetime";
	color.toggle_property = "enable_color_over_lifetime";
	color.properties.insert("enable_color_over_lifetime");
	color.properties.insert("color_over_lifetime");
	color.properties.insert("color_over_lifetime_secondary");
	color.properties.insert("alpha_over_lifetime");
	color.properties.insert("alpha_over_lifetime_secondary");
	color.properties.insert("color_over_lifetime_use_two_gradients");
	color.properties.insert("starting_hue");
	color.properties.insert("hue_variation");
	section_list.push_back(color);

	SectionInfo sheet;
	sheet.key = "texture_sheet";
	sheet.title = "Texture Sheet Animation";
	sheet.first_property = "texture_sheet_enabled";
	sheet.toggle_property = "texture_sheet_enabled";
	sheet.properties.insert("texture_sheet_enabled");
	sheet.properties.insert("h_frames");
	sheet.properties.insert("v_frames");
	sheet.properties.insert("tiles_mode");
	sheet.properties.insert("use_random_starting_tile");
	sheet.properties.insert("start_index_tile");
	sheet.properties.insert("animation_cycles");
	sheet.properties.insert("frame_over_time");
	section_list.push_back(sheet);

	SectionInfo trails;
	trails.key = "trails";
	trails.title = "Trails";
	trails.first_property = "enable_trails";
	trails.toggle_property = "enable_trails";
	trails.properties.insert("enable_trails");
	trails.properties.insert("trail_ratio");
	trails.properties.insert("trail_lifetime_mode");
	trails.properties.insert("trail_lifetime");
	trails.properties.insert("trail_lifetime_curve");
	trails.properties.insert("trail_min_vertex_distance");
	trails.properties.insert("trail_world_space");
	trails.properties.insert("trail_die_with_particles");
	trails.properties.insert("trail_size_affects_width");
	trails.properties.insert("trail_size_affects_lifetime");
	trails.properties.insert("trail_inherit_particle_color");
	trails.properties.insert("trail_texture_mode");
	trails.properties.insert("trail_color_over_lifetime");
	trails.properties.insert("trail_color_over_trail");
	trails.properties.insert("trail_width_over_trail");
	trails.properties.insert("trail_texture");
	section_list.push_back(trails);

	SectionInfo rendering;
	rendering.key = "rendering";
	rendering.title = "Rendering";
	rendering.first_property = "start_color_mode";
	rendering.properties.insert("start_color_mode");
	rendering.properties.insert("start_color_use_two_gradients");
	rendering.properties.insert("tint_color");
	rendering.properties.insert("start_color_gradient");
	rendering.properties.insert("start_color_gradient_secondary");
	rendering.properties.insert("particle_texture");
	rendering.properties.insert("billboard_mode");
	rendering.properties.insert("render_alignment");
	rendering.properties.insert("velocity_stretch");
	rendering.properties.insert("length_stretch");
	rendering.properties.insert("align_to_velocity");
	rendering.properties.insert("align_offset_degrees");
	rendering.properties.insert("blend_mode");
	rendering.properties.insert("render_priority");
	rendering.properties.insert("sampling_filter");
	rendering.properties.insert("rendering_layer");
	rendering.properties.insert("override_material");
	rendering.properties.insert("custom_mesh");
	section_list.push_back(rendering);

	for (int i = 0; i < section_list.size(); i++) {
		sections.insert(section_list[i].key, section_list[i]);
		for (const String &prop : section_list[i].properties) {
			property_to_section.insert(prop, section_list[i].key);
		}
	}
}

void YParticlesInspectorPlugin::_build_mode_groups() {
	Vector<ModeGroup> groups;

	ModeGroup lifetime;
	lifetime.key = "start_lifetime";
	lifetime.label = "Starting Lifetime";
	lifetime.mode_property = "start_lifetime_mode";
	lifetime.first_property = "start_lifetime_mode";
	lifetime.labels.push_back("Constant");
	lifetime.labels.push_back("Random");
	lifetime.labels.push_back("Curve");
	lifetime.modes.resize(3);
	lifetime.modes.write[0].insert("start_lifetime_constant");
	lifetime.modes.write[1].insert("start_lifetime_random");
	lifetime.modes.write[2].insert("start_lifetime_curve");
	groups.push_back(lifetime);

	ModeGroup speed;
	speed.key = "start_speed";
	speed.label = "Starting Speed";
	speed.mode_property = "start_speed_mode";
	speed.first_property = "start_speed_mode";
	speed.labels.push_back("Constant");
	speed.labels.push_back("Random");
	speed.modes.resize(2);
	speed.modes.write[0].insert("start_speed_constant");
	speed.modes.write[1].insert("start_speed_random");
	groups.push_back(speed);

	ModeGroup size;
	size.key = "start_size";
	size.label = "Starting Size";
	size.mode_property = "start_size_mode";
	size.first_property = "start_size_mode";
	size.labels.push_back("Constant");
	size.labels.push_back("Random");
	size.labels.push_back("Curve");
	size.labels.push_back("Square Random");
	size.labels.push_back("Two Curves");
	size.labels.push_back("3D Constant");
	size.labels.push_back("3D Random");
	size.labels.push_back("3D Curve");
	size.labels.push_back("3D Two Curves");
	size.primary_values.push_back(0);
	size.primary_values.push_back(1);
	size.primary_values.push_back(2);
	size.primary_values.push_back(3);
	size.primary_values.push_back(4);
	size.primary_values.push_back(5);
	size.primary_values.push_back(6);
	size.primary_values.push_back(7);
	size.primary_values.push_back(8);
	size.secondary_values.resize(9);
	size.modes.resize(9);
	size.modes.write[0].insert("start_size_constant");
	size.modes.write[1].insert("start_size_random");
	size.modes.write[2].insert("start_size_curve");
	size.modes.write[3].insert("start_size_square_random");
	size.modes.write[4].insert("start_size_curve_min");
	size.modes.write[4].insert("start_size_curve_max");
	size.modes.write[5].insert("start_size_constant_3d");
	size.modes.write[6].insert("start_size_random_min_3d");
	size.modes.write[6].insert("start_size_random_max_3d");
	size.modes.write[7].insert("start_size_x_curve");
	size.modes.write[7].insert("start_size_y_curve");
	size.modes.write[7].insert("start_size_z_curve");
	size.modes.write[8].insert("start_size_x_curve_min");
	size.modes.write[8].insert("start_size_y_curve_min");
	size.modes.write[8].insert("start_size_z_curve_min");
	size.modes.write[8].insert("start_size_x_curve");
	size.modes.write[8].insert("start_size_y_curve");
	size.modes.write[8].insert("start_size_z_curve");
	groups.push_back(size);

	ModeGroup rotation;
	rotation.key = "start_rotation";
	rotation.label = "Start Rotation";
	rotation.mode_property = "start_rotation_degrees_mode";
	rotation.first_property = "start_rotation_degrees_mode";
	rotation.labels.push_back("Constant");
	rotation.labels.push_back("Random");
	rotation.labels.push_back("Curve");
	rotation.labels.push_back("3D Constant");
	rotation.labels.push_back("3D Random");
	rotation.labels.push_back("3D Curve");
	rotation.primary_values.push_back(0);
	rotation.primary_values.push_back(1);
	rotation.primary_values.push_back(2);
	rotation.primary_values.push_back(3);
	rotation.primary_values.push_back(4);
	rotation.primary_values.push_back(5);
	rotation.secondary_values.resize(6);
	rotation.modes.resize(6);
	rotation.modes.write[0].insert("start_rotation_degrees_constant");
	rotation.modes.write[1].insert("start_rotation_degrees_random");
	rotation.modes.write[2].insert("start_rotation_degrees_curve");
	rotation.modes.write[3].insert("start_rotation_degrees_constant_3d");
	rotation.modes.write[4].insert("start_rotation_degrees_random_min_3d");
	rotation.modes.write[4].insert("start_rotation_degrees_random_max_3d");
	rotation.modes.write[5].insert("start_rotation_degrees_x_curve");
	rotation.modes.write[5].insert("start_rotation_degrees_y_curve");
	rotation.modes.write[5].insert("start_rotation_degrees_z_curve");
	groups.push_back(rotation);

	ModeGroup arc_speed;
	arc_speed.key = "arc_speed";
	arc_speed.label = "Arc Speed";
	arc_speed.mode_property = "arc_speed_mode";
	arc_speed.first_property = "arc_speed_mode";
	arc_speed.labels.push_back("Constant");
	arc_speed.labels.push_back("Curve");
	arc_speed.modes.resize(2);
	arc_speed.modes.write[0].insert("arc_speed_constant");
	arc_speed.modes.write[1].insert("arc_speed_curve");
	groups.push_back(arc_speed);

	ModeGroup rate_over_time_group;
	rate_over_time_group.key = "rate_over_time";
	rate_over_time_group.label = "Rate Over Time";
	rate_over_time_group.mode_property = "rate_over_time_mode";
	rate_over_time_group.first_property = "rate_over_time_mode";
	rate_over_time_group.labels.push_back("Constant");
	rate_over_time_group.labels.push_back("Curve");
	rate_over_time_group.modes.resize(2);
	rate_over_time_group.modes.write[0].insert("rate_over_time");
	rate_over_time_group.modes.write[1].insert("rate_over_time_curve");
	groups.push_back(rate_over_time_group);

	ModeGroup velocity_mode;
	velocity_mode.key = "velocity_mode";
	velocity_mode.label = "Velocity Over Lifetime";
	velocity_mode.mode_property = "velocity_over_lifetime_mode";
	velocity_mode.secondary_mode_property = "velocity_over_lifetime_use_two_curves";
	velocity_mode.first_property = "velocity_over_lifetime_mode";
	velocity_mode.labels.push_back("Curve");
	velocity_mode.labels.push_back("Two Curves");
	velocity_mode.labels.push_back("Separate Axes");
	velocity_mode.labels.push_back("Axes Two Curves");
	velocity_mode.primary_values.push_back(0);
	velocity_mode.primary_values.push_back(0);
	velocity_mode.primary_values.push_back(1);
	velocity_mode.primary_values.push_back(1);
	velocity_mode.secondary_values.push_back(0);
	velocity_mode.secondary_values.push_back(1);
	velocity_mode.secondary_values.push_back(0);
	velocity_mode.secondary_values.push_back(1);
	velocity_mode.modes.resize(4);
	velocity_mode.modes.write[0].insert("velocity_over_lifetime");
	velocity_mode.modes.write[1].insert("velocity_over_lifetime");
	velocity_mode.modes.write[1].insert("velocity_over_lifetime_min");
	velocity_mode.modes.write[2].insert("velocity_over_lifetime_x");
	velocity_mode.modes.write[2].insert("velocity_over_lifetime_y");
	velocity_mode.modes.write[2].insert("velocity_over_lifetime_z");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_x");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_x_min");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_y");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_y_min");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_z");
	velocity_mode.modes.write[3].insert("velocity_over_lifetime_z_min");
	groups.push_back(velocity_mode);

	ModeGroup force_mode;
	force_mode.key = "force_mode";
	force_mode.label = "Force Over Lifetime";
	force_mode.mode_property = "force_over_lifetime_mode";
	force_mode.first_property = "force_over_lifetime_mode";
	force_mode.labels.push_back("Curve");
	force_mode.labels.push_back("Separate Axes");
	force_mode.labels.push_back("Constant");
	force_mode.labels.push_back("Random");
	force_mode.primary_values.push_back(0);
	force_mode.primary_values.push_back(1);
	force_mode.primary_values.push_back(2);
	force_mode.primary_values.push_back(3);
	force_mode.secondary_values.push_back(0);
	force_mode.secondary_values.push_back(0);
	force_mode.secondary_values.push_back(0);
	force_mode.secondary_values.push_back(0);
	force_mode.modes.resize(4);
	force_mode.modes.write[0].insert("force_over_lifetime");
	force_mode.modes.write[1].insert("force_over_lifetime_x");
	force_mode.modes.write[1].insert("force_over_lifetime_y");
	force_mode.modes.write[1].insert("force_over_lifetime_z");
	force_mode.modes.write[2].insert("force_over_lifetime_constant");
	force_mode.modes.write[3].insert("force_over_lifetime_random_min");
	force_mode.modes.write[3].insert("force_over_lifetime_random_max");
	groups.push_back(force_mode);

	ModeGroup noise_strength_mode;
	noise_strength_mode.key = "noise_strength_mode";
	noise_strength_mode.label = "Noise Strength";
	noise_strength_mode.mode_property = "noise_strength_mode";
	noise_strength_mode.first_property = "noise_strength_mode";
	noise_strength_mode.labels.push_back("Constant");
	noise_strength_mode.labels.push_back("Curve");
	noise_strength_mode.labels.push_back("Separate Axes");
	noise_strength_mode.modes.resize(3);
	noise_strength_mode.modes.write[0].insert("noise_strength");
	noise_strength_mode.modes.write[1].insert("noise_strength_curve");
	noise_strength_mode.modes.write[2].insert("noise_strength_x");
	noise_strength_mode.modes.write[2].insert("noise_strength_y");
	noise_strength_mode.modes.write[2].insert("noise_strength_z");
	groups.push_back(noise_strength_mode);

	ModeGroup attractor_source;
	attractor_source.key = "attractor_source";
	attractor_source.label = "Attraction Target";
	attractor_source.mode_property = "attraction_target_mode";
	attractor_source.first_property = "attraction_target_mode";
	attractor_source.labels.push_back("Global Position");
	attractor_source.labels.push_back("Node3D");
	attractor_source.modes.resize(2);
	attractor_source.modes.write[0].insert("attractor_position");
	attractor_source.modes.write[1].insert("attraction_target");
	groups.push_back(attractor_source);

	ModeGroup limit_velocity_speed_mode;
	limit_velocity_speed_mode.key = "limit_velocity_speed_mode";
	limit_velocity_speed_mode.label = "Speed";
	limit_velocity_speed_mode.mode_property = "limit_velocity_over_lifetime_speed_mode";
	limit_velocity_speed_mode.first_property = "limit_velocity_over_lifetime_speed_mode";
	limit_velocity_speed_mode.labels.push_back("Constant");
	limit_velocity_speed_mode.labels.push_back("Curve");
	limit_velocity_speed_mode.labels.push_back("Separate Axes");
	limit_velocity_speed_mode.labels.push_back("Axes Curves");
	limit_velocity_speed_mode.primary_values.push_back(0);
	limit_velocity_speed_mode.primary_values.push_back(1);
	limit_velocity_speed_mode.primary_values.push_back(2);
	limit_velocity_speed_mode.primary_values.push_back(3);
	limit_velocity_speed_mode.secondary_values.resize(4);
	limit_velocity_speed_mode.modes.resize(4);
	limit_velocity_speed_mode.modes.write[0].insert("limit_velocity_over_lifetime_speed");
	limit_velocity_speed_mode.modes.write[1].insert("limit_velocity_over_lifetime_speed_curve");
	limit_velocity_speed_mode.modes.write[2].insert("limit_velocity_over_lifetime_speed_axis");
	limit_velocity_speed_mode.modes.write[3].insert("limit_velocity_over_lifetime_speed_axis");
	limit_velocity_speed_mode.modes.write[3].insert("limit_velocity_over_lifetime_speed_x_curve");
	limit_velocity_speed_mode.modes.write[3].insert("limit_velocity_over_lifetime_speed_y_curve");
	limit_velocity_speed_mode.modes.write[3].insert("limit_velocity_over_lifetime_speed_z_curve");
	groups.push_back(limit_velocity_speed_mode);

	ModeGroup start_color_mode;
	start_color_mode.key = "start_color_mode";
	start_color_mode.label = "Start Color";
	start_color_mode.mode_property = "start_color_mode";
	start_color_mode.first_property = "start_color_mode";
	start_color_mode.labels.push_back("Tint");
	start_color_mode.labels.push_back("Gradient");
	start_color_mode.primary_values.push_back(0);
	start_color_mode.primary_values.push_back(1);
	start_color_mode.secondary_values.push_back(0);
	start_color_mode.secondary_values.push_back(0);
	start_color_mode.modes.resize(2);
	start_color_mode.modes.write[0].insert("tint_color");
	start_color_mode.modes.write[1].insert("start_color_gradient");
	groups.push_back(start_color_mode);

	ModeGroup color_gradient_mode;
	color_gradient_mode.key = "color_gradient_mode";
	color_gradient_mode.label = "Color Gradient Mode";
	color_gradient_mode.mode_property = "color_over_lifetime_use_two_gradients";
	color_gradient_mode.first_property = "color_over_lifetime_use_two_gradients";
	color_gradient_mode.labels.push_back("Single Gradient");
	color_gradient_mode.labels.push_back("Two Gradients");
	color_gradient_mode.primary_values.push_back(0);
	color_gradient_mode.primary_values.push_back(1);
	color_gradient_mode.secondary_values.push_back(0);
	color_gradient_mode.secondary_values.push_back(0);
	color_gradient_mode.modes.resize(2);
	color_gradient_mode.modes.write[0].insert("color_over_lifetime");
	color_gradient_mode.modes.write[0].insert("alpha_over_lifetime");
	color_gradient_mode.modes.write[1].insert("color_over_lifetime");
	color_gradient_mode.modes.write[1].insert("alpha_over_lifetime");
	color_gradient_mode.modes.write[1].insert("color_over_lifetime_secondary");
	color_gradient_mode.modes.write[1].insert("alpha_over_lifetime_secondary");
	groups.push_back(color_gradient_mode);

	ModeGroup start_color_gradient_mode;
	start_color_gradient_mode.key = "start_color_gradient_mode";
	start_color_gradient_mode.label = "Start Gradient Mode";
	start_color_gradient_mode.mode_property = "start_color_use_two_gradients";
	start_color_gradient_mode.first_property = "start_color_use_two_gradients";
	start_color_gradient_mode.labels.push_back("Single Gradient");
	start_color_gradient_mode.labels.push_back("Two Gradients");
	start_color_gradient_mode.primary_values.push_back(0);
	start_color_gradient_mode.primary_values.push_back(1);
	start_color_gradient_mode.secondary_values.push_back(0);
	start_color_gradient_mode.secondary_values.push_back(0);
	start_color_gradient_mode.modes.resize(2);
	start_color_gradient_mode.modes.write[0].insert("start_color_gradient");
	start_color_gradient_mode.modes.write[1].insert("start_color_gradient");
	start_color_gradient_mode.modes.write[1].insert("start_color_gradient_secondary");
	groups.push_back(start_color_gradient_mode);

	ModeGroup size_over_lifetime_mode;
	size_over_lifetime_mode.key = "size_over_lifetime_mode";
	size_over_lifetime_mode.label = "Size Over Lifetime";
	size_over_lifetime_mode.mode_property = "size_over_lifetime_use_two_curves";
	size_over_lifetime_mode.first_property = "size_over_lifetime_use_two_curves";
	size_over_lifetime_mode.labels.push_back("Single Curve");
	size_over_lifetime_mode.labels.push_back("Two Curves");
	size_over_lifetime_mode.primary_values.push_back(0);
	size_over_lifetime_mode.primary_values.push_back(1);
	size_over_lifetime_mode.secondary_values.push_back(0);
	size_over_lifetime_mode.secondary_values.push_back(0);
	size_over_lifetime_mode.modes.resize(2);
	size_over_lifetime_mode.modes.write[0].insert("size_over_lifetime");
	size_over_lifetime_mode.modes.write[1].insert("size_over_lifetime");
	size_over_lifetime_mode.modes.write[1].insert("size_over_lifetime_min");
	groups.push_back(size_over_lifetime_mode);

	ModeGroup trail_lifetime_mode;
	trail_lifetime_mode.key = "trail_lifetime_mode";
	trail_lifetime_mode.label = "Trail Lifetime";
	trail_lifetime_mode.mode_property = "trail_lifetime_mode";
	trail_lifetime_mode.first_property = "trail_lifetime_mode";
	trail_lifetime_mode.labels.push_back("Constant");
	trail_lifetime_mode.labels.push_back("Curve");
	trail_lifetime_mode.modes.resize(2);
	trail_lifetime_mode.modes.write[0].insert("trail_lifetime");
	trail_lifetime_mode.modes.write[1].insert("trail_lifetime_curve");
	groups.push_back(trail_lifetime_mode);

	ModeGroup rotation_by_speed_mode;
	rotation_by_speed_mode.key = "rotation_by_speed_mode";
	rotation_by_speed_mode.label = "Rotation By Speed";
	rotation_by_speed_mode.mode_property = "rotation_by_speed_mode";
	rotation_by_speed_mode.first_property = "rotation_by_speed_mode";
	rotation_by_speed_mode.labels.push_back("Curve");
	rotation_by_speed_mode.labels.push_back("Separate Axes");
	rotation_by_speed_mode.primary_values.push_back(0);
	rotation_by_speed_mode.primary_values.push_back(1);
	rotation_by_speed_mode.secondary_values.resize(2);
	rotation_by_speed_mode.modes.resize(2);
	rotation_by_speed_mode.modes.write[0].insert("rotation_by_speed");
	rotation_by_speed_mode.modes.write[1].insert("rotation_by_speed_x");
	rotation_by_speed_mode.modes.write[1].insert("rotation_by_speed_y");
	rotation_by_speed_mode.modes.write[1].insert("rotation_by_speed_z");
	groups.push_back(rotation_by_speed_mode);

	ModeGroup inherit_velocity_mode;
	inherit_velocity_mode.key = "inherit_velocity_mode";
	inherit_velocity_mode.label = "Inherit Velocity";
	inherit_velocity_mode.mode_property = "inherit_velocity_mode";
	inherit_velocity_mode.first_property = "inherit_velocity_mode";
	inherit_velocity_mode.labels.push_back("Current Constant");
	inherit_velocity_mode.labels.push_back("Current Curve");
	inherit_velocity_mode.labels.push_back("Initial Constant");
	inherit_velocity_mode.labels.push_back("Initial Curve");
	inherit_velocity_mode.primary_values.push_back(0);
	inherit_velocity_mode.primary_values.push_back(1);
	inherit_velocity_mode.primary_values.push_back(2);
	inherit_velocity_mode.primary_values.push_back(3);
	inherit_velocity_mode.secondary_values.resize(4);
	inherit_velocity_mode.modes.resize(4);
	inherit_velocity_mode.modes.write[0].insert("inherit_velocity_multiplier");
	inherit_velocity_mode.modes.write[1].insert("inherit_velocity_curve");
	inherit_velocity_mode.modes.write[2].insert("inherit_velocity_multiplier");
	inherit_velocity_mode.modes.write[3].insert("inherit_velocity_curve");
	groups.push_back(inherit_velocity_mode);

	for (int i = 0; i < groups.size(); i++) {
		mode_groups.insert(groups[i].key, groups[i]);
		property_to_mode_group.insert(groups[i].mode_property, groups[i].key);
		if (!groups[i].secondary_mode_property.is_empty()) {
			property_to_mode_group.insert(groups[i].secondary_mode_property, groups[i].key);
		}
		for (int j = 0; j < groups[i].modes.size(); j++) {
			for (const String &prop : groups[i].modes[j]) {
				property_to_mode_group.insert(prop, groups[i].key);
			}
		}
	}
}

bool YParticlesInspectorPlugin::_can_handle(Object *p_object) const {
	return Object::cast_to<YParticles3D>(p_object) != nullptr;
}

void YParticlesInspectorPlugin::_parse_begin(Object *p_object) {
	_reset_state();
	current_particles = Object::cast_to<YParticles3D>(p_object);
	if (current_particles == nullptr) {
		return;
	}
	_build_sections();
	_build_mode_groups();
}

bool YParticlesInspectorPlugin::_is_section_visible(const String &p_section_key) const {
	if (!sections.has(p_section_key)) {
		return true;
	}

	const SectionInfo &section = sections[p_section_key];
	if (!section.toggle_property.is_empty()) {
		return (bool)current_particles->get(section.toggle_property);
	}
	return true;
}

bool YParticlesInspectorPlugin::_passes_property_conditions(const String &p_property) const {
	if (current_particles == nullptr) {
		return true;
	}

	const YParticles3D::EmissionShape shape = current_particles->get_shape_type();
	const bool is_arc = shape == YParticles3D::EMISSION_SHAPE_CONE || shape == YParticles3D::EMISSION_SHAPE_SPHERE || shape == YParticles3D::EMISSION_SHAPE_CIRCLE || shape == YParticles3D::EMISSION_SHAPE_HEMISPHERE;

	if (p_property == "start_delay_percentage") {
		return current_particles->get_start_delay() > 0.001f;
	}
	if (p_property == "angle" || p_property == "emit_from") {
		return shape == YParticles3D::EMISSION_SHAPE_CONE;
	}
	if (p_property == "shape_length") {
		return (shape == YParticles3D::EMISSION_SHAPE_CONE && current_particles->get_emit_from() == YParticles3D::EMIT_FROM_VOLUME) || shape == YParticles3D::EMISSION_SHAPE_EDGE;
	}
	if (p_property == "radius" || p_property == "radius_thickness") {
		return shape != YParticles3D::EMISSION_SHAPE_BOX && shape != YParticles3D::EMISSION_SHAPE_EDGE;
	}
	if (p_property == "box_extents") {
		return shape == YParticles3D::EMISSION_SHAPE_BOX;
	}
	if (p_property == "emission_mesh" || p_property == "emission_mesh_scale") {
		return shape == YParticles3D::EMISSION_SHAPE_MESH;
	}
	if (p_property == "spherize_direction") {
		return shape != YParticles3D::EMISSION_SHAPE_SPHERE && shape != YParticles3D::EMISSION_SHAPE_HEMISPHERE;
	}
	if (p_property == "arc_degrees" || p_property == "arc_mode") {
		return is_arc;
	}
	if (p_property == "arc_spread" || p_property == "arc_speed_mode" || p_property == "arc_speed_constant" || p_property == "arc_speed_curve") {
		return is_arc && current_particles->get_arc_mode() != YParticles3D::ARC_MODE_RANDOM;
	}
	if (p_property == "start_index_tile") {
		return current_particles->get_texture_sheet_enabled() && !current_particles->get_use_random_starting_tile();
	}
	if (p_property == "velocity_stretch" || p_property == "length_stretch") {
		const YParticles3D::BillboardMode mode = current_particles->get_billboard_mode();
		return mode == YParticles3D::BILLBOARD_MODE_STRETCHED || mode == YParticles3D::BILLBOARD_MODE_STRETCHED_VERTICAL;
	}
	if (p_property == "start_color_gradient") {
		return current_particles->get_use_start_color_gradient();
	}
	if (p_property == "start_color_use_two_gradients" || p_property == "start_color_gradient_secondary") {
		return current_particles->get_use_start_color_gradient();
	}
	if (p_property == "tint_color") {
		return !current_particles->get_use_start_color_gradient();
	}
	if (p_property == "limit_velocity_over_lifetime_separate_axis") {
		return false;
	}
	if (p_property == "limit_velocity_over_lifetime_speed_mode") {
		return true;
	}
	if (p_property == "limit_velocity_over_lifetime_speed" || p_property == "limit_velocity_over_lifetime_speed_curve") {
		return current_particles->get_limit_velocity_over_lifetime_speed_mode() < 2;
	}
	if (p_property == "limit_velocity_over_lifetime_speed_axis") {
		return current_particles->get_limit_velocity_over_lifetime_speed_mode() >= 2;
	}
	if (p_property == "limit_velocity_over_lifetime_speed_x_curve" || p_property == "limit_velocity_over_lifetime_speed_y_curve" || p_property == "limit_velocity_over_lifetime_speed_z_curve") {
		return current_particles->get_limit_velocity_over_lifetime_speed_mode() == 3;
	}

	return true;
}

bool YParticlesInspectorPlugin::_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	YParticles3D *particles = Object::cast_to<YParticles3D>(p_object);
	if (particles == nullptr) {
		return false;
	}

	if (p_path == "bursts") {
		if (!_is_section_visible("emission")) {
			return true;
		}
		YParticlesBurstEditor *editor = memnew(YParticlesBurstEditor);
		add_property_editor(p_path, editor, false, "Bursts");
		return true;
	}

	if (p_path == "sub_emitters") {
		if (!_is_section_visible("sub_emitters")) {
			return true;
		}
		YParticlesSubEmitterEditor *editor = memnew(YParticlesSubEmitterEditor);
		add_property_editor(p_path, editor, false, "Sub Emitters");
		return true;
	}

	if (property_to_section.has(p_path)) {
		const SectionInfo &section = sections[property_to_section[p_path]];
		if (section.toggle_property == p_path) {
			YParticlesModuleToggleProperty *toggle_editor = memnew(YParticlesModuleToggleProperty);
			toggle_editor->setup(particles, "Enabled");
			add_property_editor(p_path, toggle_editor, false, "Enabled");
			return true;
		}
	}

	if (property_to_mode_group.has(p_path)) {
		const String key = property_to_mode_group[p_path];
		const ModeGroup &group = mode_groups[key];
		if (group.mode_property == p_path) {
			if (_passes_property_conditions(p_path) && (!property_to_section.has(p_path) || _is_section_visible(property_to_section[p_path]))) {
				YParticlesModeSelector *selector = memnew(YParticlesModeSelector);
				if (!group.secondary_mode_property.is_empty() || !group.primary_values.is_empty() || !group.secondary_values.is_empty()) {
					selector->setup_multi(particles, group.label, group.mode_property, group.secondary_mode_property, group.labels, group.primary_values, group.secondary_values);
				} else {
					selector->setup(particles, group.label, group.mode_property, group.labels);
				}
				add_property_editor(p_path, selector);
			}
			return true;
		}

		if (key == "start_color_mode") {
			if (!_passes_property_conditions(p_path)) {
				return true;
			}
		}

		int selected_mode = (int)particles->get(group.mode_property);
		if (!group.secondary_mode_property.is_empty()) {
			const int secondary = (bool)particles->get(group.secondary_mode_property) ? 1 : 0;
			selected_mode = -1;
			for (int i = 0; i < group.primary_values.size(); i++) {
				const int expected_secondary = i < group.secondary_values.size() ? group.secondary_values[i] : 0;
				if (group.primary_values[i] == (int)particles->get(group.mode_property) && expected_secondary == secondary) {
					selected_mode = i;
					break;
				}
			}
		}
		if (selected_mode < 0 || selected_mode >= group.modes.size() || !group.modes[selected_mode].has(p_path)) {
			return true;
		}
	}

	if (p_path == "start_lifetime_random" || p_path == "start_speed_random" || p_path == "start_rotation_degrees_random" || p_path == "start_size_square_random") {
		YParticlesRangeProperty *range_editor = memnew(YParticlesRangeProperty);
		double step = p_path == "start_rotation_degrees_random" ? 0.1 : 0.01;
		String label = "Random Range";
		if (p_path == "start_lifetime_random") {
			label = "Start Lifetime Random";
		} else if (p_path == "start_speed_random") {
			label = "Start Speed Random";
		} else if (p_path == "start_rotation_degrees_random") {
			label = "Start Rotation Degrees Random";
		} else if (p_path == "start_size_square_random") {
			label = "Start Size Square Random";
		}
		range_editor->setup(label, step);
		add_property_editor(p_path, range_editor, false, label);
		return true;
	}

	if (p_path == "start_size_random") {
		YParticlesSizeRangeProperty *size_editor = memnew(YParticlesSizeRangeProperty);
		size_editor->setup("Start Size Random", 0.01);
		add_property_editor(p_path, size_editor, false, "Start Size Random");
		return true;
	}

	if (property_to_section.has(p_path) && !_is_section_visible(property_to_section[p_path])) {
		const SectionInfo &section = sections[property_to_section[p_path]];
		if (section.toggle_property != p_path) {
			return true;
		}
	}

	if (!_passes_property_conditions(p_path)) {
		return true;
	}

	return false;
}

void YParticlesInspectorPlugin::_parse_end(Object *p_object) {
	_reset_state();
}

void YParticlesEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_main_screen_changed", "screen_name"), &YParticlesEditorPlugin::_on_main_screen_changed);
	ClassDB::bind_method(D_METHOD("_on_editor_selection_changed"), &YParticlesEditorPlugin::_on_editor_selection_changed);
}

YParticlesEditorPlugin::YParticlesEditorPlugin() {
	inspector_plugin.instantiate();
	gizmo_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}

YParticlesEditorPlugin::~YParticlesEditorPlugin() {
	inspector_plugin.unref();
	gizmo_plugin.unref();
}

void YParticlesEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (gizmo_plugin.is_valid()) {
			add_node_3d_gizmo_plugin(gizmo_plugin);
		}
		connect("main_screen_changed", callable_mp(this, &YParticlesEditorPlugin::_on_main_screen_changed));

		EditorSelection *selection = get_editor_interface()->get_selection();
		if (selection != nullptr && !selection->is_connected("selection_changed", callable_mp(this, &YParticlesEditorPlugin::_on_editor_selection_changed))) {
			selection->connect("selection_changed", callable_mp(this, &YParticlesEditorPlugin::_on_editor_selection_changed));
		}

		VBoxContainer *main_screen = get_editor_interface()->get_editor_main_screen();
		if (main_screen != nullptr) {
			for (int i = 0; i < main_screen->get_child_count(); i++) {
				Node *child = main_screen->get_child(i);
				Control *control = Object::cast_to<Control>(child);
				if (control != nullptr && control->is_visible_in_tree()) {
					current_main_screen_name = control->get_class().get_basename();
					break;
				}
			}
		}

		if (current_main_screen_name.contains("3D")) {
			_on_editor_selection_changed();
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (gizmo_plugin.is_valid()) {
			remove_node_3d_gizmo_plugin(gizmo_plugin);
		}
		if (is_connected("main_screen_changed", callable_mp(this, &YParticlesEditorPlugin::_on_main_screen_changed))) {
			disconnect("main_screen_changed", callable_mp(this, &YParticlesEditorPlugin::_on_main_screen_changed));
		}

		EditorSelection *selection = get_editor_interface()->get_selection();
		if (selection != nullptr && selection->is_connected("selection_changed", callable_mp(this, &YParticlesEditorPlugin::_on_editor_selection_changed))) {
			selection->disconnect("selection_changed", callable_mp(this, &YParticlesEditorPlugin::_on_editor_selection_changed));
		}

		if (preview != nullptr) {
			preview->queue_free();
			preview = nullptr;
		}
		has_instantiated_viewport_preview = false;
	}
}

void YParticlesEditorPlugin::_on_main_screen_changed(const String &p_screen_name) {
	current_main_screen_name = p_screen_name;
	if (preview != nullptr) {
		preview->unlink_particles();
	}
	_on_editor_selection_changed();
}

void YParticlesEditorPlugin::_on_editor_selection_changed() {
	const bool in_3d_screen = current_main_screen_name.is_empty() || current_main_screen_name.contains("3D");
	TypedArray<Node> selected_nodes = get_editor_interface()->get_selection()->get_selected_nodes();
	YParticles3D *particles = _find_particles_or_null(selected_nodes);

	if (!has_instantiated_viewport_preview) {
		if (in_3d_screen || particles != nullptr) {
			has_instantiated_viewport_preview = true;
			VBoxContainer *main_screen = get_editor_interface()->get_editor_main_screen();
			Control *viewport_control = _find_3d_viewport_control(main_screen);
			if (viewport_control != nullptr) {
				preview = memnew(YParticlesPreview);
				preview->request_hide();
				viewport_control->add_child(preview);
			}
		}
	}

	if (preview == nullptr) {
		return;
	}

	if (particles != nullptr && in_3d_screen) {
		preview->link_with_particles(particles);
		preview->request_show();
		preview->view_changed(true);
	} else {
		preview->unlink_particles();
		preview->request_hide();
		preview->view_changed(false);
	}
}

YParticles3D *YParticlesEditorPlugin::_find_particles_or_null(const TypedArray<Node> &p_nodes) const {
	for (int i = 0; i < p_nodes.size(); i++) {
		YParticles3D *particles = Object::cast_to<YParticles3D>(p_nodes[i]);
		if (particles != nullptr) {
			return particles;
		}
	}
	return nullptr;
}

Control *YParticlesEditorPlugin::_find_3d_viewport_control(Node *p_node) const {
	Node *editor_viewport = _find_node_by_class(p_node, "Node3DEditorViewport");
	if (editor_viewport == nullptr) {
		editor_viewport = _find_node_by_class(p_node, "SubViewportContainer");
		if (editor_viewport == nullptr) {
			return Object::cast_to<Control>(p_node);
		}
	}
	for (int i = 0; i < editor_viewport->get_child_count(); i++) {
		Control *control = Object::cast_to<Control>(editor_viewport->get_child(i));
		if (control != nullptr && control->get_class() != "SubViewportContainer") {
			return control;
		}
	}
	return Object::cast_to<Control>(editor_viewport);
}

Node *YParticlesEditorPlugin::_find_node_by_class(Node *p_node, const String &p_class_name) const {
	if (p_node == nullptr) {
		return nullptr;
	}
	if (p_node->get_class() == p_class_name) {
		return p_node;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *found = _find_node_by_class(p_node->get_child(i), p_class_name);
		if (found != nullptr) {
			return found;
		}
	}
	return nullptr;
}

#endif // TOOLS_ENABLED
