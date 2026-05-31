#include "yparticles_gizmo.h"

#ifdef TOOLS_ENABLED

using namespace godot;

void YParticlesGizmoPlugin::_bind_methods() {
}

YParticlesGizmoPlugin::YParticlesGizmoPlugin() {
}

bool YParticlesGizmoPlugin::_has_gizmo(Node3D *p_spatial) const {
	return Object::cast_to<YParticles3D>(p_spatial) != nullptr;
}

String YParticlesGizmoPlugin::_get_gizmo_name() const {
	return "YParticles3D";
}

int32_t YParticlesGizmoPlugin::_get_priority() const {
	return -1;
}

bool YParticlesGizmoPlugin::_is_selectable_when_hidden() const {
	return true;
}

String YParticlesGizmoPlugin::_get_handle_name(const Ref<EditorNode3DGizmo> &p_gizmo, int32_t p_id, bool p_secondary) const {
	return "YParticles3D";
}

void YParticlesGizmoPlugin::_append_line(PackedVector3Array &r_lines, const Vector3 &p_a, const Vector3 &p_b) const {
	if (p_a.is_equal_approx(p_b)) {
		return;
	}
	r_lines.push_back(p_a);
	r_lines.push_back(p_b);
}

int YParticlesGizmoPlugin::_get_arc_degrees(const YParticles3D *p_particles) const {
	return CLAMP((int)Math::round(p_particles->get_arc_degrees()), 0, 360);
}

bool YParticlesGizmoPlugin::_should_draw_inner_shell(float p_outer_radius, float p_inner_radius) const {
	if (p_outer_radius <= GIZMO_EPSILON) {
		return false;
	}
	if (p_inner_radius <= GIZMO_EPSILON) {
		return false;
	}
	if (p_inner_radius <= p_outer_radius * 0.01f) {
		return false;
	}
	return !Math::is_equal_approx(p_outer_radius, p_inner_radius);
}

Vector3 YParticlesGizmoPlugin::_transform_point(const Vector3 &p_point, const Transform3D &p_transform, const YParticles3D *p_particles) const {
	Vector3 result = p_transform.xform(p_point);
	if (p_particles->is_direction_in_world_space()) {
		result = p_particles->get_transform().basis.inverse().xform(result);
	}
	return result;
}

