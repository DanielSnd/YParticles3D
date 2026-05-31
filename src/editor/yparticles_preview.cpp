#ifdef TOOLS_ENABLED

#include "yparticles_preview.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>

using namespace godot;

static float _yparticles_editor_scale() {
	EditorInterface *editor_interface = EditorInterface::get_singleton();
	return editor_interface != nullptr ? editor_interface->get_editor_scale() : 1.0f;
}

#define EDSCALE _yparticles_editor_scale()

void YParticlesPreview::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_play_pressed"), &YParticlesPreview::_on_play_pressed);
	ClassDB::bind_method(D_METHOD("_on_stop_pressed"), &YParticlesPreview::_on_stop_pressed);
	ClassDB::bind_method(D_METHOD("_on_restart_pressed"), &YParticlesPreview::_on_restart_pressed);
	ClassDB::bind_method(D_METHOD("_on_speed_changed", "value"), &YParticlesPreview::_on_speed_changed);
	ClassDB::bind_method(D_METHOD("link_with_particles", "particles"), &YParticlesPreview::link_with_particles);
	ClassDB::bind_method(D_METHOD("unlink_particles"), &YParticlesPreview::unlink_particles);
	ClassDB::bind_method(D_METHOD("request_show"), &YParticlesPreview::request_show);
	ClassDB::bind_method(D_METHOD("request_hide"), &YParticlesPreview::request_hide);
	ClassDB::bind_method(D_METHOD("view_changed", "is_viewing"), &YParticlesPreview::view_changed);
}

YParticlesPreview::YParticlesPreview() {
	set_anchors_and_offsets_preset(PRESET_BOTTOM_RIGHT);
	set_h_grow_direction(GROW_DIRECTION_BEGIN);
	set_v_grow_direction(GROW_DIRECTION_BEGIN);
	set_offset(SIDE_LEFT, -260 * EDSCALE);
	set_offset(SIDE_TOP, -190 * EDSCALE);
	set_offset(SIDE_RIGHT, -12 * EDSCALE);
	set_offset(SIDE_BOTTOM, -24 * EDSCALE);
	set_custom_minimum_size(Size2(248, 170) * EDSCALE);
	set_mouse_filter(MOUSE_FILTER_PASS);

	MarginContainer *outer_margin = memnew(MarginContainer);
	outer_margin->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	add_child(outer_margin);

	PanelContainer *panel = memnew(PanelContainer);
	Ref<StyleBoxFlat> panel_style;
	panel_style.instantiate();
	panel_style->set_bg_color(Color(0.165f, 0.165f, 0.165f, 0.82f));
	panel_style->set_border_width_all(2 * EDSCALE);
	panel_style->set_border_color(Color(0.09f, 0.09f, 0.09f, 0.45f));
	panel_style->set_corner_radius_all(6 * EDSCALE);
	panel->add_theme_stylebox_override("panel", panel_style);
	outer_margin->add_child(panel);

	MarginContainer *inner_margin = memnew(MarginContainer);
	inner_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	inner_margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	inner_margin->add_theme_constant_override("margin_right", 50 * EDSCALE);
	inner_margin->add_theme_constant_override("margin_bottom", 50 * EDSCALE);
	panel->add_child(inner_margin);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->add_theme_constant_override("separation", 10 * EDSCALE);
	inner_margin->add_child(vbox);

	Label *title = memnew(Label);
	title->set_text("Particles");
	title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	vbox->add_child(title);

	HBoxContainer *buttons = memnew(HBoxContainer);
	buttons->add_theme_constant_override("separation", 8 * EDSCALE);
	vbox->add_child(buttons);

	play_button = memnew(Button);
	play_button->set_text("Play");
	play_button->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
	play_button->set_h_size_flags(SIZE_EXPAND_FILL);
	play_button->connect("pressed", callable_mp(this, &YParticlesPreview::_on_play_pressed));
	buttons->add_child(play_button);

	restart_button = memnew(Button);
	restart_button->set_text("Restart");
	restart_button->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
	restart_button->set_h_size_flags(SIZE_EXPAND_FILL);
	restart_button->connect("pressed", callable_mp(this, &YParticlesPreview::_on_restart_pressed));
	buttons->add_child(restart_button);

	stop_button = memnew(Button);
	stop_button->set_text("Stop");
	stop_button->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
	stop_button->set_h_size_flags(SIZE_EXPAND_FILL);
	stop_button->connect("pressed", callable_mp(this, &YParticlesPreview::_on_stop_pressed));
	buttons->add_child(stop_button);

	HBoxContainer *speed_row = memnew(HBoxContainer);
	speed_row->add_theme_constant_override("separation", 12 * EDSCALE);
	vbox->add_child(speed_row);

	Label *speed_label = memnew(Label);
	speed_label->set_text("Speed:");
	speed_row->add_child(speed_label);

	speed_spinbox = memnew(SpinBox);
	speed_spinbox->set_h_size_flags(SIZE_EXPAND_FILL);
	speed_spinbox->set_max(5.0);
	speed_spinbox->set_min(0.0);
	speed_spinbox->set_step(0.01);
	speed_spinbox->set_value(1.0);
	speed_spinbox->connect("value_changed", callable_mp(this, &YParticlesPreview::_on_speed_changed));
	speed_row->add_child(speed_spinbox);

	HBoxContainer *time_row = memnew(HBoxContainer);
	time_row->add_theme_constant_override("separation", 12 * EDSCALE);
	vbox->add_child(time_row);

	Label *time_label = memnew(Label);
	time_label->set_text("Time:");
	time_row->add_child(time_label);

	actual_time_label = memnew(Label);
	actual_time_label->set_h_size_flags(SIZE_EXPAND_FILL);
	actual_time_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	actual_time_label->set_text("0.00");
	time_row->add_child(actual_time_label);

	HBoxContainer *particles_row = memnew(HBoxContainer);
	particles_row->add_theme_constant_override("separation", 12 * EDSCALE);
	vbox->add_child(particles_row);

	Label *particles_label = memnew(Label);
	particles_label->set_text("Particles:");
	particles_row->add_child(particles_label);

	actual_particles_label = memnew(Label);
	actual_particles_label->set_h_size_flags(SIZE_EXPAND_FILL);
	actual_particles_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	actual_particles_label->set_text("0");
	particles_row->add_child(actual_particles_label);

	set_visible(false);
	set_process(false);
	_update_play_button_state();
}

