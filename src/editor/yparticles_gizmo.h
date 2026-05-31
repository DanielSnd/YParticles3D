#ifndef YPARTICLES_GIZMO_H
#define YPARTICLES_GIZMO_H

#ifdef TOOLS_ENABLED

#include "../yparticles3d.h"
#include <godot_cpp/classes/editor_node3d_gizmo.hpp>
#include <godot_cpp/classes/editor_node3d_gizmo_plugin.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

using namespace godot;

class YParticlesGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(YParticlesGizmoPlugin, EditorNode3DGizmoPlugin);

	static constexpr float GIZMO_EPSILON = 0.0001f;
	bool materials_ready = false;

	static void _bind_methods();

	void _append_line(PackedVector3Array &r_lines, const Vector3 &p_a, const Vector3 &p_b) const;
	int _get_arc_degrees(const YParticles3D *p_particles) const;
	bool _should_draw_inner_shell(float p_outer_radius, float p_inner_radius) const;
	Vector3 _transform_point(const Vector3 &p_point, const Transform3D &p_transform, const YParticles3D *p_particles) const;
	void _draw_cone(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);
	void _draw_sphere(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);
	void _draw_box(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);
	void _draw_circle(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);
	void _draw_edge(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);
	void _draw_hemisphere(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform);

public:
	bool _has_gizmo(Node3D *p_spatial) const override;
	String _get_gizmo_name() const override;
	int32_t _get_priority() const override;
	bool _is_selectable_when_hidden() const override;
	String _get_handle_name(const Ref<EditorNode3DGizmo> &p_gizmo, int32_t p_id, bool p_secondary) const override;
	void _redraw(const Ref<EditorNode3DGizmo> &p_gizmo) override;

	YParticlesGizmoPlugin();
};

#endif // TOOLS_ENABLED

#endif // YPARTICLES_GIZMO_H
