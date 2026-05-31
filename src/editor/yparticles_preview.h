#ifndef YPARTICLES_PREVIEW_H
#define YPARTICLES_PREVIEW_H

#ifdef TOOLS_ENABLED

#include "../yparticles3d.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

class YParticlesPreview : public Control {
	GDCLASS(YParticlesPreview, Control);

	YParticles3D *linked_particles = nullptr;
	Button *play_button = nullptr;
	Button *stop_button = nullptr;
	Button *restart_button = nullptr;
	SpinBox *speed_spinbox = nullptr;
	Label *actual_time_label = nullptr;
	Label *actual_particles_label = nullptr;
	bool paused_by_view_change = false;
	bool updating_speed = false;

	void _on_play_pressed();
	void _on_stop_pressed();
	void _on_restart_pressed();
	void _on_speed_changed(double p_value);
	void _update_play_button_state();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void link_with_particles(YParticles3D *p_particles);
	void unlink_particles();
	void request_show();
	void request_hide();
	void view_changed(bool p_is_viewing);

	YParticlesPreview();
};

#endif // TOOLS_ENABLED

#endif // YPARTICLES_PREVIEW_H