void YParticlesGizmoPlugin::_draw_cone(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const float height = p_particles->get_shape_length();
	const float outer_radius = p_particles->get_radius();
	const float cone_angle = Math::deg_to_rad(CLAMP(p_particles->get_angle(), 0.0f, 89.99f));
	const int arc_degrees = _get_arc_degrees(p_particles);
	const float inner_radius = outer_radius * (1.0f - p_particles->get_radius_thickness());
	float inner_top_radius = inner_radius + (height * Math::tan(cone_angle));
	inner_top_radius = Math::lerp(inner_top_radius, inner_top_radius * (1.0f - p_particles->get_radius_thickness()), 1.0f - Math::inverse_lerp(0.0f, 90.0f, p_particles->get_angle()));
	inner_top_radius = MAX(inner_top_radius, inner_radius);
	const float outer_top_radius = outer_radius + (height * Math::tan(cone_angle));
	const bool draw_inner_shell = _should_draw_inner_shell(outer_radius, inner_radius) || _should_draw_inner_shell(outer_top_radius, inner_top_radius);
	const int shell_count = draw_inner_shell ? 2 : 1;

	for (int cone_index = 0; cone_index < shell_count; cone_index++) {
		const bool is_inner = cone_index == 1;
		const float cone_radius = is_inner ? inner_radius : outer_radius;
		const float top_radius = is_inner ? inner_top_radius : outer_top_radius;

		for (int i = 0; i <= arc_degrees; i++) {
			const float radius_a = Math::deg_to_rad((float)i);
			const float radius_b = Math::deg_to_rad((float)MIN(i + 1, arc_degrees));
			const Vector2 point_a = Vector2(Math::cos(radius_a), Math::sin(radius_a)) * cone_radius;
			const Vector2 point_b = Vector2(Math::cos(radius_b), Math::sin(radius_b)) * cone_radius;
			const Vector3 base_a = _transform_point(Vector3(point_a.x, 0.0f, point_a.y), p_transform, p_particles);
			const Vector3 base_b = _transform_point(Vector3(point_b.x, 0.0f, point_b.y), p_transform, p_particles);
			_append_line(lines, base_a, base_b);

			const Vector2 top_point_a = Vector2(Math::cos(radius_a), Math::sin(radius_a)) * top_radius;
			const Vector2 top_point_b = Vector2(Math::cos(radius_b), Math::sin(radius_b)) * top_radius;
			const Vector3 top_a = _transform_point(Vector3(top_point_a.x, height, top_point_a.y), p_transform, p_particles);
			const Vector3 top_b = _transform_point(Vector3(top_point_b.x, height, top_point_b.y), p_transform, p_particles);
			_append_line(lines, top_a, top_b);

			if ((i % 45) == 0 && i <= arc_degrees) {
				_append_line(lines, base_a, top_a);
			}
		}
	}

	if (arc_degrees < 360) {
		for (int cone_index = 0; cone_index < shell_count; cone_index++) {
			const float cone_radius = cone_index == 1 ? inner_radius : outer_radius;
			const float top_radius = cone_radius + (height * Math::tan(cone_angle));
			const Vector3 start_base = _transform_point(Vector3(cone_radius, 0.0f, 0.0f), p_transform, p_particles);
			const Vector3 start_top = _transform_point(Vector3(top_radius, height, 0.0f), p_transform, p_particles);
			_append_line(lines, start_base, start_top);

			const float end_angle = Math::deg_to_rad((float)arc_degrees);
			const Vector3 end_base = _transform_point(Vector3(Math::cos(end_angle) * cone_radius, 0.0f, Math::sin(end_angle) * cone_radius), p_transform, p_particles);
			const Vector3 end_top = _transform_point(Vector3(Math::cos(end_angle) * top_radius, height, Math::sin(end_angle) * top_radius), p_transform, p_particles);
			_append_line(lines, end_base, end_top);
		}
	}

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_draw_sphere(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const float outer_radius = p_particles->get_radius();
	const float inner_radius = p_particles->get_radius() * (1.0f - p_particles->get_radius_thickness());
	const int arc_degrees = _get_arc_degrees(p_particles);
	const bool draw_inner_shell = _should_draw_inner_shell(outer_radius, inner_radius);
	const int shell_count = draw_inner_shell ? 2 : 1;

	for (int radius_index = 0; radius_index < shell_count; radius_index++) {
		const float radius = radius_index == 1 ? inner_radius : outer_radius;
		for (int plane = 0; plane < 3; plane++) {
			for (int i = 0; i <= arc_degrees; i++) {
				const float angle_a = Math::deg_to_rad((float)i);
				const float angle_b = Math::deg_to_rad((float)MIN(i + 1, arc_degrees));
				Vector3 point_a;
				Vector3 point_b;

				switch (plane) {
					case 0:
						point_a = Vector3(radius * Math::cos(angle_a), radius * Math::sin(angle_a), 0.0f);
						point_b = Vector3(radius * Math::cos(angle_b), radius * Math::sin(angle_b), 0.0f);
						break;
					case 1:
						point_a = Vector3(radius * Math::cos(angle_a), 0.0f, radius * Math::sin(angle_a));
						point_b = Vector3(radius * Math::cos(angle_b), 0.0f, radius * Math::sin(angle_b));
						break;
					default:
						point_a = Vector3(0.0f, radius * Math::cos(angle_a), radius * Math::sin(angle_a));
						point_b = Vector3(0.0f, radius * Math::cos(angle_b), radius * Math::sin(angle_b));
						break;
				}

				_append_line(lines, _transform_point(point_a, p_transform, p_particles), _transform_point(point_b, p_transform, p_particles));
			}
		}
	}

	if (draw_inner_shell) {
		for (int i = 0; i <= arc_degrees; i += 45) {
		const float angle = Math::deg_to_rad((float)MIN(i, arc_degrees));
		_append_line(lines, _transform_point(Vector3(outer_radius * Math::cos(angle), outer_radius * Math::sin(angle), 0.0f), p_transform, p_particles), _transform_point(Vector3(inner_radius * Math::cos(angle), inner_radius * Math::sin(angle), 0.0f), p_transform, p_particles));
		_append_line(lines, _transform_point(Vector3(outer_radius * Math::cos(angle), 0.0f, outer_radius * Math::sin(angle)), p_transform, p_particles), _transform_point(Vector3(inner_radius * Math::cos(angle), 0.0f, inner_radius * Math::sin(angle)), p_transform, p_particles));
		_append_line(lines, _transform_point(Vector3(0.0f, outer_radius * Math::cos(angle), outer_radius * Math::sin(angle)), p_transform, p_particles), _transform_point(Vector3(0.0f, inner_radius * Math::cos(angle), inner_radius * Math::sin(angle)), p_transform, p_particles));
		}
	}

	if (arc_degrees < 360) {
		for (int radius_index = 0; radius_index < shell_count; radius_index++) {
			const float radius = radius_index == 1 ? inner_radius : outer_radius;
			const float end_angle = Math::deg_to_rad((float)arc_degrees);
			_append_line(lines, _transform_point(Vector3(radius, 0.0f, 0.0f), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(radius * Math::cos(end_angle), radius * Math::sin(end_angle), 0.0f), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(radius, 0.0f, 0.0f), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(radius * Math::cos(end_angle), 0.0f, radius * Math::sin(end_angle)), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(0.0f, radius, 0.0f), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(0.0f, radius * Math::cos(end_angle), radius * Math::sin(end_angle)), p_transform, p_particles), _transform_point(Vector3(), p_transform, p_particles));
		}
	}

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_draw_box(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const Vector3 extents = p_particles->get_box_extents();

	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, -extents.z), p_transform, p_particles));

	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, -extents.z), p_transform, p_particles));

	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, -extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(extents.x, extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, -extents.y, extents.z), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-extents.x, extents.y, extents.z), p_transform, p_particles));

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_draw_circle(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const float outer_radius = p_particles->get_radius();
	const float inner_radius = p_particles->get_radius() * (1.0f - p_particles->get_radius_thickness());
	const int arc_degrees = _get_arc_degrees(p_particles);
	const bool draw_inner_shell = _should_draw_inner_shell(outer_radius, inner_radius);
	const int shell_count = draw_inner_shell ? 2 : 1;

	for (int i = 0; i <= arc_degrees; i++) {
		const float angle_a = Math::deg_to_rad((float)i);
		const float angle_b = Math::deg_to_rad((float)MIN(i + 1, arc_degrees));
		_append_line(lines, _transform_point(Vector3(outer_radius * Math::cos(angle_a), 0.0f, outer_radius * Math::sin(angle_a)), p_transform, p_particles), _transform_point(Vector3(outer_radius * Math::cos(angle_b), 0.0f, outer_radius * Math::sin(angle_b)), p_transform, p_particles));
		if (draw_inner_shell) {
			_append_line(lines, _transform_point(Vector3(inner_radius * Math::cos(angle_a), 0.0f, inner_radius * Math::sin(angle_a)), p_transform, p_particles), _transform_point(Vector3(inner_radius * Math::cos(angle_b), 0.0f, inner_radius * Math::sin(angle_b)), p_transform, p_particles));
		}
	}

	if (draw_inner_shell) {
		for (int i = 0; i <= arc_degrees; i += 45) {
		const float angle = Math::deg_to_rad((float)MIN(i, arc_degrees));
		_append_line(lines, _transform_point(Vector3(inner_radius * Math::cos(angle), 0.0f, inner_radius * Math::sin(angle)), p_transform, p_particles), _transform_point(Vector3(outer_radius * Math::cos(angle), 0.0f, outer_radius * Math::sin(angle)), p_transform, p_particles));
		}
	}

	if (arc_degrees < 360) {
		const float end_angle = Math::deg_to_rad((float)arc_degrees);
		for (int radius_index = 0; radius_index < shell_count; radius_index++) {
			const float radius = radius_index == 1 ? inner_radius : outer_radius;
			_append_line(lines, _transform_point(Vector3(), p_transform, p_particles), _transform_point(Vector3(radius, 0.0f, 0.0f), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(), p_transform, p_particles), _transform_point(Vector3(radius * Math::cos(end_angle), 0.0f, radius * Math::sin(end_angle)), p_transform, p_particles));
		}
	}

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_draw_edge(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const float length = p_particles->get_radius();
	const float cross_size = length * 0.1f;

	lines.push_back(_transform_point(Vector3(0.0f, -length, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, length, 0.0f), p_transform, p_particles));

	lines.push_back(_transform_point(Vector3(-cross_size, -length, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(cross_size, -length, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, -length, -cross_size), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, -length, cross_size), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-cross_size, length, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(cross_size, length, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, length, -cross_size), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, length, cross_size), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(-cross_size, 0.0f, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(cross_size, 0.0f, 0.0f), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, 0.0f, -cross_size), p_transform, p_particles));
	lines.push_back(_transform_point(Vector3(0.0f, 0.0f, cross_size), p_transform, p_particles));

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_draw_hemisphere(EditorNode3DGizmo *p_gizmo, const YParticles3D *p_particles, const Transform3D &p_transform) {
	PackedVector3Array lines;
	const float outer_radius = p_particles->get_radius();
	const float inner_radius = p_particles->get_radius() * (1.0f - p_particles->get_radius_thickness());
	const int arc_degrees = _get_arc_degrees(p_particles);
	const bool draw_inner_shell = _should_draw_inner_shell(outer_radius, inner_radius);
	const int shell_count = draw_inner_shell ? 2 : 1;

	for (int radius_index = 0; radius_index < shell_count; radius_index++) {
		const float radius = radius_index == 1 ? inner_radius : outer_radius;
		for (int height_step = 0; height_step <= 90; height_step += 15) {
			const float height_angle = Math::deg_to_rad((float)height_step);
			const float circle_radius = radius * Math::sin(height_angle);
			const float y = radius * Math::cos(height_angle);
			for (int i = 0; i <= arc_degrees; i++) {
				const float angle_a = Math::deg_to_rad((float)i);
				const float angle_b = Math::deg_to_rad((float)MIN(i + 1, arc_degrees));
				const Vector3 point_a(circle_radius * Math::cos(angle_a), y, circle_radius * Math::sin(angle_a));
				const Vector3 point_b(circle_radius * Math::cos(angle_b), y, circle_radius * Math::sin(angle_b));
				_append_line(lines, _transform_point(point_a, p_transform, p_particles), _transform_point(point_b, p_transform, p_particles));
			}
		}

		for (int i = 0; i <= arc_degrees; i += 45) {
			const float angle = Math::deg_to_rad((float)MIN(i, arc_degrees));
			const int steps = 16;
			for (int step = 0; step < steps; step++) {
				const float t1 = (float)step / (float)steps;
				const float t2 = (float)(step + 1) / (float)steps;
				const float height_angle_1 = t1 * (float)Math_PI * 0.5f;
				const float height_angle_2 = t2 * (float)Math_PI * 0.5f;
				const Vector3 point_a(radius * Math::sin(height_angle_1) * Math::cos(angle), radius * Math::cos(height_angle_1), radius * Math::sin(height_angle_1) * Math::sin(angle));
				const Vector3 point_b(radius * Math::sin(height_angle_2) * Math::cos(angle), radius * Math::cos(height_angle_2), radius * Math::sin(height_angle_2) * Math::sin(angle));
				_append_line(lines, _transform_point(point_a, p_transform, p_particles), _transform_point(point_b, p_transform, p_particles));
			}
		}
	}

	for (int radius_index = 0; radius_index < shell_count; radius_index++) {
		const float radius = radius_index == 1 ? inner_radius : outer_radius;
		for (int i = 0; i <= arc_degrees; i++) {
			const float angle_a = Math::deg_to_rad((float)i);
			const float angle_b = Math::deg_to_rad((float)MIN(i + 1, arc_degrees));
			_append_line(lines, _transform_point(Vector3(radius * Math::cos(angle_a), 0.0f, radius * Math::sin(angle_a)), p_transform, p_particles), _transform_point(Vector3(radius * Math::cos(angle_b), 0.0f, radius * Math::sin(angle_b)), p_transform, p_particles));
		}
	}

	if (arc_degrees < 360) {
		const float end_angle = Math::deg_to_rad((float)arc_degrees);
		for (int radius_index = 0; radius_index < shell_count; radius_index++) {
			const float radius = radius_index == 1 ? inner_radius : outer_radius;
			_append_line(lines, _transform_point(Vector3(), p_transform, p_particles), _transform_point(Vector3(radius, 0.0f, 0.0f), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(), p_transform, p_particles), _transform_point(Vector3(radius * Math::cos(end_angle), 0.0f, radius * Math::sin(end_angle)), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(0.0f, radius, 0.0f), p_transform, p_particles), _transform_point(Vector3(radius, 0.0f, 0.0f), p_transform, p_particles));
			_append_line(lines, _transform_point(Vector3(0.0f, radius, 0.0f), p_transform, p_particles), _transform_point(Vector3(radius * Math::cos(end_angle), 0.0f, radius * Math::sin(end_angle)), p_transform, p_particles));
		}
	}

	p_gizmo->add_lines(lines, get_material("main", p_gizmo), false);
}

void YParticlesGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &p_gizmo_ref) {
	EditorNode3DGizmo *p_gizmo = p_gizmo_ref.ptr();
	p_gizmo->clear();

	if (!materials_ready) {
		create_material("main", Color(0.1f, 0.32f, 0.64f));
		materials_ready = true;
	}

	YParticles3D *particles = Object::cast_to<YParticles3D>(p_gizmo->get_node_3d());
	if (particles == nullptr || !particles->get_enable_shape()) {
		return;
	}

	Transform3D gizmo_transform;
	Vector3 final_offset = particles->get_position_offset();
	if (!particles->is_using_world_space()) {
		final_offset = particles->get_global_transform().basis.inverse().xform(final_offset);
	}
	gizmo_transform.origin = final_offset;

	Basis basis;
	const Vector3 rotation_offset = particles->get_rotation_offset();
	if (particles->is_direction_in_world_space()) {
		basis = basis.rotated(Vector3(1.0f, 0.0f, 0.0f), Math::deg_to_rad(rotation_offset.x));
		basis = basis.rotated(Vector3(0.0f, 1.0f, 0.0f), Math::deg_to_rad(rotation_offset.y));
		basis = basis.rotated(Vector3(0.0f, 0.0f, -1.0f), Math::deg_to_rad(rotation_offset.z));
	} else {
		basis = basis.rotated(Vector3(0.0f, 0.0f, -1.0f), Math::deg_to_rad(rotation_offset.z));
		basis = basis.rotated(Vector3(0.0f, 1.0f, 0.0f), Math::deg_to_rad(rotation_offset.y));
		basis = basis.rotated(Vector3(1.0f, 0.0f, 0.0f), Math::deg_to_rad(rotation_offset.x));
	}
	gizmo_transform.basis = basis;

	switch (particles->get_shape_type()) {
		case YParticles3D::EMISSION_SHAPE_CONE:
			_draw_cone(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_SPHERE:
			_draw_sphere(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_BOX:
			_draw_box(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_CIRCLE:
			_draw_circle(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_EDGE:
			_draw_edge(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_HEMISPHERE:
			_draw_hemisphere(p_gizmo, particles, gizmo_transform);
			break;
		case YParticles3D::EMISSION_SHAPE_MESH:
			break;
	}
}

#endif // TOOLS_ENABLED