void YParticlesPreview::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		if (linked_particles != nullptr) {
			actual_time_label->set_text(String::num(linked_particles->get_simulation_time(), 2));
			actual_particles_label->set_text(itos(linked_particles->get_visible_particle_count()));
			_update_play_button_state();
		}
	}
}

void YParticlesPreview::_update_play_button_state() {
	if (play_button == nullptr) {
		return;
	}
	if (linked_particles == nullptr) {
		play_button->set_text("Play");
		return;
	}

	if (linked_particles->is_playing()) {
		play_button->set_text(linked_particles->is_paused() ? "Resume" : "Pause");
	} else {
		play_button->set_text("Play");
	}
}

void YParticlesPreview::link_with_particles(YParticles3D *p_particles) {
	linked_particles = p_particles;
	if (linked_particles != nullptr && speed_spinbox != nullptr) {
		updating_speed = true;
		speed_spinbox->set_value(linked_particles->get_playback_speed());
		updating_speed = false;
	}
	_update_play_button_state();
}

void YParticlesPreview::unlink_particles() {
	linked_particles = nullptr;
	actual_time_label->set_text("0.00");
	actual_particles_label->set_text("0");
	_update_play_button_state();
}

void YParticlesPreview::request_show() {
	set_visible(true);
	set_process(true);
}

void YParticlesPreview::request_hide() {
	set_visible(false);
	set_process(false);
}

void YParticlesPreview::view_changed(bool p_is_viewing) {
	if (linked_particles == nullptr) {
		return;
	}
	if (p_is_viewing) {
		if (paused_by_view_change && linked_particles->is_playing() && linked_particles->is_paused()) {
			linked_particles->set_paused(false);
			paused_by_view_change = false;
		}
	} else {
		if (!paused_by_view_change && linked_particles->is_playing() && !linked_particles->is_paused()) {
			linked_particles->set_paused(true);
			paused_by_view_change = true;
		}
	}
	_update_play_button_state();
}

void YParticlesPreview::_on_play_pressed() {
	if (linked_particles == nullptr) {
		return;
	}
	if (linked_particles->is_playing()) {
		linked_particles->set_paused(!linked_particles->is_paused());
	} else {
		linked_particles->play();
	}
	_update_play_button_state();
}

void YParticlesPreview::_on_stop_pressed() {
	if (linked_particles != nullptr) {
		linked_particles->stop(true);
		linked_particles->clear();
		linked_particles->set_paused(false);
	}
	_update_play_button_state();
}

void YParticlesPreview::_on_restart_pressed() {
	if (linked_particles != nullptr) {
		linked_particles->stop(true);
		linked_particles->clear();
		linked_particles->set_paused(false);
		linked_particles->play(true);
	}
	_update_play_button_state();
}

void YParticlesPreview::_on_speed_changed(double p_value) {
	if (!updating_speed && linked_particles != nullptr) {
		linked_particles->set_playback_speed((float)p_value);
	}
}

#endif // TOOLS_ENABLED
