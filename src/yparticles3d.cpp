#include "yparticles3d.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

static void _yparticles3d_notify_inspector_if_editor(YParticles3D *p_particles) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && p_particles != nullptr) {
		p_particles->notify_property_list_changed();
	}
#else
	(void)p_particles;
#endif
}

static void _yparticles3d_update_gizmos_if_editor(YParticles3D *p_particles) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && p_particles != nullptr) {
		p_particles->update_gizmos();
	}
#else
	(void)p_particles;
#endif
}

static String _yparticles3d_get_shader_setting_name(YParticles3D::BlendMode p_blend_mode, YParticles3D::SamplingFilter p_sampling_filter) {
	const bool nearest = p_sampling_filter == YParticles3D::SAMPLING_FILTER_NEAREST;
	switch (p_blend_mode) {
		case YParticles3D::BLEND_MODE_ADD:
			return nearest ? "yengine/yparticles3d/shaders/add_nearest" : "yengine/yparticles3d/shaders/add";
		case YParticles3D::BLEND_MODE_SUBTRACT:
			return nearest ? "yengine/yparticles3d/shaders/subtract_nearest" : "yengine/yparticles3d/shaders/subtract";
		case YParticles3D::BLEND_MODE_MULTIPLY:
			return nearest ? "yengine/yparticles3d/shaders/multiply_nearest" : "yengine/yparticles3d/shaders/multiply";
		case YParticles3D::BLEND_MODE_PREMULTIPLIED_ALPHA:
			return nearest ? "yengine/yparticles3d/shaders/premultiplied_alpha_nearest" : "yengine/yparticles3d/shaders/premultiplied_alpha";
		case YParticles3D::BLEND_MODE_MIX:
		default:
			return nearest ? "yengine/yparticles3d/shaders/mix_nearest" : "yengine/yparticles3d/shaders/mix";
	}
}

static String _yparticles3d_get_trail_shader_setting_name(YParticles3D::BlendMode p_blend_mode, YParticles3D::SamplingFilter p_sampling_filter) {
	const bool nearest = p_sampling_filter == YParticles3D::SAMPLING_FILTER_NEAREST;
	switch (p_blend_mode) {
		case YParticles3D::BLEND_MODE_ADD:
			return nearest ? "yengine/yparticles3d/trail_shaders/add_nearest" : "yengine/yparticles3d/trail_shaders/add";
		case YParticles3D::BLEND_MODE_SUBTRACT:
			return nearest ? "yengine/yparticles3d/trail_shaders/subtract_nearest" : "yengine/yparticles3d/trail_shaders/subtract";
		case YParticles3D::BLEND_MODE_MULTIPLY:
			return nearest ? "yengine/yparticles3d/trail_shaders/multiply_nearest" : "yengine/yparticles3d/trail_shaders/multiply";
		case YParticles3D::BLEND_MODE_PREMULTIPLIED_ALPHA:
			return nearest ? "yengine/yparticles3d/trail_shaders/premultiplied_alpha_nearest" : "yengine/yparticles3d/trail_shaders/premultiplied_alpha";
		case YParticles3D::BLEND_MODE_MIX:
		default:
			return nearest ? "yengine/yparticles3d/trail_shaders/mix_nearest" : "yengine/yparticles3d/trail_shaders/mix";
	}
}

void YParticles3D::BurstInstance::initialize(RandomNumberGenerator *p_rng) {
	remaining_cycles = p_rng->randi_range(min_cycles, max_cycles);
	next_cycle_time = time;
	reset_burst(p_rng);
}

void YParticles3D::BurstInstance::reset_burst(RandomNumberGenerator *p_rng) {
	particles_in_burst = p_rng->randi_range(min_particles, max_particles);
	current_burst_index = 0;
}

int YParticles3D::BurstInstance::process(float p_current_time, RandomNumberGenerator *p_rng, int &r_start_index, int &r_total_in_burst) {
	if (remaining_cycles <= 0 || p_current_time < next_cycle_time) {
		return 0;
	}

	if (p_rng->randf() > probability) {
		remaining_cycles--;
		if (remaining_cycles > 0) {
			next_cycle_time += particle_interval;
		}
		return 0;
	}

	r_start_index = current_burst_index;
	r_total_in_burst = particles_in_burst;
	const int to_emit = particles_in_burst;
	current_burst_index = to_emit;

	remaining_cycles--;
	if (remaining_cycles > 0) {
		next_cycle_time += particle_interval;
		reset_burst(p_rng);
	}

	return to_emit;
}

float YParticles3D::_sample_curve(const Ref<Curve> &p_curve, float p_t, float p_default) const {
	if (p_curve.is_null()) {
		return p_default;
	}
	return p_curve->sample(CLAMP(p_t, 0.0f, 1.0f));
}

float YParticles3D::_sample_emission_rate() const {
	if (rate_over_time_mode == 1) {
		const float normalized_time = duration > 0.0f ? CLAMP(_emission_time / duration, 0.0f, 1.0f) : 0.0f;
		return MAX(0.0f, _sample_curve(rate_over_time_curve, normalized_time, rate_over_time));
	}
	return MAX(0.0f, rate_over_time);
}

float YParticles3D::_pick_start_lifetime() const {
	switch (start_lifetime_mode) {
		case 1:
			return _rng->randf_range(start_lifetime_random.x, start_lifetime_random.y);
		case 2:
			return _sample_curve(start_lifetime_curve, _rng->randf(), start_lifetime_constant);
		default:
			return start_lifetime_constant;
	}
}

float YParticles3D::_pick_start_speed() const {
	switch (start_speed_mode) {
		case 1:
			return _rng->randf_range(start_speed_random.x, start_speed_random.y);
		default:
			return start_speed_constant;
	}
}

Vector3 YParticles3D::_pick_start_size(const Particle &p_particle) const {
	switch (start_size_mode) {
		case 1:
			if (Math::is_equal_approx(start_size_random.x, start_size_random.y) && Math::is_equal_approx(start_size_random.z, start_size_random.w)) {
				const float s = _rng->randf_range(start_size_random.x, start_size_random.z);
				return Vector3(s, s, s);
			}
			return Vector3(_rng->randf_range(start_size_random.x, start_size_random.z), _rng->randf_range(start_size_random.y, start_size_random.w), _rng->randf_range(start_size_random.x, start_size_random.z));
		case 2: {
			const float s = _sample_curve(start_size_curve, _rng->randf(), start_size_constant.x);
			return Vector3(s, s, s);
		}
		case 4: {
			const float min_size = _sample_curve(start_size_curve_min, p_particle.random_a, start_size_constant.x);
			const float max_size = _sample_curve(start_size_curve_max, p_particle.random_a, start_size_constant.x);
			const float s = Math::lerp(min_size, max_size, p_particle.random_b);
			return Vector3(s, s, s);
		}
		case 3: {
			const float s = _rng->randf_range(start_size_square_random.x, start_size_square_random.y);
			return Vector3(s, s, s);
		}
		case 5:
			return start_size_constant_3d;
		case 6:
			return Vector3(
					_rng->randf_range(start_size_random_min_3d.x, start_size_random_max_3d.x),
					_rng->randf_range(start_size_random_min_3d.y, start_size_random_max_3d.y),
					_rng->randf_range(start_size_random_min_3d.z, start_size_random_max_3d.z));
		case 7:
			return Vector3(
					_sample_curve(start_size_x_curve, _rng->randf(), start_size_constant_3d.x),
					_sample_curve(start_size_y_curve, _rng->randf(), start_size_constant_3d.y),
					_sample_curve(start_size_z_curve, _rng->randf(), start_size_constant_3d.z));
		case 8:
			return Vector3(
					Math::lerp(_sample_curve(start_size_x_curve_min, p_particle.random_a, start_size_constant_3d.x), _sample_curve(start_size_x_curve, p_particle.random_a, start_size_constant_3d.x), p_particle.random_b),
					Math::lerp(_sample_curve(start_size_y_curve_min, p_particle.random_a, start_size_constant_3d.y), _sample_curve(start_size_y_curve, p_particle.random_a, start_size_constant_3d.y), p_particle.random_b),
					Math::lerp(_sample_curve(start_size_z_curve_min, p_particle.random_a, start_size_constant_3d.z), _sample_curve(start_size_z_curve, p_particle.random_a, start_size_constant_3d.z), p_particle.random_b));
		default:
			if (start_size_constant.y == 0.0f) {
				return Vector3(start_size_constant.x, start_size_constant.x, start_size_constant.x);
			}
			return Vector3(start_size_constant.x, start_size_constant.y, start_size_constant.x);
		}
}

Vector3 YParticles3D::_pick_start_rotation_degrees(const Particle &p_particle) const {
	switch (start_rotation_degrees_mode) {
		case 1:
			return Vector3(0.0f, 0.0f, _rng->randf_range(start_rotation_degrees_random.x, start_rotation_degrees_random.y));
		case 2:
			return Vector3(0.0f, 0.0f, _sample_curve(start_rotation_degrees_curve, _rng->randf(), start_rotation_degrees_constant));
		case 3:
			return start_rotation_degrees_constant_3d;
		case 4:
			return Vector3(
					_rng->randf_range(start_rotation_degrees_random_min_3d.x, start_rotation_degrees_random_max_3d.x),
					_rng->randf_range(start_rotation_degrees_random_min_3d.y, start_rotation_degrees_random_max_3d.y),
					_rng->randf_range(start_rotation_degrees_random_min_3d.z, start_rotation_degrees_random_max_3d.z));
		case 5:
			return Vector3(
					_sample_curve(start_rotation_degrees_x_curve, p_particle.random_a, start_rotation_degrees_constant_3d.x),
					_sample_curve(start_rotation_degrees_y_curve, p_particle.random_a, start_rotation_degrees_constant_3d.y),
					_sample_curve(start_rotation_degrees_z_curve, p_particle.random_a, start_rotation_degrees_constant_3d.z));
		default:
			return Vector3(0.0f, 0.0f, start_rotation_degrees_constant);
	}
}

Color YParticles3D::_sample_particle_color(const Particle &p_particle, float p_normalized) const {
	Color color = tint_color;

	if (use_start_color_gradient && start_color_gradient.is_valid() && start_color_gradient->get_gradient().is_valid()) {
		Color sampled_start = start_color_gradient->get_gradient()->sample(CLAMP(p_particle.random_a, 0.0f, 1.0f));
		if (start_color_use_two_gradients && start_color_gradient_secondary.is_valid() && start_color_gradient_secondary->get_gradient().is_valid()) {
			const Color sampled_secondary = start_color_gradient_secondary->get_gradient()->sample(CLAMP(p_particle.random_a, 0.0f, 1.0f));
			sampled_start = sampled_start.lerp(sampled_secondary, p_particle.random_b);
		}
		color *= Color(sampled_start.r, sampled_start.g, sampled_start.b, 1.0f);
	}

	if (enable_color_over_lifetime && color_over_lifetime.is_valid() && color_over_lifetime->get_gradient().is_valid()) {
		Color sampled_color = color_over_lifetime->get_gradient()->sample(CLAMP(p_normalized, 0.0f, 1.0f));
		if (color_over_lifetime_use_two_gradients && color_over_lifetime_secondary.is_valid() && color_over_lifetime_secondary->get_gradient().is_valid()) {
			const Color secondary = color_over_lifetime_secondary->get_gradient()->sample(CLAMP(p_normalized, 0.0f, 1.0f));
			sampled_color = sampled_color.lerp(secondary, p_particle.random_b);
		}
		color *= Color(sampled_color.r, sampled_color.g, sampled_color.b, 1.0f);
	}

	if (!Math::is_zero_approx(p_particle.hue_offset)) {
		color = color.from_hsv(Math::fposmod(color.get_h() + p_particle.hue_offset, 1.0f), color.get_s(), color.get_v(), color.a);
	}

	return color;
}

float YParticles3D::_sample_alpha_over_lifetime(const Particle &p_particle, float p_normalized) const {
	float alpha = p_particle.start_alpha;
	if (alpha_over_lifetime.is_valid()) {
		float sampled_alpha = alpha_over_lifetime->sample(CLAMP(p_normalized, 0.0f, 1.0f));
		if (color_over_lifetime_use_two_gradients && alpha_over_lifetime_secondary.is_valid()) {
			const float secondary_alpha = alpha_over_lifetime_secondary->sample(CLAMP(p_normalized, 0.0f, 1.0f));
			sampled_alpha = Math::lerp(sampled_alpha, secondary_alpha, p_particle.random_b);
		}
		alpha *= sampled_alpha;
	}
	return CLAMP(alpha, 0.0f, 1.0f);
}

float YParticles3D::_sample_inherit_velocity_multiplier(float p_normalized) const {
	if (inherit_velocity_mode == 1 || inherit_velocity_mode == 3) {
		return inherit_velocity_curve.is_valid() ? inherit_velocity_curve->sample(CLAMP(p_normalized, 0.0f, 1.0f)) : inherit_velocity_multiplier;
	}
	return inherit_velocity_multiplier;
}

Dictionary YParticles3D::_make_default_sub_emitter_entry() const {
	Dictionary entry;
	entry["path"] = NodePath();
	entry["event"] = (int)SUB_EMITTER_CONDITION_BIRTH;
	entry["probability"] = 1.0f;
	entry["inherit"] = (int)SUB_EMITTER_INHERIT_NOTHING;
	return entry;
}

int YParticles3D::_normalize_sub_emitter_inherit_flags(int p_flags) const {
	if (p_flags & SUB_EMITTER_INHERIT_EVERYTHING) {
		return SUB_EMITTER_INHERIT_EVERYTHING;
	}
	return p_flags & SUB_EMITTER_INHERIT_EVERYTHING;
}

Vector3 YParticles3D::_get_random_unit_vector() const {
	const float phi = _rng->randf() * (float)Math_TAU;
	const float costheta = _rng->randf_range(-1.0f, 1.0f);
	const float theta = Math::acos(costheta);
	return Vector3(Math::sin(theta) * Math::cos(phi), Math::cos(theta), Math::sin(theta) * Math::sin(phi)).normalized();
}

float YParticles3D::_next_arc_angle(float p_burst_spot) {
	const float arc_radians = Math::deg_to_rad(arc_degrees);

	switch (arc_mode) {
		case ARC_MODE_LOOP: {
			const float arc_speed = arc_speed_mode == 1 ? _sample_curve(arc_speed_curve, duration > 0.0f ? _emission_time / duration : 0.0f, arc_speed_constant) : arc_speed_constant;
			_current_arc_rotation = Math::fposmod(_current_arc_rotation + arc_speed * arc_radians, MAX(arc_degrees, 0.001f));
			if (arc_spread > 0.0f) {
				return Math::deg_to_rad(Math::floor(_current_arc_rotation * arc_spread) / arc_spread);
			}
			return Math::deg_to_rad(_current_arc_rotation);
		}
		case ARC_MODE_PING_PONG: {
			const float arc_speed = arc_speed_mode == 1 ? _sample_curve(arc_speed_curve, duration > 0.0f ? _emission_time / duration : 0.0f, arc_speed_constant) : arc_speed_constant;
			_current_arc_rotation += (arc_speed * arc_radians) * (float)_arc_direction;
			if (_current_arc_rotation >= arc_degrees) {
				_current_arc_rotation = arc_degrees;
				_arc_direction = -1;
			} else if (_current_arc_rotation <= 0.0f) {
				_current_arc_rotation = 0.0f;
				_arc_direction = 1;
			}
			if (arc_spread > 0.0f) {
				return Math::deg_to_rad(Math::floor(_current_arc_rotation * arc_spread) / arc_spread);
			}
			return Math::deg_to_rad(_current_arc_rotation);
		}
		case ARC_MODE_BURST_SPREAD:
			if (arc_spread > 0.0f) {
				const int segments = MAX(1, (int)Math::floor(arc_degrees * arc_spread));
				const int segment_index = MIN(segments - 1, (int)Math::floor(p_burst_spot * segments));
				return Math::deg_to_rad((float)segment_index / arc_spread);
			}
			return arc_radians * p_burst_spot;
		case ARC_MODE_RANDOM:
		default:
			return _rng->randf() * arc_radians;
	}
}

Vector3 YParticles3D::_sample_velocity_over_lifetime(const Particle &p_particle, const Vector3 &p_base_velocity, const Vector3 &p_fallback_direction, float p_normalized) const {
	if (!enable_velocity_over_lifetime) {
		return p_base_velocity;
	}

	const float sample_time = play_in_reverse ? (1.0f - p_normalized) : p_normalized;
	const float clamped_time = CLAMP(sample_time, 0.0f, 1.0f);
	const bool has_base_velocity = p_base_velocity.length_squared() > 0.000001f;
	Vector3 fallback_direction = p_fallback_direction;
	if (fallback_direction.length_squared() <= 0.000001f) {
		fallback_direction = Vector3(0.0f, 1.0f, 0.0f);
	} else {
		fallback_direction.normalize();
	}

	if (velocity_over_lifetime_mode == 1) {
		const Vector3 sampled_velocity(
				velocity_over_lifetime_use_two_curves ? Math::lerp(
						velocity_over_lifetime_x_min.is_valid() ? velocity_over_lifetime_x_min->sample(clamped_time) : 0.0f,
						velocity_over_lifetime_x.is_valid() ? velocity_over_lifetime_x->sample(clamped_time) : 0.0f,
						p_particle.random_b) :
						(velocity_over_lifetime_x.is_valid() ? velocity_over_lifetime_x->sample(clamped_time) : 1.0f),
				velocity_over_lifetime_use_two_curves ? Math::lerp(
						velocity_over_lifetime_y_min.is_valid() ? velocity_over_lifetime_y_min->sample(clamped_time) : 0.0f,
						velocity_over_lifetime_y.is_valid() ? velocity_over_lifetime_y->sample(clamped_time) : 0.0f,
						p_particle.random_b) :
						(velocity_over_lifetime_y.is_valid() ? velocity_over_lifetime_y->sample(clamped_time) : 1.0f),
				velocity_over_lifetime_use_two_curves ? Math::lerp(
						velocity_over_lifetime_z_min.is_valid() ? velocity_over_lifetime_z_min->sample(clamped_time) : 0.0f,
						velocity_over_lifetime_z.is_valid() ? velocity_over_lifetime_z->sample(clamped_time) : 0.0f,
						p_particle.random_b) :
						(velocity_over_lifetime_z.is_valid() ? velocity_over_lifetime_z->sample(clamped_time) : 1.0f));
		if (!has_base_velocity) {
			return sampled_velocity;
		}
		Vector3 result = p_base_velocity;
		result.x *= sampled_velocity.x;
		result.y *= sampled_velocity.y;
		result.z *= sampled_velocity.z;
		return result;
	}

	if (velocity_over_lifetime.is_valid()) {
		float sampled_speed = velocity_over_lifetime->sample(clamped_time);
		if (velocity_over_lifetime_use_two_curves && velocity_over_lifetime_min.is_valid()) {
			sampled_speed = Math::lerp(velocity_over_lifetime_min->sample(clamped_time), sampled_speed, p_particle.random_b);
		}
		if (!has_base_velocity) {
			return fallback_direction * sampled_speed;
		}
		return p_base_velocity * sampled_speed;
	}

	return p_base_velocity;
}

Vector3 YParticles3D::_sample_force_over_lifetime(const Particle &p_particle, float p_normalized, const Basis &p_global_basis) const {
	if (!enable_force_over_lifetime) {
		return Vector3();
	}

	const float sample_time = play_in_reverse ? (1.0f - p_normalized) : p_normalized;
	const float clamped_time = CLAMP(sample_time, 0.0f, 1.0f);
	Vector3 force;
	if (force_over_lifetime_mode == 1) {
		force = Vector3(
				force_over_lifetime_x.is_valid() ? force_over_lifetime_x->sample(clamped_time) : 0.0f,
				force_over_lifetime_y.is_valid() ? force_over_lifetime_y->sample(clamped_time) : 0.0f,
				force_over_lifetime_z.is_valid() ? force_over_lifetime_z->sample(clamped_time) : 0.0f);
	} else if (force_over_lifetime_mode == 2) {
		force = force_over_lifetime_constant;
	} else if (force_over_lifetime_mode == 3) {
		force = Vector3(
				Math::lerp(force_over_lifetime_random_min.x, force_over_lifetime_random_max.x, p_particle.random_a),
				Math::lerp(force_over_lifetime_random_min.y, force_over_lifetime_random_max.y, p_particle.random_b),
				Math::lerp(force_over_lifetime_random_min.z, force_over_lifetime_random_max.z, Math::fposmod(p_particle.random_a + p_particle.random_b, 1.0f)));
	} else {
		const float sampled_force = force_over_lifetime.is_valid() ? force_over_lifetime->sample(clamped_time) : 0.0f;
		force = p_particle.direction.normalized() * sampled_force;
	}

	if (!force_in_world_space) {
		force = p_global_basis.xform(force);
	}
	return force;
}

Vector3 YParticles3D::_sample_limit_velocity_over_lifetime(const Vector3 &p_velocity, float p_normalized) const {
	if (!enable_limit_velocity_over_lifetime) {
		return p_velocity;
	}

	const float sample_time = play_in_reverse ? (1.0f - p_normalized) : p_normalized;
	const float clamped_time = CLAMP(sample_time, 0.0f, 1.0f);
	const float dampen = CLAMP(limit_velocity_over_lifetime_dampen, 0.0f, 1.0f);

	if (limit_velocity_over_lifetime_speed_mode >= 2) {
		Vector3 result = p_velocity;
		Vector3 limits;
		if (limit_velocity_over_lifetime_speed_mode == 3) {
			limits = Vector3(
					MAX(0.0f, _sample_curve(limit_velocity_over_lifetime_speed_x_curve, clamped_time, limit_velocity_over_lifetime_speed_axis.x)),
					MAX(0.0f, _sample_curve(limit_velocity_over_lifetime_speed_y_curve, clamped_time, limit_velocity_over_lifetime_speed_axis.y)),
					MAX(0.0f, _sample_curve(limit_velocity_over_lifetime_speed_z_curve, clamped_time, limit_velocity_over_lifetime_speed_axis.z)));
		} else {
			limits = Vector3(
					MAX(0.0f, limit_velocity_over_lifetime_speed_axis.x),
					MAX(0.0f, limit_velocity_over_lifetime_speed_axis.y),
					MAX(0.0f, limit_velocity_over_lifetime_speed_axis.z));
		}

		for (int axis = 0; axis < 3; axis++) {
			const float value = result[axis];
			const float abs_value = Math::abs(value);
			const float limit = limits[axis];
			if (abs_value > limit) {
				const float exceeded = abs_value - limit;
				result[axis] = SIGN(value) * (limit + exceeded * (1.0f - dampen));
			}
		}

		return result;
	}

	const float limit = limit_velocity_over_lifetime_speed_mode == 1 ?
			_sample_curve(limit_velocity_over_lifetime_speed_curve, clamped_time, limit_velocity_over_lifetime_speed) :
			limit_velocity_over_lifetime_speed;
	const float safe_limit = MAX(0.0f, limit);
	const float speed = p_velocity.length();
	if (speed <= safe_limit || speed <= 0.000001f) {
		return p_velocity;
	}

	const float exceeded = speed - safe_limit;
	const float adjusted_speed = safe_limit + exceeded * (1.0f - dampen);
	return p_velocity * (adjusted_speed / speed);
}

Vector3 YParticles3D::_sample_noise_strength(float p_normalized) const {
	if (!enable_noise) {
		return Vector3();
	}

	switch (noise_strength_mode) {
		case 1: {
			const float value = noise_strength_curve.is_valid() ? noise_strength_curve->sample(CLAMP(p_normalized, 0.0f, 1.0f)) : 0.0f;
			return Vector3(value, value, value);
		}
		case 2:
			return Vector3(
					noise_strength_x.is_valid() ? noise_strength_x->sample(CLAMP(p_normalized, 0.0f, 1.0f)) : 0.0f,
					noise_strength_y.is_valid() ? noise_strength_y->sample(CLAMP(p_normalized, 0.0f, 1.0f)) : 0.0f,
					noise_strength_z.is_valid() ? noise_strength_z->sample(CLAMP(p_normalized, 0.0f, 1.0f)) : 0.0f);
		case 0:
		default:
			return Vector3(noise_strength, noise_strength, noise_strength);
	}
}

float YParticles3D::_sample_fbm_noise(const Vector3 &p_position, const Vector3 &p_offset) const {
	if (_noise_generator.is_null() || !enable_noise) {
		return 0.0f;
	}

	const float safe_scale = MAX(noise_scale, 0.001f);
	Vector3 sample_pos = (p_position + (noise_scroll_speed * _time) + p_offset) / safe_scale;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	float total = 0.0f;
	float amplitude_sum = 0.0f;
	const int octave_count = MAX(1, noise_octaves);
	const float lacunarity = MAX(noise_lacunarity, 1.0f);

	for (int i = 0; i < octave_count; i++) {
		total += amplitude * _noise_generator->get_noise_3d(sample_pos.x * frequency, sample_pos.y * frequency, sample_pos.z * frequency);
		amplitude_sum += amplitude;
		amplitude *= 0.5f;
		frequency *= lacunarity;
	}

	return amplitude_sum > 0.0f ? (total / amplitude_sum) : 0.0f;
}

Vector3 YParticles3D::_sample_noise_velocity(const Vector3 &p_position) const {
	if (!enable_noise) {
		return Vector3();
	}

	const Vector3 sampled_noise = Vector3(
			_sample_fbm_noise(p_position, Vector3(17.13f, 43.71f, 91.07f)),
			_sample_fbm_noise(p_position, Vector3(59.23f, 11.89f, 137.17f)),
			_sample_fbm_noise(p_position, Vector3(101.41f, 73.37f, 29.53f)));
	return sampled_noise;
}

Vector3 YParticles3D::_get_attraction_target_position() const {
	if (attraction_target_mode == ATTRACTION_TARGET_MODE_NODE3D && !attraction_target.is_empty()) {
		const Node3D *attractor_node = Object::cast_to<Node3D>(get_node_or_null(attraction_target));
		if (attractor_node != nullptr) {
			return attractor_node->get_global_position();
		}
	}

	return attractor_position;
}

Vector3 YParticles3D::_sample_particle_scale(const Particle &p_particle, float p_normalized) const {
	Vector3 scale = p_particle.scale;
	if (enable_size_over_lifetime) {
		if (width_over_lifetime.is_valid()) {
			scale.x *= width_over_lifetime->sample(p_normalized);
		}
		if (height_over_lifetime.is_valid()) {
			scale.y *= height_over_lifetime->sample(p_normalized);
		}
		if (depth_over_lifetime.is_valid()) {
			scale.z *= depth_over_lifetime->sample(p_normalized);
		}
		if (size_over_lifetime.is_valid()) {
			float s = size_over_lifetime->sample(p_normalized);
			if (size_over_lifetime_use_two_curves && size_over_lifetime_min.is_valid()) {
				s = Math::lerp(size_over_lifetime_min->sample(p_normalized), s, p_particle.random_b);
			}
			scale *= s;
		}
	}
	if (!Math::is_zero_approx(noise_size_amount) && enable_noise) {
		const Vector3 raw_noise = _sample_noise_velocity(p_particle.position);
		const float noise_scalar = (raw_noise.x + raw_noise.y + raw_noise.z) / 3.0f;
		const Vector3 noise_strength_sample = _sample_noise_strength(p_normalized);
		const float noise_strength_scalar = (noise_strength_sample.x + noise_strength_sample.y + noise_strength_sample.z) / 3.0f;
		const float size_multiplier = MAX(0.01f, 1.0f + (noise_scalar * noise_strength_scalar * noise_size_amount));
		scale *= size_multiplier;
	}
	return scale;
}

float YParticles3D::_sample_trail_lifetime(float p_normalized) const {
	if (trail_lifetime_mode == 1) {
		return MAX(0.0f, _sample_curve(trail_lifetime_curve, p_normalized, trail_lifetime));
	}
	return MAX(0.0f, trail_lifetime);
}

int YParticles3D::_get_collision_query_budget() const {
	switch (collision_quality) {
		case COLLISION_QUALITY_LOW:
			return 32;
		case COLLISION_QUALITY_MEDIUM:
			return 128;
		case COLLISION_QUALITY_HIGH:
		default:
			return 512;
	}
}

int64_t YParticles3D::_get_collision_voxel_key(const Vector3 &p_position) const {
	const float voxel = MAX(collision_voxel_size, 0.001f);
	const Vector3i cell(
			(int)Math::floor(p_position.x / voxel),
			(int)Math::floor(p_position.y / voxel),
			(int)Math::floor(p_position.z / voxel));
	const int64_t mask = 0x1FFFFF;
	const int64_t bias = 1 << 20;
	const int64_t x = (((int64_t)cell.x + bias) & mask);
	const int64_t y = (((int64_t)cell.y + bias) & mask);
	const int64_t z = (((int64_t)cell.z + bias) & mask);
	return x | (y << 21) | (z << 42);
}

bool YParticles3D::_lookup_collision_plane(const Vector3 &p_position, Plane &r_plane) const {
	if (collision_quality == COLLISION_QUALITY_HIGH) {
		return false;
	}
	const CollisionCacheEntry *entry = _collision_plane_cache.getptr(_get_collision_voxel_key(p_position));
	if (entry == nullptr || !entry->has_plane) {
		return false;
	}
	r_plane = entry->plane;
	return true;
}

bool YParticles3D::_is_collision_voxel_known_empty(const Vector3 &p_position) const {
	if (collision_quality == COLLISION_QUALITY_HIGH) {
		return false;
	}
	const CollisionCacheEntry *entry = _collision_plane_cache.getptr(_get_collision_voxel_key(p_position));
	return entry != nullptr && entry->known_empty && !entry->has_plane;
}

void YParticles3D::_store_collision_plane(const Vector3 &p_position, const Plane &p_plane) {
	if (collision_quality == COLLISION_QUALITY_HIGH) {
		return;
	}
	CollisionCacheEntry entry;
	entry.has_plane = true;
	entry.known_empty = false;
	entry.plane = p_plane;
	_collision_plane_cache.insert(_get_collision_voxel_key(p_position), entry);
}

void YParticles3D::_store_collision_empty(const Vector3 &p_position) {
	if (collision_quality == COLLISION_QUALITY_HIGH) {
		return;
	}
	CollisionCacheEntry entry;
	entry.has_plane = false;
	entry.known_empty = true;
	_collision_plane_cache.insert(_get_collision_voxel_key(p_position), entry);
}

bool YParticles3D::_query_collision_plane(const Vector3 &p_from, const Vector3 &p_to, Plane &r_plane, Vector3 &r_point) {
	if (p_from.is_equal_approx(p_to)) {
		return false;
	}
	if (_collision_queries_used_this_frame >= _get_collision_query_budget()) {
		return false;
	}
	Ref<World3D> world = get_world_3d();
	if (world.is_null()) {
		return false;
	}
	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (space_state == nullptr) {
		return false;
	}

	Ref<PhysicsRayQueryParameters3D> ray_params = PhysicsRayQueryParameters3D::create(p_from, p_to, collision_layer);
	ray_params->set_collide_with_bodies(true);
	ray_params->set_collide_with_areas(true);
	ray_params->set_hit_back_faces(true);
	ray_params->set_hit_from_inside(true);

	_collision_queries_used_this_frame++;
	Dictionary result = space_state->intersect_ray(ray_params);
	if (result.is_empty()) {
		_store_collision_empty(p_to);
		return false;
	}

	r_point = result["position"];
	Vector3 normal = result["normal"];
	normal = normal.normalized();
	r_plane = Plane(normal, normal.dot(r_point));
	_store_collision_plane(r_point, r_plane);
	return true;
}

void YParticles3D::_apply_particle_collision(Particle &r_particle, Vector3 &r_world_position, Vector3 &r_world_velocity, float p_normalized, double p_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv) {
	if (!enable_collision) {
		return;
	}

	const Vector3 previous_world_position = use_world_space ? (r_particle.last_position + r_particle.creation_position) : (p_global_basis.xform(r_particle.last_position) + get_global_position());
	const Vector3 sampled_scale = _sample_particle_scale(r_particle, p_normalized);
	const float radius = MAX(0.001f, MAX(sampled_scale.x, MAX(sampled_scale.y, sampled_scale.z)) * 0.5f * MAX(collision_radius_scale, 0.0f));
	const Vector3 move = r_world_position - previous_world_position;

	Plane plane;
	Vector3 hit_point;
	bool collided = false;

	if (_lookup_collision_plane(r_world_position, plane)) {
		const float current_distance = plane.distance_to(r_world_position);
		const float previous_distance = plane.distance_to(previous_world_position);
		if ((current_distance <= radius || (previous_distance > radius && current_distance <= radius)) && r_world_velocity.dot(plane.normal) < 0.0f) {
			hit_point = r_world_position - plane.normal * current_distance;
			collided = true;
		}
	}

	if (!collided && !_is_collision_voxel_known_empty(r_world_position) && move.length_squared() > 0.000001f) {
		const Vector3 query_to = previous_world_position + move.normalized() * (move.length() + radius);
		if (!_is_collision_voxel_known_empty(query_to)) {
			collided = _query_collision_plane(previous_world_position, query_to, plane, hit_point);
		}
	}

	if (!collided) {
		return;
	}

	_trigger_sub_emitters(SUB_EMITTER_CONDITION_COLLISION, r_particle, p_normalized, hit_point);

	const Vector3 normal = plane.normal.normalized();
	r_world_position = hit_point + normal * radius;

	const float normal_speed = r_world_velocity.dot(normal);
	Vector3 normal_component = normal * normal_speed;
	Vector3 tangent_component = r_world_velocity - normal_component;
	if (normal_speed < 0.0f) {
		normal_component = -normal_component * MAX(collision_bounce, 0.0f);
	}
	tangent_component *= MAX(0.0f, 1.0f - collision_dampen);
	r_world_velocity = tangent_component + normal_component;

	if (collision_lifetime_loss > 0.0f) {
		const float old_lifetime = r_particle.lifetime;
		r_particle.lifetime = MAX(0.001f, r_particle.lifetime * MAX(0.0f, 1.0f - collision_lifetime_loss));
		const float age = _time - r_particle.creation_time;
		if (age >= r_particle.lifetime || r_particle.lifetime < old_lifetime * 0.05f) {
			_trigger_sub_emitters(SUB_EMITTER_CONDITION_DEATH, r_particle, p_normalized, r_world_position);
			r_particle.dead = true;
			return;
		}
	}

	if (r_world_velocity.length() < collision_min_kill_speed) {
		_trigger_sub_emitters(SUB_EMITTER_CONDITION_DEATH, r_particle, p_normalized, r_world_position);
		r_particle.dead = true;
		return;
	}

	const Vector3 sampled_noise = _sample_noise_velocity(r_particle.position);
	const Vector3 sampled_noise_strength = _sample_noise_strength(p_normalized);
	const Vector3 additive_velocity = r_particle.gravity_velocity + r_particle.force_velocity + Vector3(
			sampled_noise.x * sampled_noise_strength.x,
			sampled_noise.y * sampled_noise_strength.y,
			sampled_noise.z * sampled_noise_strength.z) *
			noise_position_amount;
	r_particle.base_velocity = r_world_velocity - additive_velocity;
	if (r_particle.base_velocity.length_squared() > 0.000001f) {
		r_particle.direction = r_particle.base_velocity.normalized();
	}

	if (use_world_space) {
		r_particle.position = r_world_position - r_particle.creation_position;
	} else {
		r_particle.position = p_global_basis_inv.xform(r_world_position - get_global_position());
	}
	(void)p_delta;
}

bool YParticles3D::_is_registered_sub_emitter_node(const YParticles3D *p_candidate) const {
	if (p_candidate == nullptr || !enable_sub_emitters) {
		return false;
	}
	for (int i = 0; i < sub_emitters.size(); i++) {
		if (sub_emitters[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary entry = sub_emitters[i];
		const NodePath path = entry.get("path", NodePath());
		if (path.is_empty()) {
			continue;
		}
		const YParticles3D *found = Object::cast_to<YParticles3D>(get_node_or_null(path));
		if (found == p_candidate) {
			return true;
		}
	}
	return false;
}

bool YParticles3D::_is_sub_emitter_template_for_parent() const {
	const YParticles3D *parent_particles = Object::cast_to<YParticles3D>(get_parent());
	return parent_particles != nullptr && parent_particles->_is_registered_sub_emitter_node(this);
}

void YParticles3D::_cleanup_spawned_sub_emitter_instances() {
	for (int i = 0; i < _spawned_sub_emitter_instances.size(); i++) {
		YParticles3D *instance = Object::cast_to<YParticles3D>(ObjectDB::get_instance(_spawned_sub_emitter_instances[i]));
		if (instance == nullptr) {
			continue;
		}
		instance->stop(true);
		instance->set_paused(false);
		instance->set_visible(false);
		if (Node *parent = instance->get_parent()) {
			parent->remove_child(instance);
		}
		memdelete(instance);
	}
	_spawned_sub_emitter_instances.clear();
}

void YParticles3D::_prepare_sub_emitter_instance(YParticles3D *p_instance, const Particle &p_particle, float p_normalized) const {
	if (p_instance == nullptr) {
		return;
	}

	p_instance->_pending_sub_emitter_inherit = true;
	p_instance->_pending_sub_emitter_color_multiplier = Color(1, 1, 1, 1);
	p_instance->_pending_sub_emitter_size_multiplier = Vector3(1, 1, 1);
	p_instance->_pending_sub_emitter_rotation_offset = Vector3();
	p_instance->_pending_sub_emitter_lifetime_multiplier = 1.0f;
	p_instance->_pending_sub_emitter_inherit_duration = false;
	p_instance->_pending_sub_emitter_source_time = _time;
	p_instance->_pending_sub_emitter_source_emission_time = _emission_time;
}

void YParticles3D::_trigger_sub_emitters(SubEmitterCondition p_event, const Particle &p_particle, float p_normalized, const Vector3 &p_world_position) {
	if (!enable_sub_emitters || sub_emitters.is_empty()) {
		return;
	}

	for (int i = 0; i < sub_emitters.size(); i++) {
		if (sub_emitters[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary entry = sub_emitters[i];
		const NodePath path = entry.get("path", NodePath());
		if (path.is_empty()) {
			continue;
		}
		if ((int)entry.get("event", (int)SUB_EMITTER_CONDITION_BIRTH) != (int)p_event) {
			continue;
		}
		const float probability = CLAMP((float)entry.get("probability", 1.0f), 0.0f, 1.0f);
		if (_rng->randf() > probability) {
			continue;
		}

		YParticles3D *template_particles = Object::cast_to<YParticles3D>(get_node_or_null(path));
		if (template_particles == nullptr) {
			continue;
		}

		Node *duplicate = template_particles->duplicate();
		YParticles3D *instance = Object::cast_to<YParticles3D>(duplicate);
		if (instance == nullptr) {
			if (duplicate != nullptr) {
				memdelete(duplicate);
			}
			continue;
		}

		Node *spawn_parent = template_particles->get_parent();
		if (spawn_parent == nullptr) {
			memdelete(instance);
			continue;
		}

		const int inherit_flags = _normalize_sub_emitter_inherit_flags((int)entry.get("inherit", (int)SUB_EMITTER_INHERIT_NOTHING));
		instance->_spawned_as_sub_emitter_instance = true;
		instance->set_play_on_start(false);
		instance->set_loop(false);
		instance->set_destroy_on_finish(true);
		instance->set_paused(false);
		_prepare_sub_emitter_instance(instance, p_particle, p_normalized);
		if (inherit_flags & SUB_EMITTER_INHERIT_COLOR) {
			instance->tint_color *= _sample_particle_color(p_particle, p_normalized);
		}
		if (inherit_flags & SUB_EMITTER_INHERIT_SIZE) {
			instance->_pending_sub_emitter_size_multiplier = _sample_particle_scale(p_particle, p_normalized);
		}
		if (inherit_flags & SUB_EMITTER_INHERIT_ROTATION) {
			instance->_pending_sub_emitter_rotation_offset = p_particle.rotation;
		}
		if (inherit_flags & SUB_EMITTER_INHERIT_LIFETIME) {
			const float remaining = CLAMP(1.0f - p_normalized, 0.0f, 1.0f);
			instance->_pending_sub_emitter_lifetime_multiplier = remaining;
		}
		if (inherit_flags & SUB_EMITTER_INHERIT_DURATION) {
			instance->_pending_sub_emitter_inherit_duration = true;
		}

		spawn_parent->add_child(instance);
		if (Node3D *instance_3d = Object::cast_to<Node3D>(instance)) {
			Transform3D spawn_transform = template_particles->get_global_transform();
			spawn_transform.origin = p_world_position;
			instance_3d->set_global_transform(spawn_transform);
		}
		_spawned_sub_emitter_instances.push_back(instance->get_instance_id());
		instance->play(true);
	}
}

RID YParticles3D::_create_shared_quad_mesh() {
	static RID shared_quad_mesh;
	static Array shared_mesh_arrays;

	if (shared_quad_mesh.is_valid()) {
		return shared_quad_mesh;
	}

	shared_quad_mesh = RenderingServer::get_singleton()->mesh_create();
	if (shared_mesh_arrays.is_empty()) {
		shared_mesh_arrays.resize(RenderingServer::ARRAY_MAX);

		PackedVector3Array vertices;
		vertices.push_back(Vector3(-0.5f, -0.5f, 0.0f));
		vertices.push_back(Vector3(0.5f, -0.5f, 0.0f));
		vertices.push_back(Vector3(0.5f, 0.5f, 0.0f));
		vertices.push_back(Vector3(-0.5f, 0.5f, 0.0f));
		shared_mesh_arrays[RenderingServer::ARRAY_VERTEX] = vertices;

		PackedVector2Array uvs;
		uvs.push_back(Vector2(0.0f, 1.0f));
		uvs.push_back(Vector2(1.0f, 1.0f));
		uvs.push_back(Vector2(1.0f, 0.0f));
		uvs.push_back(Vector2(0.0f, 0.0f));
		shared_mesh_arrays[RenderingServer::ARRAY_TEX_UV] = uvs;

		PackedInt32Array indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);
		indices.push_back(0);
		indices.push_back(2);
		indices.push_back(3);
		shared_mesh_arrays[RenderingServer::ARRAY_INDEX] = indices;
	}

	RenderingServer::get_singleton()->mesh_add_surface_from_arrays(shared_quad_mesh, RenderingServer::PRIMITIVE_TRIANGLES, shared_mesh_arrays);
	return shared_quad_mesh;
}

RID YParticles3D::_get_particle_mesh() const {
	if (custom_mesh.is_valid()) {
		return custom_mesh->get_rid();
	}
	return const_cast<YParticles3D *>(this)->_create_shared_quad_mesh();
}

Ref<Material> YParticles3D::_create_material() const {
	if (override_material.is_valid()) {
		return override_material;
	}

	const String shader_setting = _yparticles3d_get_shader_setting_name(blend_mode, sampling_filter);
	const String shader_path = ProjectSettings::get_singleton()->get_setting(shader_setting, "");
	if (shader_path.is_empty()) {
		return Ref<Material>();
	}

	Ref<Shader> shader = ResourceLoader::get_singleton()->load(shader_path);
	if (shader.is_null()) {
		ERR_PRINT("YParticles3D failed to load shader from Project Settings path: " + shader_path);
		return Ref<Material>();
	}

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	material->set_render_priority(render_priority);

	if (particle_texture.is_valid()) {
		material->set_shader_parameter("albedo_texture", particle_texture);
	}
	if (enable_color_over_lifetime && color_over_lifetime.is_valid()) {
		material->set_shader_parameter("gradient_texture", color_over_lifetime);
	}
	if (color_over_lifetime_secondary.is_valid()) {
		material->set_shader_parameter("secondary_gradient_texture", color_over_lifetime_secondary);
	}
	if (start_color_gradient.is_valid()) {
		material->set_shader_parameter("start_color_gradient_texture", start_color_gradient);
	}

	return material;
}

Ref<Material> YParticles3D::_create_trail_material() const {
	const String shader_setting = _yparticles3d_get_trail_shader_setting_name(blend_mode, sampling_filter);
	const String shader_path = ProjectSettings::get_singleton()->get_setting(shader_setting, "");

	Ref<ShaderMaterial> material;
	if (!shader_path.is_empty()) {
		Ref<Shader> shader = ResourceLoader::get_singleton()->load(shader_path);
		if (shader.is_valid()) {
			material.instantiate();
			material->set_shader(shader);
			material->set_render_priority(render_priority);
		} else {
			ERR_PRINT("YParticles3D failed to load trail shader from Project Settings path: " + shader_path);
		}
	}

	if (material.is_null()) {
		Ref<StandardMaterial3D> fallback_material;
		fallback_material.instantiate();
		fallback_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		fallback_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		fallback_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, trail_texture);
		fallback_material->set_render_priority(render_priority);
		return fallback_material;
	}

	if (trail_texture.is_valid()) {
		material->set_shader_parameter("albedo_texture", trail_texture);
	}
	return material;
}

void YParticles3D::_update_material() {
	if (!_instance.is_valid()) {
		return;
	}

	if (!_shared_material_enabled) {
		RenderingServer::get_singleton()->instance_geometry_set_material_override(_instance, override_material.is_valid() ? override_material->get_rid() : RID());
		return;
	}

	_shared_material = _create_material();
	RenderingServer::get_singleton()->instance_geometry_set_material_override(_instance, _shared_material.is_valid() ? _shared_material->get_rid() : RID());
}

void YParticles3D::_update_trail_material() {
	if (!_trail_instance.is_valid()) {
		return;
	}

	_trail_material = _create_trail_material();
	RenderingServer::get_singleton()->instance_geometry_set_material_override(_trail_instance, _trail_material.is_valid() ? _trail_material->get_rid() : RID());
}

void YParticles3D::_create_multimesh() {
	_create_shared_quad_mesh();
	_free_rendering();

	if (max_particles <= 0 || !is_inside_tree() || get_world_3d().is_null()) {
		return;
	}

	_multimesh = RenderingServer::get_singleton()->multimesh_create();
	RenderingServer::get_singleton()->multimesh_allocate_data(_multimesh, max_particles, RenderingServer::MULTIMESH_TRANSFORM_3D, true, true);
	RenderingServer::get_singleton()->multimesh_set_mesh(_multimesh, custom_mesh.is_valid() ? custom_mesh->get_rid() : _create_shared_quad_mesh());

	_buffer.resize(max_particles * 20);
	RenderingServer::get_singleton()->multimesh_set_visible_instances(_multimesh, 0);
	RenderingServer::get_singleton()->multimesh_set_custom_aabb(_multimesh, visibility_aabb);

	if (_particles.size() > max_particles) {
		_particles.resize(max_particles);
	}
	_visible_count = MIN(_visible_count, max_particles);

	_instance = RenderingServer::get_singleton()->instance_create2(_multimesh, get_world_3d()->get_scenario());
	RenderingServer::get_singleton()->instance_set_layer_mask(_instance, rendering_layer);
	RenderingServer::get_singleton()->instance_set_visible(_instance, is_visible_in_tree());
	_update_instance_transform();
	_update_material();
	_update_material_shader_params();
	_create_trail_rendering();
}

void YParticles3D::_create_trail_rendering() {
	if (!enable_trails || !is_inside_tree() || get_world_3d().is_null()) {
		return;
	}
	_trail_mesh = RenderingServer::get_singleton()->mesh_create();
	_trail_instance = RenderingServer::get_singleton()->instance_create2(_trail_mesh, get_world_3d()->get_scenario());
	RenderingServer::get_singleton()->instance_set_layer_mask(_trail_instance, rendering_layer);
	RenderingServer::get_singleton()->instance_set_visible(_trail_instance, is_visible_in_tree());
	_update_trail_instance_transform();
	_update_trail_material();
}

void YParticles3D::_free_rendering() {
	if (_trail_instance.is_valid()) {
		RenderingServer::get_singleton()->free_rid(_trail_instance);
		_trail_instance = RID();
	}
	if (_trail_mesh.is_valid()) {
		RenderingServer::get_singleton()->free_rid(_trail_mesh);
		_trail_mesh = RID();
	}
	if (_instance.is_valid()) {
		RenderingServer::get_singleton()->free_rid(_instance);
		_instance = RID();
	}
	if (_multimesh.is_valid()) {
		RenderingServer::get_singleton()->free_rid(_multimesh);
		_multimesh = RID();
	}
}

void YParticles3D::_clear_buffer() {
	float *buffer_write = _buffer.ptrw();
	for (int i = 0; i < _buffer.size(); i++) {
		buffer_write[i] = 0.0f;
	}
}

void YParticles3D::_update_instance_transform() {
	if (!_instance.is_valid()) {
		return;
	}
	RenderingServer::get_singleton()->instance_set_transform(_instance, use_world_space ? Transform3D() : get_global_transform());
	_last_transform = get_global_transform();
}

void YParticles3D::_update_trail_instance_transform() {
	if (!_trail_instance.is_valid()) {
		return;
	}
	const bool use_identity = use_world_space || trail_world_space;
	RenderingServer::get_singleton()->instance_set_transform(_trail_instance, use_identity ? Transform3D() : get_global_transform());
}

void YParticles3D::_update_material_shader_params() {
	if (!_instance.is_valid()) {
		return;
	}
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "particles_anim_h_frames", h_frames);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "particles_anim_v_frames", v_frames);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "particles_anim_tiles_mode", (int)tiles_mode);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "particles_anim_enabled", texture_sheet_enabled);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "billboard_mode", (int)billboard_mode);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "render_alignment", (int)render_alignment);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "align_to_velocity", align_to_velocity ? 1 : 0);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "align_offset_radians", Math::deg_to_rad(align_offset_degrees));
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "use_start_color_gradient", use_start_color_gradient ? 1 : 0);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "use_secondary_gradient", color_over_lifetime_use_two_gradients ? 1 : 0);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "use_color_random_factor", (use_start_color_gradient || color_over_lifetime_use_two_gradients) ? 1 : 0);
	RenderingServer::get_singleton()->instance_geometry_set_shader_parameter(_instance, "tint_color", tint_color);
}

void YParticles3D::_clear_trails() {
	for (int i = 0; i < _trail_states.size(); i++) {
		_trail_states.write[i].clear();
	}
	if (_trail_mesh.is_valid()) {
		RenderingServer::get_singleton()->mesh_clear(_trail_mesh);
	}
}

void YParticles3D::_initialize_cone_particle(Particle &r_particle) {
	const float arc_angle = _next_arc_angle(r_particle.burst_spot);
	const float outer_radius = radius;
	const float inner_radius = radius * (1.0f - radius_thickness);
	const float radius_sample = _rng->randf();
	const float r = Math::sqrt(Math::lerp(inner_radius * inner_radius, outer_radius * outer_radius, radius_sample));

	r_particle.position = Vector3(r * Math::cos(arc_angle), 0.0f, r * Math::sin(arc_angle));

	const float cone_angle_rad = Math::deg_to_rad(CLAMP(angle, 0.0f, 89.99f));
	if (r_particle.position.length_squared() > 0.0001f) {
		const float top_radius = r + (shape_length * Math::tan(cone_angle_rad));
		const Vector2 base_pos = Vector2(r_particle.position.x, r_particle.position.z).normalized();
		const Vector3 target(base_pos.x * top_radius, shape_length, base_pos.y * top_radius);
		r_particle.direction = (target - r_particle.position).normalized();
	} else {
		r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
	}

	if (emit_from == EMIT_FROM_VOLUME && shape_length > 0.001f) {
		const float height = _rng->randf() * shape_length;
		const float radius_at_height = (r * (1.0f - (height / shape_length))) + (height * Math::tan(cone_angle_rad));
		r_particle.position.y = height;
		if (Math::abs(r) > 0.0001f) {
			r_particle.position.x *= radius_at_height / r;
			r_particle.position.z *= radius_at_height / r;
		}
	}
}

void YParticles3D::_initialize_sphere_particle(Particle &r_particle, bool p_hemisphere) {
	const float arc_angle = _next_arc_angle(r_particle.burst_spot);
	const float outer_radius = radius;
	const float inner_radius = radius * (1.0f - radius_thickness);
	const float radius_sample = _rng->randf();
	const float r = Math::pow(Math::lerp(inner_radius * inner_radius * inner_radius, outer_radius * outer_radius * outer_radius, radius_sample), 1.0f / 3.0f);
	const float costheta = p_hemisphere ? _rng->randf_range(0.0f, 1.0f) : _rng->randf_range(-1.0f, 1.0f);
	const float theta = Math::acos(costheta);

	r_particle.position = Vector3(r * Math::sin(theta) * Math::cos(arc_angle), r * Math::cos(theta), r * Math::sin(theta) * Math::sin(arc_angle));
	r_particle.direction = r_particle.position.normalized();
}

void YParticles3D::_initialize_box_particle(Particle &r_particle) {
	r_particle.position = Vector3(_rng->randf_range(-box_extents.x, box_extents.x), _rng->randf_range(-box_extents.y, box_extents.y), _rng->randf_range(-box_extents.z, box_extents.z));
	r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
}

void YParticles3D::_initialize_circle_particle(Particle &r_particle) {
	const float arc_angle = _next_arc_angle(r_particle.burst_spot);
	const float outer_radius = radius;
	const float inner_radius = radius * (1.0f - radius_thickness);
	const float radius_sample = _rng->randf();
	const float r = Math::sqrt(Math::lerp(inner_radius * inner_radius, outer_radius * outer_radius, radius_sample));
	r_particle.position = Vector3(r * Math::cos(arc_angle), 0.0f, r * Math::sin(arc_angle));
	r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
}

void YParticles3D::_initialize_edge_particle(Particle &r_particle) {
	r_particle.position = Vector3(0.0f, Math::lerp(-radius, radius, _rng->randf()), 0.0f);
	r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
}

void YParticles3D::_update_emission_mesh_cache() {
	_emission_mesh_face_centers.clear();
	_emission_mesh_cache_dirty = false;

	if (emission_mesh.is_null()) {
		return;
	}

	for (int surface_i = 0; surface_i < emission_mesh->get_surface_count(); surface_i++) {
		if (emission_mesh->_surface_get_primitive_type(surface_i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}

		const Array arrays = emission_mesh->surface_get_arrays(surface_i);
		if (arrays.is_empty() || arrays[Mesh::ARRAY_VERTEX].get_type() == Variant::NIL) {
			continue;
		}

		const PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		if (vertices.is_empty()) {
			continue;
		}

		if (arrays[Mesh::ARRAY_INDEX].get_type() != Variant::NIL) {
			const PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
			for (int i = 0; i + 2 < indices.size(); i += 3) {
				const int a = indices[i];
				const int b = indices[i + 1];
				const int c = indices[i + 2];
				if (a < 0 || b < 0 || c < 0 || a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
					continue;
				}
				const Vector3 center = (vertices[a] + vertices[b] + vertices[c]) / 3.0f;
				_emission_mesh_face_centers.push_back(center * emission_mesh_scale);
			}
		} else {
			for (int i = 0; i + 2 < vertices.size(); i += 3) {
				const Vector3 center = (vertices[i] + vertices[i + 1] + vertices[i + 2]) / 3.0f;
				_emission_mesh_face_centers.push_back(center * emission_mesh_scale);
			}
		}
	}
}

void YParticles3D::_initialize_mesh_particle(Particle &r_particle) {
	if (_emission_mesh_cache_dirty) {
		_update_emission_mesh_cache();
	}

	if (_emission_mesh_face_centers.is_empty()) {
		r_particle.position = Vector3();
		r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
		return;
	}

	const int random_index = _rng->randi_range(0, _emission_mesh_face_centers.size() - 1);
	r_particle.position = _emission_mesh_face_centers[random_index];
	r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
}

void YParticles3D::_initialize_particle(Particle &r_particle) {
	r_particle.dead = false;
	r_particle.visible = true;
	r_particle.trail_only = false;
	r_particle.trail_enabled = enable_trails && (_rng->randf() <= CLAMP(trail_ratio, 0.0f, 1.0f));
	r_particle.creation_time = _time;
	r_particle.creation_position = use_world_space ? get_global_position() : Vector3();
	r_particle.force_velocity = Vector3();
	r_particle.inherited_velocity = Vector3();
	r_particle.random_a = _rng->randf();
	r_particle.random_b = _rng->randf();
	r_particle.start_alpha = 1.0f;
	r_particle.lifetime = MAX(0.001f, _pick_start_lifetime() * (_pending_sub_emitter_inherit ? _pending_sub_emitter_lifetime_multiplier : 1.0f));
	r_particle.scale = _pick_start_size(r_particle);
	if (_pending_sub_emitter_inherit) {
		r_particle.scale *= _pending_sub_emitter_size_multiplier;
	}
	const Vector3 start_rotation_degrees = _pick_start_rotation_degrees(r_particle);
	r_particle.rotation = Vector3(
			Math::deg_to_rad(start_rotation_degrees.x),
			Math::deg_to_rad(start_rotation_degrees.y),
			Math::deg_to_rad(start_rotation_degrees.z)) + (_pending_sub_emitter_inherit ? _pending_sub_emitter_rotation_offset : Vector3());
	r_particle.hue_offset = enable_color_over_lifetime ? (starting_hue + _rng->randf() * hue_variation) : 0.0f;
	r_particle.gravity_velocity = play_in_reverse ? -gravity * r_particle.lifetime : Vector3();
	r_particle.position = Vector3();
	r_particle.direction = Vector3(0.0f, 1.0f, 0.0f);
	r_particle.orbit_angle = 0.0f;

	if (texture_sheet_enabled && use_random_starting_tile) {
		r_particle.tilesheet_starting_tile = tiles_mode == TEXTURE_SHEET_TILES_WHOLE_SHEET ? _rng->randi_range(0, MAX(0, h_frames * v_frames - 1)) : _rng->randi_range(0, MAX(0, v_frames - 1));
	} else {
		r_particle.tilesheet_starting_tile = start_index_tile;
	}

	if (enable_shape) {
		switch (shape_type) {
			case EMISSION_SHAPE_SPHERE:
				_initialize_sphere_particle(r_particle, false);
				break;
			case EMISSION_SHAPE_HEMISPHERE:
				_initialize_sphere_particle(r_particle, true);
				break;
			case EMISSION_SHAPE_BOX:
				_initialize_box_particle(r_particle);
				break;
			case EMISSION_SHAPE_CIRCLE:
				_initialize_circle_particle(r_particle);
				break;
			case EMISSION_SHAPE_EDGE:
				_initialize_edge_particle(r_particle);
				break;
			case EMISSION_SHAPE_MESH:
				_initialize_mesh_particle(r_particle);
				break;
			case EMISSION_SHAPE_CONE:
			default:
				_initialize_cone_particle(r_particle);
				break;
		}
	}

	if (spherize_direction > 0.001f && r_particle.position.length_squared() > 0.0001f) {
		r_particle.direction = r_particle.direction.lerp(r_particle.position.normalized(), spherize_direction).normalized();
	}

	if (random_direction > 0.001f) {
		r_particle.direction = r_particle.direction.lerp(_get_random_unit_vector(), random_direction).normalized();
	}

	if (rotation_offset != Vector3()) {
		Basis basis;
		if (direction_in_world_space) {
			basis = basis.rotated(Vector3(1.0f, 0.0f, 0.0f), Math::deg_to_rad(rotation_offset.x));
			basis = basis.rotated(Vector3(0.0f, 1.0f, 0.0f), Math::deg_to_rad(rotation_offset.y));
			basis = basis.rotated(Vector3(0.0f, 0.0f, -1.0f), Math::deg_to_rad(rotation_offset.z));
		} else {
			basis = basis.rotated(Vector3(0.0f, 0.0f, -1.0f), Math::deg_to_rad(rotation_offset.z));
			basis = basis.rotated(Vector3(0.0f, 1.0f, 0.0f), Math::deg_to_rad(rotation_offset.y));
			basis = basis.rotated(Vector3(1.0f, 0.0f, 0.0f), Math::deg_to_rad(rotation_offset.x));
		}
		r_particle.direction = basis.xform(r_particle.direction * (invert_direction ? -1.0f : 1.0f)).normalized();
		r_particle.position = basis.xform(r_particle.position);
	} else if (invert_direction) {
		r_particle.direction = -r_particle.direction;
	}

	r_particle.distance = _pick_start_speed();
	const Basis global_basis = get_global_transform().basis;
	r_particle.base_velocity = r_particle.direction * r_particle.distance;
	if (!direction_in_world_space) {
		r_particle.base_velocity = global_basis.xform(r_particle.base_velocity);
	}
	if (enable_inherit_velocity) {
		r_particle.inherited_velocity = _emitter_velocity_world;
	}
	if (use_start_color_gradient && start_color_gradient.is_valid() && start_color_gradient->get_gradient().is_valid()) {
		Color sampled_alpha_color = start_color_gradient->get_gradient()->sample(CLAMP(r_particle.random_a, 0.0f, 1.0f));
		if (start_color_use_two_gradients && start_color_gradient_secondary.is_valid() && start_color_gradient_secondary->get_gradient().is_valid()) {
			const Color secondary_alpha_color = start_color_gradient_secondary->get_gradient()->sample(CLAMP(r_particle.random_a, 0.0f, 1.0f));
			sampled_alpha_color = sampled_alpha_color.lerp(secondary_alpha_color, r_particle.random_b);
		}
		r_particle.start_alpha = sampled_alpha_color.a;
	}

	if (use_world_space) {
		r_particle.position = global_basis.xform(r_particle.position);
	}

	if (play_in_reverse) {
		const float lifetime_seconds = r_particle.lifetime / MAX(playback_speed, 0.001f);
		r_particle.position += (r_particle.base_velocity * lifetime_seconds) + (0.5f * gravity * lifetime_seconds * lifetime_seconds);
		r_particle.base_velocity = -r_particle.base_velocity;
	}

	r_particle.last_position = r_particle.position;
}

void YParticles3D::_emit_particle(float p_burst_spot, bool p_has_override_position, const Vector3 &p_override_position) {
	if (!emitting || max_particles <= 0) {
		return;
	}

	Particle *particle = nullptr;
	if (_particles.size() < max_particles) {
		Particle new_particle;
		new_particle.index = _particles.size();
		new_particle.slot_index = _particles.size();
		_particles.push_back(new_particle);
		if (_trail_states.size() < _particles.size()) {
			TrailState new_trail_state;
			_trail_states.push_back(new_trail_state);
		}
		particle = &_particles.write[_particles.size() - 1];
	} else {
		for (int i = 0; i < _particles.size(); i++) {
			if (_particles.write[i].dead) {
				particle = &_particles.write[i];
				break;
			}
		}

		if (particle == nullptr) {
			int oldest_index = 0;
			float oldest_time = _time + 1.0f;
			for (int i = 0; i < _particles.size(); i++) {
				if (_particles[i].creation_time < oldest_time) {
					oldest_time = _particles[i].creation_time;
					oldest_index = i;
				}
			}
			particle = &_particles.write[oldest_index];
		}
	}

	particle->burst_spot = p_burst_spot;
	if (particle->slot_index < 0) {
		particle->slot_index = particle - _particles.ptrw();
	}
	_trail_states.write[particle->slot_index].clear();
	_initialize_particle(*particle);
	if (p_has_override_position && use_world_space) {
		particle->creation_position = p_override_position;
	}
	const Transform3D global_transform = get_global_transform();
	if (particle->trail_enabled) {
		const Basis global_basis = global_transform.basis;
		const Basis global_basis_inv = global_basis.inverse();
		const Vector3 global_position = global_transform.origin;
		_append_trail_point(*particle, _trail_states.write[particle->slot_index], 0.0f, global_basis, global_basis_inv, global_position);
	}
	const Vector3 world_position = use_world_space ? (particle->position + particle->creation_position) : (global_transform.basis.xform(particle->position) + global_transform.origin);
	_trigger_sub_emitters(SUB_EMITTER_CONDITION_BIRTH, *particle, 0.0f, world_position);
}

void YParticles3D::_update_particle(Particle &r_particle, float p_normalized, double p_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position) {
	Vector3 fallback_direction = r_particle.direction;
	if (!direction_in_world_space) {
		fallback_direction = p_global_basis.xform(fallback_direction);
	}
	Vector3 velocity = _sample_velocity_over_lifetime(r_particle, r_particle.base_velocity, fallback_direction, p_normalized);
	velocity = _sample_limit_velocity_over_lifetime(velocity, p_normalized);
	r_particle.force_velocity += _sample_force_over_lifetime(r_particle, p_normalized, p_global_basis) * (float)p_delta * playback_speed;
	const Vector3 noise_strength_sample = enable_noise ? _sample_noise_strength(p_normalized) : Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 raw_noise = enable_noise ? _sample_noise_velocity(r_particle.position) : Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 noise_velocity = enable_noise ? Vector3(
			raw_noise.x * noise_strength_sample.x,
			raw_noise.y * noise_strength_sample.y,
			raw_noise.z * noise_strength_sample.z) *
			noise_position_amount : Vector3(0.0f, 0.0f, 0.0f);
	const float noise_scalar = enable_noise ? (raw_noise.x + raw_noise.y + raw_noise.z) / 3.0f : 0.0f;

	r_particle.gravity_velocity += gravity * (play_in_reverse ? -1.0f : 1.0f) * (float)p_delta;
	Vector3 world_position = use_world_space ? (r_particle.position + r_particle.creation_position) : (p_global_basis.xform(r_particle.position) + p_global_position);
	Vector3 world_velocity = velocity + r_particle.gravity_velocity + r_particle.force_velocity + noise_velocity;
	if (enable_inherit_velocity) {
		const float inherit_multiplier = _sample_inherit_velocity_multiplier(p_normalized);
		const Vector3 inherited_velocity = (inherit_velocity_mode >= 2 ? r_particle.inherited_velocity : _emitter_velocity_world) * inherit_multiplier;
		world_velocity += inherited_velocity;
	}
	world_position += world_velocity * (float)p_delta * playback_speed;
	if (!Math::is_zero_approx(noise_rotation_amount)) {
		r_particle.rotation.z += Math::deg_to_rad(noise_scalar * noise_rotation_amount) * (float)p_delta * playback_speed;
	}

	if (enable_rotation_over_lifetime && orbit_over_lifetime.is_valid()) {
		Vector3 orbit_axis = orbit_around_axis;
		if (orbit_axis.length_squared() <= 0.000001f) {
			orbit_axis = Vector3(0.0f, 1.0f, 0.0f);
		} else {
			orbit_axis.normalize();
		}

		const float orbit_angle = orbit_over_lifetime->sample(p_normalized) * (float)Math_TAU * 0.01f;
		const float delta_orbit_angle = orbit_angle - r_particle.orbit_angle;
		r_particle.orbit_angle = orbit_angle;

		if (!Math::is_zero_approx(delta_orbit_angle)) {
			const Basis rotation(orbit_axis, delta_orbit_angle);
			Vector3 local_position = use_world_space ? p_global_basis_inv.xform(world_position - r_particle.creation_position) : r_particle.position;
			Vector3 local_velocity = use_world_space ? p_global_basis_inv.xform(r_particle.base_velocity) : r_particle.base_velocity;
			local_position = rotation.xform(local_position);
			local_velocity = rotation.xform(local_velocity);
			r_particle.base_velocity = use_world_space ? p_global_basis.xform(local_velocity) : local_velocity;
			world_position = use_world_space ? (r_particle.creation_position + p_global_basis.xform(local_position)) : (p_global_basis.xform(local_position) + p_global_position);
		}
	}

	if (enable_collision) {
		_apply_particle_collision(r_particle, world_position, world_velocity, p_normalized, p_delta, p_global_basis, p_global_basis_inv);
	}

	if (!r_particle.dead && enable_attractor && attraction_over_lifetime.is_valid()) {
		const float attractor_amount = CLAMP(attraction_over_lifetime->sample(CLAMP(p_normalized, 0.0f, 1.0f)), 0.0f, 1.0f);
		if (!Math::is_zero_approx(attractor_amount)) {
			world_position = world_position.lerp(_get_attraction_target_position(), attractor_amount);
		}
	}

	if (!r_particle.dead) {
		if (use_world_space) {
			r_particle.position = world_position - r_particle.creation_position;
		} else {
			r_particle.position = p_global_basis_inv.xform(world_position - p_global_position);
		}
	}
	r_particle.last_position = r_particle.position;
}

Vector3 YParticles3D::_get_particle_world_position(const Particle &p_particle, const Basis &p_global_basis, const Vector3 &p_global_position) const {
	return use_world_space ? (p_particle.position + p_particle.creation_position) : (p_global_basis.xform(p_particle.position) + p_global_position);
}

Vector3 YParticles3D::_world_to_trail_space(const Vector3 &p_world_position, const Basis &p_global_basis_inv, const Vector3 &p_global_position) const {
	if (trail_world_space || use_world_space) {
		return p_world_position;
	}
	return p_global_basis_inv.xform(p_world_position - p_global_position);
}

void YParticles3D::_trim_trail_points(TrailState &r_trail_state) {
	while (!r_trail_state.points.is_empty() && r_trail_state.points[0].expiry_time <= _time) {
		r_trail_state.points.remove_at(0);
	}
}

void YParticles3D::_append_trail_point(Particle &r_particle, TrailState &r_trail_state, float p_normalized, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position) {
	_trim_trail_points(r_trail_state);
	if (!r_particle.trail_enabled) {
		return;
	}

	const Vector3 world_position = _get_particle_world_position(r_particle, p_global_basis, p_global_position);
	const Vector3 trail_position = _world_to_trail_space(world_position, p_global_basis_inv, p_global_position);
	if (!r_trail_state.points.is_empty()) {
		const float distance = r_trail_state.points[r_trail_state.points.size() - 1].position.distance_to(trail_position);
		if (distance < MAX(trail_min_vertex_distance, 0.001f)) {
			r_trail_state.points.write[r_trail_state.points.size() - 1].particle_normalized = CLAMP(p_normalized, 0.0f, 1.0f);
			return;
		}
	}

	TrailPoint point;
	point.position = trail_position;
	point.particle_normalized = CLAMP(p_normalized, 0.0f, 1.0f);
	point.size = MAX(r_particle.scale.x, MAX(r_particle.scale.y, r_particle.scale.z));
	point.particle_color = _sample_particle_color(r_particle, p_normalized);
	point.particle_color.a *= _sample_alpha_over_lifetime(r_particle, p_normalized);

	float point_lifetime = _sample_trail_lifetime(p_normalized) * r_particle.lifetime;
	if (trail_size_affects_lifetime) {
		point_lifetime *= MAX(point.size, 0.001f);
	}
	point.expiry_time = _time + MAX(point_lifetime, 0.001f);
	r_trail_state.points.push_back(point);
}

void YParticles3D::_write_particle_to_buffer(const Particle &p_particle, float p_normalized, const Basis &p_global_basis, const Basis &p_global_basis_inv) {
	if (!_multimesh.is_valid()) {
		return;
	}

	const int idx = p_particle.index * 20;
	if (idx + 19 >= _buffer.size()) {
		return;
	}
	float *buffer_write = _buffer.ptrw();

	Vector3 scale = _sample_particle_scale(p_particle, p_normalized);

	Vector3 fallback_direction = p_particle.direction;
	if (!direction_in_world_space) {
		fallback_direction = p_global_basis.xform(fallback_direction);
	}
	Vector3 sampled_velocity = _sample_velocity_over_lifetime(p_particle, p_particle.base_velocity, fallback_direction, p_normalized);
	sampled_velocity = _sample_limit_velocity_over_lifetime(sampled_velocity, p_normalized);
	if (enable_noise && !Math::is_zero_approx(noise_position_amount)) {
		const Vector3 raw_noise = _sample_noise_velocity(p_particle.position);
		const Vector3 noise_strength_sample = _sample_noise_strength(p_normalized);
		sampled_velocity += Vector3(
				raw_noise.x * noise_strength_sample.x,
				raw_noise.y * noise_strength_sample.y,
				raw_noise.z * noise_strength_sample.z) *
				noise_position_amount;
	}

	Vector3 forward = p_particle.direction;
	const Vector3 velocity_combined = sampled_velocity + p_particle.gravity_velocity + p_particle.force_velocity;
	if ((align_to_velocity || billboard_mode == BILLBOARD_MODE_STRETCHED || billboard_mode == BILLBOARD_MODE_STRETCHED_VERTICAL) && velocity_combined.length() > 0.001f) {
		const Vector3 local_velocity = p_global_basis_inv.xform(velocity_combined);
		forward = local_velocity.normalized();
	}

	Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
	if (Math::abs(forward.dot(up)) > 0.99f) {
		up = Vector3(0.0f, 0.0f, -1.0f);
	}
	Vector3 right = up.cross(forward).normalized();
	up = forward.cross(right).normalized();

	float stretch = 1.0f;
	if (billboard_mode == BILLBOARD_MODE_STRETCHED || billboard_mode == BILLBOARD_MODE_STRETCHED_VERTICAL) {
		stretch += length_stretch;
		if (Math::abs(velocity_stretch) > 0.001f) {
			stretch += velocity_combined.length() * velocity_stretch;
		}
		stretch = MAX(0.01f, stretch);
	}

	Vector3 particle_rotation = p_particle.rotation;
	particle_rotation.z += ((enable_rotation_over_lifetime && rotation_over_lifetime.is_valid()) ? rotation_over_lifetime->sample(p_normalized) : 0.0f) +
			Math::deg_to_rad(align_offset_degrees);
	if (enable_rotation_by_speed) {
		const float min_speed = rotation_by_speed_range.x;
		const float max_speed = MAX(rotation_by_speed_range.y, min_speed + 0.0001f);
		const float normalized_speed = CLAMP((velocity_combined.length() - min_speed) / (max_speed - min_speed), 0.0f, 1.0f);
		if (rotation_by_speed_mode == 1) {
			particle_rotation += Vector3(
					rotation_by_speed_x.is_valid() ? rotation_by_speed_x->sample(normalized_speed) : 0.0f,
					rotation_by_speed_y.is_valid() ? rotation_by_speed_y->sample(normalized_speed) : 0.0f,
					rotation_by_speed_z.is_valid() ? rotation_by_speed_z->sample(normalized_speed) : 0.0f);
		} else if (rotation_by_speed.is_valid()) {
			particle_rotation.z += rotation_by_speed->sample(normalized_speed);
		}
	}
	const bool shader_driven_transform = override_material.is_null() && custom_mesh.is_null();

	Basis basis;
	if (shader_driven_transform) {
		// The built-in particle shader already applies billboard orientation, scale,
		// angle, and stretch from MODEL_MATRIX + INSTANCE_CUSTOM. Keep the CPU-side
		// transform cheap and avoid double-applying those effects here.
		basis = Basis(right, up, forward);
	} else {
		basis = Basis(right, up, forward);
		if (!Math::is_zero_approx(particle_rotation.x) || !Math::is_zero_approx(particle_rotation.y) || !Math::is_zero_approx(particle_rotation.z)) {
			Basis local_rotation;
			local_rotation = local_rotation.rotated(Vector3(1.0f, 0.0f, 0.0f), particle_rotation.x);
			local_rotation = local_rotation.rotated(Vector3(0.0f, 1.0f, 0.0f), particle_rotation.y);
			local_rotation = local_rotation.rotated(Vector3(0.0f, 0.0f, 1.0f), particle_rotation.z);
			basis = basis * local_rotation;
		}
		basis.scale(Vector3(scale.x, scale.y * stretch, scale.z));
	}
	if (use_world_space) {
		basis = p_global_basis * basis;
	}

	Transform3D xform;
	xform.basis = basis;
	xform.origin = p_particle.position + (use_world_space ? p_particle.creation_position : Vector3());
	const bool has_position_offset = position_offset != Vector3();
	if (has_position_offset || offset_over_lifetime.is_valid()) {
		const float sample_time = play_in_reverse ? (1.0f - p_normalized) : p_normalized;
		const float offset_amount = offset_over_lifetime.is_valid() ? offset_over_lifetime->sample(CLAMP(sample_time, 0.0f, 1.0f)) : 1.0f;
		Vector3 offset;
		if (has_position_offset) {
			offset = position_offset * offset_amount;
		} else {
			// If no explicit offset vector is set, use the particle's emission direction as the offset axis.
			offset = p_particle.direction.normalized() * offset_amount;
		}
		if (!use_world_space) {
			offset = p_global_basis_inv.xform(offset);
		}
		xform.origin += offset;
	}

	buffer_write[idx + 0] = xform.basis.rows[0][0];
	buffer_write[idx + 1] = xform.basis.rows[0][1];
	buffer_write[idx + 2] = xform.basis.rows[0][2];
	buffer_write[idx + 3] = xform.origin.x;
	buffer_write[idx + 4] = xform.basis.rows[1][0];
	buffer_write[idx + 5] = xform.basis.rows[1][1];
	buffer_write[idx + 6] = xform.basis.rows[1][2];
	buffer_write[idx + 7] = xform.origin.y;
	buffer_write[idx + 8] = xform.basis.rows[2][0];
	buffer_write[idx + 9] = xform.basis.rows[2][1];
	buffer_write[idx + 10] = xform.basis.rows[2][2];
	buffer_write[idx + 11] = xform.origin.z;

	if (override_material.is_valid()) {
		const Color color = _sample_particle_color(p_particle, p_normalized);
		buffer_write[idx + 12] = color.r;
		buffer_write[idx + 13] = color.g;
		buffer_write[idx + 14] = color.b;
		buffer_write[idx + 15] = color.a * _sample_alpha_over_lifetime(p_particle, p_normalized);
	} else {
		// Shared-material shaders use:
		// COLOR.r = hue offset or random color factor, COLOR.g = normalized lifetime, COLOR.b = alpha over lifetime.
		const bool uses_random_color_factor = use_start_color_gradient || color_over_lifetime_use_two_gradients;
		buffer_write[idx + 12] = uses_random_color_factor ? p_particle.random_a : p_particle.hue_offset;
		buffer_write[idx + 13] = CLAMP(p_normalized, 0.0f, 1.0f);
		buffer_write[idx + 14] = _sample_alpha_over_lifetime(p_particle, p_normalized);
		buffer_write[idx + 15] = -1.0f;

		if (texture_sheet_enabled) {
			float frame_time = frame_over_time.is_valid() ? frame_over_time->sample(p_normalized) : p_normalized;
			frame_time *= animation_cycles;
			const int total_frames = MAX(1, h_frames * v_frames);
			if (tiles_mode == TEXTURE_SHEET_TILES_WHOLE_SHEET) {
				float absolute_frame = (float)CLAMP(p_particle.tilesheet_starting_tile, 0, total_frames - 1);
				if (animation_cycles > 0.001f) {
					absolute_frame = Math::fposmod(frame_time * (float)total_frames + absolute_frame, (float)total_frames);
				}
				buffer_write[idx + 15] = absolute_frame / (float)total_frames;
			} else {
				const int row = CLAMP(p_particle.tilesheet_starting_tile, 0, MAX(0, v_frames - 1));
				float frame_progress = 0.0f;
				if (animation_cycles > 0.001f) {
					frame_progress = Math::fposmod(frame_time, 1.0f);
				}
				buffer_write[idx + 15] = (float)row + frame_progress;
			}
		}
	}

	// Match the original UniParticles shader contract:
	// INSTANCE_CUSTOM.x = angle, .y = scale_x, .z = scale_y, .w = stretch.
	buffer_write[idx + 16] = (!shader_driven_transform && custom_mesh.is_valid()) ? 0.0f : particle_rotation.z;
	buffer_write[idx + 17] = scale.x;
	buffer_write[idx + 18] = scale.y;
	buffer_write[idx + 19] = stretch;
}

void YParticles3D::_process_simulation_step(double p_raw_delta, double p_scaled_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position) {
	_collision_queries_used_this_frame = 0;
	_emission_time += (float)p_scaled_delta;

	if (_has_last_position && p_raw_delta > 0.000001) {
		_emitter_velocity_world = (p_global_position - _last_position) / (float)p_raw_delta;
	} else {
		_emitter_velocity_world = Vector3();
	}

	if (_emission_time < 0.0f) {
		_last_position = p_global_position;
		_has_last_position = true;
		return;
	}

	_time += (float)p_scaled_delta;

	int alive_count = 0;
	int last_alive_index = -1;
	for (int i = 0; i < _particles.size(); i++) {
		Particle &particle = _particles.write[i];
		if (particle.dead) {
			continue;
		}
		TrailState &trail_state = _trail_states.write[i];
		_trim_trail_points(trail_state);
		if (!particle.trail_only) {
			float particle_time = (_time - particle.creation_time) / particle.lifetime;
			if (play_in_reverse) {
				particle_time = 1.0f - particle_time;
			}
			if (particle_time < 0.0f || particle_time > 1.0f) {
				const Vector3 death_world_position = _get_particle_world_position(particle, p_global_basis, p_global_position);
				_trigger_sub_emitters(SUB_EMITTER_CONDITION_DEATH, particle, CLAMP(particle_time, 0.0f, 1.0f), death_world_position);
				if (particle.trail_enabled && !trail_die_with_particles && !trail_state.points.is_empty()) {
					particle.visible = false;
					particle.trail_only = true;
				} else {
					particle.dead = true;
					trail_state.clear();
					continue;
				}
			}
		} else if (trail_state.points.is_empty()) {
			particle.dead = true;
			continue;
		}
		alive_count++;
		last_alive_index = i;
	}

	if (duration > 0.0f && _emission_time >= duration) {
		if (loop) {
			_emission_time = -start_delay;
			_emission_accumulator = Vector2();
			_restart_bursts();
		} else if (alive_count == 0) {
			_finish();
			return;
		}
	}

	const bool should_emit = emitting && (duration <= 0.0f || _emission_time < duration);
	if (should_emit) {
		const float sampled_rate_over_time = _sample_emission_rate();
		if (sampled_rate_over_time > 0.0001f) {
			const float time_per_particle = 1.0f / sampled_rate_over_time;
			_emission_accumulator.x += (float)p_scaled_delta;
			int emission_count = 0;
			while (_emission_accumulator.x >= time_per_particle && emission_count < max_emissions_per_frame) {
				_emission_accumulator.x -= time_per_particle;
				_emit_particle();
				emission_count++;
			}
		}
	}

	if (should_emit && rate_over_distance > 0.001f) {
		if (_has_last_position) {
			const float distance_moved = p_global_position.distance_to(_last_position);
			_emission_accumulator.y += rate_over_distance * distance_moved;
			const int to_emit = MIN((int)Math::floor(_emission_accumulator.y), max_emissions_per_frame);
			_emission_accumulator.y -= to_emit;

			const Vector3 move = p_global_position - _last_position;
			for (int i = 0; i < to_emit; i++) {
				const float t = to_emit <= 1 ? 0.0f : (float)i / (float)(to_emit - 1);
				_emit_particle(0.0f, true, _last_position + move * t);
			}
		}
	}

	_last_position = p_global_position;
	_has_last_position = true;

	for (int i = 0; i < _active_bursts.size(); i++) {
		int burst_start = 0;
		int burst_total = 0;
		const int to_emit = _active_bursts.write[i].process(_emission_time, _rng.ptr(), burst_start, burst_total);
		for (int emitted = 0; emitted < to_emit; emitted++) {
			const float burst_spot = burst_total <= 1 ? 0.0f : (float)(burst_start + emitted) / (float)(burst_total - 1);
			_emit_particle(burst_spot);
		}
	}

	last_alive_index = _particles.size() - 1;
	int write_index = 0;
	for (int i = 0; i <= last_alive_index; i++) {
		Particle &particle = _particles.write[i];
		if (particle.dead) {
			continue;
		}

		float particle_time = (_time - particle.creation_time) / particle.lifetime;
		if (play_in_reverse) {
			particle_time = 1.0f - particle_time;
		}
		if (!particle.trail_only) {
			particle.index = write_index;
			_update_particle(particle, particle_time, p_raw_delta, p_global_basis, p_global_basis_inv, p_global_position);
			if (particle.dead) {
				continue;
			}
			if (particle.visible) {
				_write_particle_to_buffer(particle, particle_time, p_global_basis, p_global_basis_inv);
				write_index++;
			}
			if (particle.trail_enabled) {
				_append_trail_point(particle, _trail_states.write[i], particle_time, p_global_basis, p_global_basis_inv, p_global_position);
			}
		} else {
			_trim_trail_points(_trail_states.write[i]);
			if (_trail_states[i].points.is_empty()) {
				particle.dead = true;
			}
		}
	}

	if (_multimesh.is_valid()) {
		if (_visible_count != write_index) {
			_visible_count = write_index;
			RenderingServer::get_singleton()->multimesh_set_visible_instances(_multimesh, _visible_count);
		}
		if (write_index > 0) {
			RenderingServer::get_singleton()->multimesh_set_buffer(_multimesh, _buffer);
		}
	}
	if (_trail_mesh.is_valid()) {
		_update_trail_mesh(p_global_basis, p_global_position);
	}
}

void YParticles3D::_update_trail_mesh(const Basis &p_global_basis, const Vector3 &p_global_position) {
	if (!_trail_mesh.is_valid()) {
		return;
	}

	(void)p_global_basis;
	(void)p_global_position;

	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedColorArray colors;
	PackedVector2Array uvs;
	PackedInt32Array indices;
	int vertex_offset = 0;
	int trail_count_with_segments = 0;
	int trail_count_with_points = 0;
	int max_points_in_trail = 0;

	for (int particle_index = 0; particle_index < _trail_states.size(); particle_index++) {
		const TrailState &trail_state = _trail_states[particle_index];
		if (!trail_state.points.is_empty()) {
			trail_count_with_points++;
			max_points_in_trail = MAX(max_points_in_trail, trail_state.points.size());
		}
		if (trail_state.points.size() < 2) {
			continue;
		}
		trail_count_with_segments++;

		float total_length = 0.0f;
		for (int point_index = 1; point_index < trail_state.points.size(); point_index++) {
			total_length += trail_state.points[point_index - 1].position.distance_to(trail_state.points[point_index].position);
		}
		total_length = MAX(total_length, 0.0001f);

		float distance_accum = 0.0f;
		for (int point_index = 0; point_index < trail_state.points.size(); point_index++) {
			const TrailPoint &point = trail_state.points[point_index];
			const float over_trail = trail_state.points.size() <= 1 ? 0.0f : (float)point_index / (float)(trail_state.points.size() - 1);
			const float head_ratio = 1.0f - over_trail;

			float width = 1.0f;
			if (trail_width_over_trail.is_valid()) {
				width = trail_width_over_trail->sample(CLAMP(head_ratio, 0.0f, 1.0f));
			}
			if (trail_size_affects_width) {
				width *= point.size;
			}
			width = MAX(width, 0.0001f);

			Vector3 tangent;
			if (point_index == 0) {
				tangent = (trail_state.points[1].position - point.position).normalized();
			} else if (point_index == trail_state.points.size() - 1) {
				tangent = (point.position - trail_state.points[point_index - 1].position).normalized();
			} else {
				const Vector3 to_prev = (point.position - trail_state.points[point_index - 1].position).normalized();
				const Vector3 to_next = (trail_state.points[point_index + 1].position - point.position).normalized();
				tangent = (to_prev + to_next).normalized();
			}
			if (tangent.length_squared() <= 0.000001f) {
				tangent = Vector3(0.0f, 1.0f, 0.0f);
			}

			Color vertex_color = Color(1, 1, 1, 1);
			if (trail_color_over_lifetime.is_valid() && trail_color_over_lifetime->get_gradient().is_valid()) {
				vertex_color *= trail_color_over_lifetime->get_gradient()->sample(CLAMP(point.particle_normalized, 0.0f, 1.0f));
			}
			if (trail_color_over_trail.is_valid() && trail_color_over_trail->get_gradient().is_valid()) {
				vertex_color *= trail_color_over_trail->get_gradient()->sample(CLAMP(head_ratio, 0.0f, 1.0f));
			}
			if (trail_inherit_particle_color) {
				vertex_color *= point.particle_color;
			}

			float u = 0.0f;
			switch (trail_texture_mode) {
				case TRAIL_TEXTURE_MODE_TILE:
					u = distance_accum;
					break;
				case TRAIL_TEXTURE_MODE_REPEAT_PER_SEGMENT:
					u = (float)point_index;
					break;
				case TRAIL_TEXTURE_MODE_DISTRIBUTE_PER_SEGMENT:
					u = over_trail;
					break;
				case TRAIL_TEXTURE_MODE_STRETCH:
				default:
					u = distance_accum / total_length;
					break;
			}

			const float half_width = width * 0.5f;
			vertices.push_back(point.position);
			vertices.push_back(point.position);
			vertices.push_back(point.position);
			normals.push_back(tangent);
			normals.push_back(tangent);
			normals.push_back(tangent);
			colors.push_back(vertex_color);
			colors.push_back(vertex_color);
			colors.push_back(vertex_color);
			uvs.push_back(Vector2(u, -half_width));
			uvs.push_back(Vector2(u, 0.0f));
			uvs.push_back(Vector2(u, half_width));

			if (point_index > 0) {
				const int base = vertex_offset + (point_index - 1) * 3;
				indices.push_back(base);
				indices.push_back(base + 3);
				indices.push_back(base + 1);
				indices.push_back(base + 1);
				indices.push_back(base + 3);
				indices.push_back(base + 4);
				indices.push_back(base + 1);
				indices.push_back(base + 4);
				indices.push_back(base + 2);
				indices.push_back(base + 2);
				indices.push_back(base + 4);
				indices.push_back(base + 5);
			}

			if (point_index + 1 < trail_state.points.size()) {
				distance_accum += point.position.distance_to(trail_state.points[point_index + 1].position);
			}
		}

		vertex_offset += trail_state.points.size() * 3;
	}

	RenderingServer::get_singleton()->mesh_clear(_trail_mesh);
	if (debugging && _time >= _trail_debug_next_time) {
		_trail_debug_next_time = _time + 0.5f;
		print_line(vformat("[YParticles3D Trails] %s points=%d segmented=%d max_points=%d vertices=%d indices=%d",
				get_name(),
				trail_count_with_points,
				trail_count_with_segments,
				max_points_in_trail,
				vertices.size(),
				indices.size()));
	}
	if (vertices.is_empty()) {
		return;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	RenderingServer::get_singleton()->mesh_add_surface_from_arrays(_trail_mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
	if (_trail_material.is_valid()) {
		RenderingServer::get_singleton()->mesh_surface_set_material(_trail_mesh, 0, _trail_material->get_rid());
	}
}

YParticles3D::BurstInstance YParticles3D::_create_burst_instance(int p_index) const {
	BurstInstance instance;
	if (p_index < 0 || p_index >= bursts.size()) {
		return instance;
	}
	const Variant burst_variant = bursts[p_index];
	if (burst_variant.get_type() == Variant::DICTIONARY) {
		const Dictionary burst = burst_variant;
		instance.time = (float)(double)burst.get("time", 0.0);
		const int count_mode = (int)burst.get("count_mode", 0);
		instance.min_particles = MAX(0, (int)burst.get("min_count", 0));
		instance.max_particles = count_mode == 1 ? MAX(instance.min_particles, (int)burst.get("max_count", instance.min_particles)) : instance.min_particles;
		const int cycle_mode = (int)burst.get("cycle_mode", 0);
		instance.min_cycles = MAX(0, (int)burst.get("min_cycles", 1));
		instance.max_cycles = cycle_mode == 1 ? MAX(instance.min_cycles, (int)burst.get("max_cycles", instance.min_cycles)) : instance.min_cycles;
		instance.particle_interval = MAX(0.0f, (float)(double)burst.get("interval", 0.0));
		instance.probability = CLAMP((float)(double)burst.get("probability", 1.0), 0.0f, 1.0f);
		return instance;
	}

	const int base_idx = p_index * 9;
	if (base_idx + 8 >= bursts.size()) {
		return instance;
	}
	instance.time = (float)bursts[base_idx];
	const int count_mode = (int)bursts[base_idx + 1];
	instance.min_particles = MAX(0, (int)bursts[base_idx + 2]);
	instance.max_particles = count_mode == 1 ? MAX(instance.min_particles, (int)bursts[base_idx + 3]) : instance.min_particles;
	const int cycle_mode = (int)bursts[base_idx + 4];
	instance.min_cycles = MAX(0, (int)bursts[base_idx + 5]);
	instance.max_cycles = cycle_mode == 1 ? MAX(instance.min_cycles, (int)bursts[base_idx + 6]) : instance.min_cycles;
	instance.particle_interval = MAX(0.0f, (float)bursts[base_idx + 7]);
	instance.probability = CLAMP((float)bursts[base_idx + 8], 0.0f, 1.0f);
	return instance;
}

void YParticles3D::_restart_bursts() {
	_active_bursts.clear();
	const int burst_count = bursts.is_empty() ? 0 : (bursts[0].get_type() == Variant::DICTIONARY ? bursts.size() : bursts.size() / 9);
	for (int i = 0; i < burst_count; i++) {
		BurstInstance burst = _create_burst_instance(i);
		burst.initialize(_rng.ptr());
		_active_bursts.push_back(burst);
	}
}

void YParticles3D::_finish() {
	_playing = false;
	set_process(false);
	_visible_count = 0;
	if (_multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_visible_instances(_multimesh, 0);
	}
	emit_signal("finished_burst");
	if (_spawned_as_sub_emitter_instance) {
		queue_free();
	} else if (destroy_on_finish && !Engine::get_singleton()->is_editor_hint()) {
		queue_free();
	}
}

void YParticles3D::_mark_core_dirty() {
	_core_params_dirty = true;
	_yparticles3d_update_gizmos_if_editor(this);
}

void YParticles3D::_mark_material_dirty() {
	_material_dirty = true;
	_yparticles3d_update_gizmos_if_editor(this);
}

YParticles3D::YParticles3D() {
	_rng.instantiate();
	set_notify_transform(true);
	set_process(false);
}

YParticles3D::~YParticles3D() {
	_cleanup_spawned_sub_emitter_instances();
	_free_rendering();
}

void YParticles3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (_core_params_dirty || !_multimesh.is_valid()) {
				_create_multimesh();
				_core_params_dirty = false;
			}
			if (play_on_start && !Engine::get_singleton()->is_editor_hint() && !_is_sub_emitter_template_for_parent()) {
				play();
			}
			break;
		case NOTIFICATION_PROCESS: {
			if (!_playing || paused) {
				break;
			}
			if (_particles.is_empty() && !emitting && _active_bursts.is_empty()) {
				break;
			}

			if (_core_params_dirty) {
				_create_multimesh();
				_core_params_dirty = false;
			}
			if (_material_dirty) {
				_update_material();
				_update_trail_material();
				_update_material_shader_params();
				_material_dirty = false;
			}
			const Transform3D global_transform = get_global_transform();
			const Basis global_basis = global_transform.basis;
			const Basis global_basis_inv = global_basis.inverse();
			const Vector3 global_position = global_transform.origin;
			if (global_transform != _last_transform) {
				_update_instance_transform();
			}

			double raw_delta = get_process_delta_time();
			if (fixed_fps > 0) {
				if (raw_delta > 0.1) {
					raw_delta = 0.1;
				} else if (raw_delta <= 0.0) {
					raw_delta = 0.001;
				}

				const double raw_step = 1.0 / (double)fixed_fps;
				_fixed_fps_remainder += raw_delta;
				while (_fixed_fps_remainder >= raw_step) {
					_process_simulation_step(raw_step, raw_step * playback_speed, global_basis, global_basis_inv, global_position);
					_fixed_fps_remainder -= raw_step;
					if (!_playing) {
						break;
					}
				}
			} else {
				_process_simulation_step(raw_delta, raw_delta * playback_speed, global_basis, global_basis_inv, global_position);
			}
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED:
			if (!use_world_space) {
				_update_instance_transform();
			}
			if (!use_world_space || !trail_world_space) {
				_update_trail_instance_transform();
			}
			break;
		case NOTIFICATION_VISIBILITY_CHANGED:
			if (_instance.is_valid()) {
				RenderingServer::get_singleton()->instance_set_visible(_instance, is_visible_in_tree());
			}
			if (_trail_instance.is_valid()) {
				RenderingServer::get_singleton()->instance_set_visible(_trail_instance, is_visible_in_tree());
			}
			break;
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_PREDELETE:
			_has_last_position = false;
			_free_rendering();
			break;
	}
}

void YParticles3D::play(bool p_clear_on_play) {
	if (!is_inside_tree()) {
		return;
	}

	if (_spawned_as_sub_emitter_instance) {
		loop = false;
		destroy_on_finish = true;
	}

	_cleanup_spawned_sub_emitter_instances();

	if (enable_noise && _noise_generator.is_null()) {
		_noise_generator.instantiate();
		_noise_generator->set_noise_type(FastNoiseLite::TYPE_SIMPLEX);
		_noise_generator->set_fractal_type(FastNoiseLite::FRACTAL_NONE);
		_noise_generator->set_seed(1337);
	}

	if (_core_params_dirty || !_multimesh.is_valid()) {
		_create_multimesh();
		_core_params_dirty = false;
	}

	if (p_clear_on_play) {
		clear(false);
	}

	_playing = true;
	_time = MAX(0.0f, start_delay_percentage * duration);
	_emission_time = -start_delay + (start_delay_percentage * duration);
	_fixed_fps_remainder = 0.0;
	_emission_accumulator = Vector2();
	_current_arc_rotation = 0.0f;
	_arc_direction = 1;
	_collision_plane_cache.clear();
	if (_pending_sub_emitter_inherit && _pending_sub_emitter_inherit_duration) {
		_time = _pending_sub_emitter_source_time;
		_emission_time = _pending_sub_emitter_source_emission_time;
	}
	_last_position = get_global_position();
	_has_last_position = false;
	_last_transform = get_global_transform();
	_clear_trails();
	_restart_bursts();
	set_process(true);

	for (int i = 0; i < get_child_count(); i++) {
		YParticles3D *child_particles = Object::cast_to<YParticles3D>(get_child(i));
		if (child_particles != nullptr) {
			child_particles->set_loop(loop);
			if (!child_particles->_playing && !_is_registered_sub_emitter_node(child_particles)) {
				child_particles->play(p_clear_on_play);
			}
		}
	}
}

void YParticles3D::stop(bool p_clear) {
	_playing = false;
	_active_bursts.clear();
	set_process(false);
	_cleanup_spawned_sub_emitter_instances();

	for (int i = 0; i < get_child_count(); i++) {
		YParticles3D *child_particles = Object::cast_to<YParticles3D>(get_child(i));
		if (child_particles != nullptr) {
			child_particles->stop(p_clear);
		}
	}

	if (p_clear) {
		clear(false);
	}
}

void YParticles3D::clear(bool p_stop) {
	if (p_stop) {
		stop(false);
	}

	for (int i = 0; i < _particles.size(); i++) {
		_particles.write[i].dead = true;
	}
	_clear_trails();
	_active_bursts.clear();
	_collision_plane_cache.clear();
	_cleanup_spawned_sub_emitter_instances();
	_visible_count = 0;
	if (_multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_visible_instances(_multimesh, 0);
	}
	_clear_trails();
}

#define SIMPLE_SETGET(type, name, dirty_call) \
	void YParticles3D::set_##name(type p_value) { name = p_value; dirty_call; } \
	type YParticles3D::get_##name() const { return name; }

SIMPLE_SETGET(float, duration, _mark_core_dirty())
void YParticles3D::set_start_lifetime_mode(int p_value) { start_lifetime_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_start_lifetime_mode() const { return start_lifetime_mode; }
SIMPLE_SETGET(float, start_lifetime_constant, _mark_core_dirty())
SIMPLE_SETGET(Vector2, start_lifetime_random, _mark_core_dirty())
void YParticles3D::set_start_lifetime_curve(const Ref<Curve> &p_value) { start_lifetime_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_lifetime_curve() const { return start_lifetime_curve; }
void YParticles3D::set_start_speed_mode(int p_value) { start_speed_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_start_speed_mode() const { return start_speed_mode; }
SIMPLE_SETGET(float, start_speed_constant, _mark_core_dirty())
SIMPLE_SETGET(Vector2, start_speed_random, _mark_core_dirty())
SIMPLE_SETGET(Vector3, gravity, _mark_core_dirty())
void YParticles3D::set_start_size_mode(int p_value) { start_size_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_start_size_mode() const { return start_size_mode; }
SIMPLE_SETGET(Vector2, start_size_constant, _mark_core_dirty())
SIMPLE_SETGET(Vector4, start_size_random, _mark_core_dirty())
void YParticles3D::set_start_size_curve(const Ref<Curve> &p_value) { start_size_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_curve() const { return start_size_curve; }
void YParticles3D::set_start_size_curve_min(const Ref<Curve> &p_value) { start_size_curve_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_curve_min() const { return start_size_curve_min; }
void YParticles3D::set_start_size_curve_max(const Ref<Curve> &p_value) { start_size_curve_max = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_curve_max() const { return start_size_curve_max; }
void YParticles3D::set_start_size_square_random(Vector2 p_value) { start_size_square_random = p_value; _mark_core_dirty(); }
Vector2 YParticles3D::get_start_size_square_random() const { return start_size_square_random; }
SIMPLE_SETGET(Vector3, start_size_constant_3d, _mark_core_dirty())
SIMPLE_SETGET(Vector3, start_size_random_min_3d, _mark_core_dirty())
SIMPLE_SETGET(Vector3, start_size_random_max_3d, _mark_core_dirty())
void YParticles3D::set_start_size_x_curve(const Ref<Curve> &p_value) { start_size_x_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_x_curve() const { return start_size_x_curve; }
void YParticles3D::set_start_size_y_curve(const Ref<Curve> &p_value) { start_size_y_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_y_curve() const { return start_size_y_curve; }
void YParticles3D::set_start_size_z_curve(const Ref<Curve> &p_value) { start_size_z_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_z_curve() const { return start_size_z_curve; }
void YParticles3D::set_start_size_x_curve_min(const Ref<Curve> &p_value) { start_size_x_curve_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_x_curve_min() const { return start_size_x_curve_min; }
void YParticles3D::set_start_size_y_curve_min(const Ref<Curve> &p_value) { start_size_y_curve_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_y_curve_min() const { return start_size_y_curve_min; }
void YParticles3D::set_start_size_z_curve_min(const Ref<Curve> &p_value) { start_size_z_curve_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_size_z_curve_min() const { return start_size_z_curve_min; }
void YParticles3D::set_start_rotation_degrees_mode(int p_value) { start_rotation_degrees_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_start_rotation_degrees_mode() const { return start_rotation_degrees_mode; }
SIMPLE_SETGET(float, start_rotation_degrees_constant, _mark_core_dirty())
SIMPLE_SETGET(Vector2, start_rotation_degrees_random, _mark_core_dirty())
void YParticles3D::set_start_rotation_degrees_curve(const Ref<Curve> &p_value) { start_rotation_degrees_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_rotation_degrees_curve() const { return start_rotation_degrees_curve; }
SIMPLE_SETGET(Vector3, start_rotation_degrees_constant_3d, _mark_core_dirty())
SIMPLE_SETGET(Vector3, start_rotation_degrees_random_min_3d, _mark_core_dirty())
SIMPLE_SETGET(Vector3, start_rotation_degrees_random_max_3d, _mark_core_dirty())
void YParticles3D::set_start_rotation_degrees_x_curve(const Ref<Curve> &p_value) { start_rotation_degrees_x_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_rotation_degrees_x_curve() const { return start_rotation_degrees_x_curve; }
void YParticles3D::set_start_rotation_degrees_y_curve(const Ref<Curve> &p_value) { start_rotation_degrees_y_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_rotation_degrees_y_curve() const { return start_rotation_degrees_y_curve; }
void YParticles3D::set_start_rotation_degrees_z_curve(const Ref<Curve> &p_value) { start_rotation_degrees_z_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_start_rotation_degrees_z_curve() const { return start_rotation_degrees_z_curve; }
void YParticles3D::set_use_world_space(bool p_value) { use_world_space = p_value; _mark_core_dirty(); }
bool YParticles3D::is_using_world_space() const { return use_world_space; }
SIMPLE_SETGET(bool, play_on_start, _mark_core_dirty())
SIMPLE_SETGET(bool, loop, _mark_core_dirty())
SIMPLE_SETGET(bool, play_in_reverse, _mark_core_dirty())
void YParticles3D::set_start_delay(float p_value) { start_delay = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
float YParticles3D::get_start_delay() const { return start_delay; }
SIMPLE_SETGET(float, start_delay_percentage, _mark_core_dirty())
SIMPLE_SETGET(bool, destroy_on_finish, _mark_core_dirty())
SIMPLE_SETGET(bool, debugging, _mark_core_dirty())
void YParticles3D::set_emitting(bool p_value) { emitting = p_value; _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::is_emitting() const { return emitting; }
void YParticles3D::set_max_particles(int p_value) { max_particles = MAX(1, p_value); _mark_core_dirty(); }
int YParticles3D::get_max_particles() const { return max_particles; }
void YParticles3D::set_max_emissions_per_frame(int p_value) { max_emissions_per_frame = MAX(1, p_value); _mark_core_dirty(); }
int YParticles3D::get_max_emissions_per_frame() const { return max_emissions_per_frame; }
void YParticles3D::set_rate_over_time_mode(int p_value) { rate_over_time_mode = CLAMP(p_value, 0, 1); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_rate_over_time_mode() const { return rate_over_time_mode; }
SIMPLE_SETGET(float, rate_over_time, _mark_core_dirty())
void YParticles3D::set_rate_over_time_curve(const Ref<Curve> &p_value) { rate_over_time_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rate_over_time_curve() const { return rate_over_time_curve; }
SIMPLE_SETGET(float, rate_over_distance, _mark_core_dirty())
void YParticles3D::set_bursts(const Array &p_value) { bursts = p_value.duplicate(true); _mark_core_dirty(); }
Array YParticles3D::get_bursts() const { return bursts; }
void YParticles3D::set_enable_shape(bool p_value) { enable_shape = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_shape() const { return enable_shape; }
void YParticles3D::set_shape_type(EmissionShape p_value) { shape_type = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::EmissionShape YParticles3D::get_shape_type() const { return shape_type; }
SIMPLE_SETGET(float, radius, _mark_core_dirty())
SIMPLE_SETGET(float, radius_thickness, _mark_core_dirty())
SIMPLE_SETGET(float, angle, _mark_core_dirty())
SIMPLE_SETGET(Vector3, box_extents, _mark_core_dirty())
void YParticles3D::set_emission_mesh(const Ref<Mesh> &p_value) {
	emission_mesh = p_value;
	_emission_mesh_cache_dirty = true;
	_mark_core_dirty();
}
Ref<Mesh> YParticles3D::get_emission_mesh() const { return emission_mesh; }
void YParticles3D::set_emission_mesh_scale(Vector3 p_value) {
	emission_mesh_scale = p_value;
	_emission_mesh_cache_dirty = true;
	_mark_core_dirty();
}
Vector3 YParticles3D::get_emission_mesh_scale() const { return emission_mesh_scale; }
SIMPLE_SETGET(float, random_direction, _mark_core_dirty())
SIMPLE_SETGET(float, spherize_direction, _mark_core_dirty())
void YParticles3D::set_emit_from(EmitFrom p_value) { emit_from = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::EmitFrom YParticles3D::get_emit_from() const { return emit_from; }
SIMPLE_SETGET(float, shape_length, _mark_core_dirty())
SIMPLE_SETGET(float, arc_degrees, _mark_core_dirty())
void YParticles3D::set_arc_mode(ArcMode p_value) { arc_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::ArcMode YParticles3D::get_arc_mode() const { return arc_mode; }
void YParticles3D::set_arc_spread(float p_value) { arc_spread = CLAMP(p_value, 0.0f, 1.0f); _mark_core_dirty(); }
float YParticles3D::get_arc_spread() const { return arc_spread; }
void YParticles3D::set_arc_speed_mode(int p_value) { arc_speed_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_arc_speed_mode() const { return arc_speed_mode; }
SIMPLE_SETGET(float, arc_speed_constant, _mark_core_dirty())
void YParticles3D::set_arc_speed_curve(const Ref<Curve> &p_value) { arc_speed_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_arc_speed_curve() const { return arc_speed_curve; }
void YParticles3D::set_direction_in_world_space(bool p_value) { direction_in_world_space = p_value; _mark_core_dirty(); }
bool YParticles3D::is_direction_in_world_space() const { return direction_in_world_space; }
void YParticles3D::set_invert_direction(bool p_value) { invert_direction = p_value; _mark_core_dirty(); }
bool YParticles3D::is_direction_inverted() const { return invert_direction; }
SIMPLE_SETGET(Vector3, position_offset, _mark_core_dirty())
SIMPLE_SETGET(Vector3, rotation_offset, _mark_core_dirty())
void YParticles3D::set_enable_size_over_lifetime(bool p_value) { enable_size_over_lifetime = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_size_over_lifetime() const { return enable_size_over_lifetime; }
void YParticles3D::set_size_over_lifetime(const Ref<Curve> &p_value) { size_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_size_over_lifetime() const { return size_over_lifetime; }
void YParticles3D::set_size_over_lifetime_min(const Ref<Curve> &p_value) { size_over_lifetime_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_size_over_lifetime_min() const { return size_over_lifetime_min; }
void YParticles3D::set_width_over_lifetime(const Ref<Curve> &p_value) { width_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_width_over_lifetime() const { return width_over_lifetime; }
void YParticles3D::set_height_over_lifetime(const Ref<Curve> &p_value) { height_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_height_over_lifetime() const { return height_over_lifetime; }
void YParticles3D::set_depth_over_lifetime(const Ref<Curve> &p_value) { depth_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_depth_over_lifetime() const { return depth_over_lifetime; }
SIMPLE_SETGET(bool, size_over_lifetime_use_two_curves, _mark_core_dirty())
void YParticles3D::set_enable_velocity_over_lifetime(bool p_value) { enable_velocity_over_lifetime = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_velocity_over_lifetime() const { return enable_velocity_over_lifetime; }
void YParticles3D::set_velocity_over_lifetime_mode(int p_value) { velocity_over_lifetime_mode = CLAMP(p_value, 0, 1); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_velocity_over_lifetime_mode() const { return velocity_over_lifetime_mode; }
void YParticles3D::set_velocity_over_lifetime(const Ref<Curve> &p_value) { velocity_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime() const { return velocity_over_lifetime; }
void YParticles3D::set_velocity_over_lifetime_min(const Ref<Curve> &p_value) { velocity_over_lifetime_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_min() const { return velocity_over_lifetime_min; }
void YParticles3D::set_velocity_over_lifetime_x(const Ref<Curve> &p_value) { velocity_over_lifetime_x = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_x() const { return velocity_over_lifetime_x; }
void YParticles3D::set_velocity_over_lifetime_x_min(const Ref<Curve> &p_value) { velocity_over_lifetime_x_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_x_min() const { return velocity_over_lifetime_x_min; }
void YParticles3D::set_velocity_over_lifetime_y(const Ref<Curve> &p_value) { velocity_over_lifetime_y = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_y() const { return velocity_over_lifetime_y; }
void YParticles3D::set_velocity_over_lifetime_y_min(const Ref<Curve> &p_value) { velocity_over_lifetime_y_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_y_min() const { return velocity_over_lifetime_y_min; }
void YParticles3D::set_velocity_over_lifetime_z(const Ref<Curve> &p_value) { velocity_over_lifetime_z = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_z() const { return velocity_over_lifetime_z; }
void YParticles3D::set_velocity_over_lifetime_z_min(const Ref<Curve> &p_value) { velocity_over_lifetime_z_min = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_velocity_over_lifetime_z_min() const { return velocity_over_lifetime_z_min; }
void YParticles3D::set_offset_over_lifetime(const Ref<Curve> &p_value) { offset_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_offset_over_lifetime() const { return offset_over_lifetime; }
SIMPLE_SETGET(bool, velocity_over_lifetime_use_two_curves, _mark_core_dirty())
SIMPLE_SETGET(bool, velocity_in_world_space, _mark_core_dirty())
void YParticles3D::set_enable_force_over_lifetime(bool p_value) { enable_force_over_lifetime = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_force_over_lifetime() const { return enable_force_over_lifetime; }
void YParticles3D::set_force_over_lifetime_mode(int p_value) { force_over_lifetime_mode = CLAMP(p_value, 0, 3); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_force_over_lifetime_mode() const { return force_over_lifetime_mode; }
void YParticles3D::set_force_over_lifetime(const Ref<Curve> &p_value) { force_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_force_over_lifetime() const { return force_over_lifetime; }
void YParticles3D::set_force_over_lifetime_x(const Ref<Curve> &p_value) { force_over_lifetime_x = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_force_over_lifetime_x() const { return force_over_lifetime_x; }
void YParticles3D::set_force_over_lifetime_y(const Ref<Curve> &p_value) { force_over_lifetime_y = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_force_over_lifetime_y() const { return force_over_lifetime_y; }
void YParticles3D::set_force_over_lifetime_z(const Ref<Curve> &p_value) { force_over_lifetime_z = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_force_over_lifetime_z() const { return force_over_lifetime_z; }
SIMPLE_SETGET(Vector3, force_over_lifetime_constant, _mark_core_dirty())
SIMPLE_SETGET(Vector3, force_over_lifetime_random_min, _mark_core_dirty())
SIMPLE_SETGET(Vector3, force_over_lifetime_random_max, _mark_core_dirty())
SIMPLE_SETGET(bool, force_in_world_space, _mark_core_dirty())
void YParticles3D::set_enable_limit_velocity_over_lifetime(bool p_value) { enable_limit_velocity_over_lifetime = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_limit_velocity_over_lifetime() const { return enable_limit_velocity_over_lifetime; }
void YParticles3D::set_limit_velocity_over_lifetime_separate_axis(bool p_value) { limit_velocity_over_lifetime_speed_mode = p_value ? MAX(2, limit_velocity_over_lifetime_speed_mode) : MIN(1, limit_velocity_over_lifetime_speed_mode); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_limit_velocity_over_lifetime_separate_axis() const { return limit_velocity_over_lifetime_speed_mode >= 2; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_mode(int p_value) { limit_velocity_over_lifetime_speed_mode = CLAMP(p_value, 0, 3); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_limit_velocity_over_lifetime_speed_mode() const { return limit_velocity_over_lifetime_speed_mode; }
void YParticles3D::set_limit_velocity_over_lifetime_speed(float p_value) { limit_velocity_over_lifetime_speed = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_limit_velocity_over_lifetime_speed() const { return limit_velocity_over_lifetime_speed; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_curve(const Ref<Curve> &p_value) { limit_velocity_over_lifetime_speed_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_limit_velocity_over_lifetime_speed_curve() const { return limit_velocity_over_lifetime_speed_curve; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_axis(Vector3 p_value) {
	limit_velocity_over_lifetime_speed_axis = Vector3(MAX(0.0f, p_value.x), MAX(0.0f, p_value.y), MAX(0.0f, p_value.z));
	_mark_core_dirty();
}
Vector3 YParticles3D::get_limit_velocity_over_lifetime_speed_axis() const { return limit_velocity_over_lifetime_speed_axis; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_x_curve(const Ref<Curve> &p_value) { limit_velocity_over_lifetime_speed_x_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_limit_velocity_over_lifetime_speed_x_curve() const { return limit_velocity_over_lifetime_speed_x_curve; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_y_curve(const Ref<Curve> &p_value) { limit_velocity_over_lifetime_speed_y_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_limit_velocity_over_lifetime_speed_y_curve() const { return limit_velocity_over_lifetime_speed_y_curve; }
void YParticles3D::set_limit_velocity_over_lifetime_speed_z_curve(const Ref<Curve> &p_value) { limit_velocity_over_lifetime_speed_z_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_limit_velocity_over_lifetime_speed_z_curve() const { return limit_velocity_over_lifetime_speed_z_curve; }
void YParticles3D::set_limit_velocity_over_lifetime_dampen(float p_value) { limit_velocity_over_lifetime_dampen = CLAMP(p_value, 0.0f, 1.0f); _mark_core_dirty(); }
float YParticles3D::get_limit_velocity_over_lifetime_dampen() const { return limit_velocity_over_lifetime_dampen; }
void YParticles3D::set_enable_noise(bool p_value) { enable_noise = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_noise() const { return enable_noise; }
SIMPLE_SETGET(float, noise_strength, _mark_core_dirty())
void YParticles3D::set_noise_strength_mode(int p_value) { noise_strength_mode = CLAMP(p_value, 0, 2); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_noise_strength_mode() const { return noise_strength_mode; }
void YParticles3D::set_noise_strength_curve(const Ref<Curve> &p_value) { noise_strength_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_noise_strength_curve() const { return noise_strength_curve; }
void YParticles3D::set_noise_strength_x(const Ref<Curve> &p_value) { noise_strength_x = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_noise_strength_x() const { return noise_strength_x; }
void YParticles3D::set_noise_strength_y(const Ref<Curve> &p_value) { noise_strength_y = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_noise_strength_y() const { return noise_strength_y; }
void YParticles3D::set_noise_strength_z(const Ref<Curve> &p_value) { noise_strength_z = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_noise_strength_z() const { return noise_strength_z; }
void YParticles3D::set_noise_scale(float p_value) { noise_scale = MAX(0.001f, p_value); _mark_core_dirty(); }
float YParticles3D::get_noise_scale() const { return noise_scale; }
SIMPLE_SETGET(Vector3, noise_scroll_speed, _mark_core_dirty())
SIMPLE_SETGET(float, noise_position_amount, _mark_core_dirty())
SIMPLE_SETGET(float, noise_rotation_amount, _mark_core_dirty())
SIMPLE_SETGET(float, noise_size_amount, _mark_core_dirty())
void YParticles3D::set_noise_octaves(int p_value) { noise_octaves = MAX(1, p_value); _mark_core_dirty(); }
int YParticles3D::get_noise_octaves() const { return noise_octaves; }
void YParticles3D::set_noise_lacunarity(float p_value) { noise_lacunarity = MAX(1.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_noise_lacunarity() const { return noise_lacunarity; }
void YParticles3D::set_enable_attractor(bool p_value) { enable_attractor = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_attractor() const { return enable_attractor; }
void YParticles3D::set_attraction_target_mode(int p_value) { attraction_target_mode = CLAMP(p_value, 0, 1); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_attraction_target_mode() const { return attraction_target_mode; }
SIMPLE_SETGET(Vector3, attractor_position, _mark_core_dirty())
void YParticles3D::set_attraction_target(const NodePath &p_value) { attraction_target = p_value; _mark_core_dirty(); }
NodePath YParticles3D::get_attraction_target() const { return attraction_target; }
void YParticles3D::set_attraction_over_lifetime(const Ref<Curve> &p_value) { attraction_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_attraction_over_lifetime() const { return attraction_over_lifetime; }
void YParticles3D::set_enable_collision(bool p_value) { enable_collision = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_collision() const { return enable_collision; }
void YParticles3D::set_collision_layer(uint32_t p_value) { collision_layer = p_value; _mark_core_dirty(); }
uint32_t YParticles3D::get_collision_layer() const { return collision_layer; }
void YParticles3D::set_collision_radius_scale(float p_value) { collision_radius_scale = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_collision_radius_scale() const { return collision_radius_scale; }
void YParticles3D::set_collision_dampen(float p_value) { collision_dampen = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_collision_dampen() const { return collision_dampen; }
void YParticles3D::set_collision_bounce(float p_value) { collision_bounce = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_collision_bounce() const { return collision_bounce; }
void YParticles3D::set_collision_lifetime_loss(float p_value) { collision_lifetime_loss = CLAMP(p_value, 0.0f, 1.0f); _mark_core_dirty(); }
float YParticles3D::get_collision_lifetime_loss() const { return collision_lifetime_loss; }
void YParticles3D::set_collision_min_kill_speed(float p_value) { collision_min_kill_speed = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_collision_min_kill_speed() const { return collision_min_kill_speed; }
void YParticles3D::set_collision_quality(int p_value) { collision_quality = (CollisionQuality)CLAMP(p_value, 0, 2); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_collision_quality() const { return (int)collision_quality; }
void YParticles3D::set_collision_voxel_size(float p_value) { collision_voxel_size = MAX(0.001f, p_value); _mark_core_dirty(); }
float YParticles3D::get_collision_voxel_size() const { return collision_voxel_size; }
void YParticles3D::set_enable_sub_emitters(bool p_value) { enable_sub_emitters = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_sub_emitters() const { return enable_sub_emitters; }
void YParticles3D::set_sub_emitters(const Array &p_value) { sub_emitters = p_value.duplicate(true); _mark_core_dirty(); }
Array YParticles3D::get_sub_emitters() const { return sub_emitters; }
void YParticles3D::set_enable_rotation_over_lifetime(bool p_value) { enable_rotation_over_lifetime = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_rotation_over_lifetime() const { return enable_rotation_over_lifetime; }
void YParticles3D::set_rotation_over_lifetime(const Ref<Curve> &p_value) { rotation_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rotation_over_lifetime() const { return rotation_over_lifetime; }
void YParticles3D::set_rotation_over_lifetime_axis(Vector3 p_value) { rotation_over_lifetime_axis = p_value; _mark_core_dirty(); }
Vector3 YParticles3D::get_rotation_over_lifetime_axis() const { return rotation_over_lifetime_axis; }
void YParticles3D::set_enable_rotation_by_speed(bool p_value) { enable_rotation_by_speed = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_rotation_by_speed() const { return enable_rotation_by_speed; }
void YParticles3D::set_rotation_by_speed_mode(int p_value) { rotation_by_speed_mode = CLAMP(p_value, 0, 1); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_rotation_by_speed_mode() const { return rotation_by_speed_mode; }
void YParticles3D::set_rotation_by_speed(const Ref<Curve> &p_value) { rotation_by_speed = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rotation_by_speed() const { return rotation_by_speed; }
void YParticles3D::set_rotation_by_speed_x(const Ref<Curve> &p_value) { rotation_by_speed_x = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rotation_by_speed_x() const { return rotation_by_speed_x; }
void YParticles3D::set_rotation_by_speed_y(const Ref<Curve> &p_value) { rotation_by_speed_y = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rotation_by_speed_y() const { return rotation_by_speed_y; }
void YParticles3D::set_rotation_by_speed_z(const Ref<Curve> &p_value) { rotation_by_speed_z = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_rotation_by_speed_z() const { return rotation_by_speed_z; }
SIMPLE_SETGET(Vector2, rotation_by_speed_range, _mark_core_dirty())
void YParticles3D::set_enable_inherit_velocity(bool p_value) { enable_inherit_velocity = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_inherit_velocity() const { return enable_inherit_velocity; }
void YParticles3D::set_inherit_velocity_mode(int p_value) { inherit_velocity_mode = CLAMP(p_value, 0, 3); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_inherit_velocity_mode() const { return inherit_velocity_mode; }
void YParticles3D::set_inherit_velocity_multiplier(float p_value) { inherit_velocity_multiplier = p_value; _mark_core_dirty(); }
float YParticles3D::get_inherit_velocity_multiplier() const { return inherit_velocity_multiplier; }
void YParticles3D::set_inherit_velocity_curve(const Ref<Curve> &p_value) { inherit_velocity_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_inherit_velocity_curve() const { return inherit_velocity_curve; }
void YParticles3D::set_orbit_over_lifetime(const Ref<Curve> &p_value) { orbit_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_orbit_over_lifetime() const { return orbit_over_lifetime; }
SIMPLE_SETGET(Vector3, orbit_around_axis, _mark_core_dirty())
void YParticles3D::set_enable_color_over_lifetime(bool p_value) { enable_color_over_lifetime = p_value; _mark_material_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_color_over_lifetime() const { return enable_color_over_lifetime; }
void YParticles3D::set_color_over_lifetime(const Ref<GradientTexture1D> &p_value) { color_over_lifetime = p_value; _mark_material_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_color_over_lifetime() const { return color_over_lifetime; }
void YParticles3D::set_color_over_lifetime_secondary(const Ref<GradientTexture1D> &p_value) { color_over_lifetime_secondary = p_value; _mark_material_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_color_over_lifetime_secondary() const { return color_over_lifetime_secondary; }
void YParticles3D::set_alpha_over_lifetime(const Ref<Curve> &p_value) { alpha_over_lifetime = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_alpha_over_lifetime() const { return alpha_over_lifetime; }
void YParticles3D::set_alpha_over_lifetime_secondary(const Ref<Curve> &p_value) { alpha_over_lifetime_secondary = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_alpha_over_lifetime_secondary() const { return alpha_over_lifetime_secondary; }
SIMPLE_SETGET(bool, color_over_lifetime_use_two_gradients, _mark_material_dirty())
SIMPLE_SETGET(bool, use_start_color_gradient, _mark_material_dirty())
void YParticles3D::set_start_color_gradient(const Ref<GradientTexture1D> &p_value) { start_color_gradient = p_value; _mark_material_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_start_color_gradient() const { return start_color_gradient; }
void YParticles3D::set_start_color_gradient_secondary(const Ref<GradientTexture1D> &p_value) { start_color_gradient_secondary = p_value; _mark_material_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_start_color_gradient_secondary() const { return start_color_gradient_secondary; }
SIMPLE_SETGET(bool, start_color_use_two_gradients, _mark_material_dirty())
SIMPLE_SETGET(float, starting_hue, _mark_core_dirty())
SIMPLE_SETGET(float, hue_variation, _mark_core_dirty())
void YParticles3D::set_texture_sheet_enabled(bool p_value) { texture_sheet_enabled = p_value; _mark_material_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_texture_sheet_enabled() const { return texture_sheet_enabled; }
void YParticles3D::set_h_frames(int p_value) { h_frames = MAX(1, p_value); _mark_material_dirty(); }
int YParticles3D::get_h_frames() const { return h_frames; }
void YParticles3D::set_v_frames(int p_value) { v_frames = MAX(1, p_value); _mark_material_dirty(); }
int YParticles3D::get_v_frames() const { return v_frames; }
void YParticles3D::set_tiles_mode(TextureSheetTiles p_value) { tiles_mode = p_value; _mark_material_dirty(); }
YParticles3D::TextureSheetTiles YParticles3D::get_tiles_mode() const { return tiles_mode; }
void YParticles3D::set_use_random_starting_tile(bool p_value) { use_random_starting_tile = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_use_random_starting_tile() const { return use_random_starting_tile; }
SIMPLE_SETGET(int, start_index_tile, _mark_core_dirty())
SIMPLE_SETGET(float, animation_cycles, _mark_material_dirty())
void YParticles3D::set_frame_over_time(const Ref<Curve> &p_value) { frame_over_time = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_frame_over_time() const { return frame_over_time; }
void YParticles3D::set_particle_texture(const Ref<Texture2D> &p_value) { particle_texture = p_value; _mark_material_dirty(); }
Ref<Texture2D> YParticles3D::get_particle_texture() const { return particle_texture; }
void YParticles3D::set_enable_trails(bool p_value) { enable_trails = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
bool YParticles3D::get_enable_trails() const { return enable_trails; }
void YParticles3D::set_trail_ratio(float p_value) { trail_ratio = CLAMP(p_value, 0.0f, 1.0f); _mark_core_dirty(); }
float YParticles3D::get_trail_ratio() const { return trail_ratio; }
void YParticles3D::set_trail_lifetime_mode(int p_value) { trail_lifetime_mode = CLAMP(p_value, 0, 1); _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
int YParticles3D::get_trail_lifetime_mode() const { return trail_lifetime_mode; }
void YParticles3D::set_trail_lifetime(float p_value) { trail_lifetime = MAX(0.0f, p_value); _mark_core_dirty(); }
float YParticles3D::get_trail_lifetime() const { return trail_lifetime; }
void YParticles3D::set_trail_lifetime_curve(const Ref<Curve> &p_value) { trail_lifetime_curve = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_trail_lifetime_curve() const { return trail_lifetime_curve; }
void YParticles3D::set_trail_min_vertex_distance(float p_value) { trail_min_vertex_distance = MAX(0.001f, p_value); _mark_core_dirty(); }
float YParticles3D::get_trail_min_vertex_distance() const { return trail_min_vertex_distance; }
SIMPLE_SETGET(bool, trail_world_space, _mark_core_dirty())
SIMPLE_SETGET(bool, trail_die_with_particles, _mark_core_dirty())
SIMPLE_SETGET(bool, trail_size_affects_width, _mark_core_dirty())
SIMPLE_SETGET(bool, trail_size_affects_lifetime, _mark_core_dirty())
SIMPLE_SETGET(bool, trail_inherit_particle_color, _mark_core_dirty())
void YParticles3D::set_trail_texture_mode(TrailTextureMode p_value) { trail_texture_mode = p_value; _mark_core_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::TrailTextureMode YParticles3D::get_trail_texture_mode() const { return trail_texture_mode; }
void YParticles3D::set_trail_color_over_lifetime(const Ref<GradientTexture1D> &p_value) { trail_color_over_lifetime = p_value; _mark_core_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_trail_color_over_lifetime() const { return trail_color_over_lifetime; }
void YParticles3D::set_trail_color_over_trail(const Ref<GradientTexture1D> &p_value) { trail_color_over_trail = p_value; _mark_core_dirty(); }
Ref<GradientTexture1D> YParticles3D::get_trail_color_over_trail() const { return trail_color_over_trail; }
void YParticles3D::set_trail_width_over_trail(const Ref<Curve> &p_value) { trail_width_over_trail = p_value; _mark_core_dirty(); }
Ref<Curve> YParticles3D::get_trail_width_over_trail() const { return trail_width_over_trail; }
void YParticles3D::set_trail_texture(const Ref<Texture2D> &p_value) { trail_texture = p_value; _mark_material_dirty(); }
Ref<Texture2D> YParticles3D::get_trail_texture() const { return trail_texture; }
SIMPLE_SETGET(Color, tint_color, _mark_material_dirty())
void YParticles3D::set_billboard_mode(BillboardMode p_value) { billboard_mode = p_value; _mark_material_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::BillboardMode YParticles3D::get_billboard_mode() const { return billboard_mode; }
void YParticles3D::set_render_alignment(RenderAlignment p_value) { render_alignment = p_value; _mark_material_dirty(); _yparticles3d_notify_inspector_if_editor(this); }
YParticles3D::RenderAlignment YParticles3D::get_render_alignment() const { return render_alignment; }
SIMPLE_SETGET(float, velocity_stretch, _mark_core_dirty())
SIMPLE_SETGET(float, length_stretch, _mark_core_dirty())
SIMPLE_SETGET(bool, align_to_velocity, _mark_core_dirty())
SIMPLE_SETGET(float, align_offset_degrees, _mark_core_dirty())
void YParticles3D::set_blend_mode(BlendMode p_value) { blend_mode = p_value; _mark_material_dirty(); }
YParticles3D::BlendMode YParticles3D::get_blend_mode() const { return blend_mode; }
SIMPLE_SETGET(int, render_priority, _mark_material_dirty())
void YParticles3D::set_sampling_filter(SamplingFilter p_value) { sampling_filter = p_value; _mark_material_dirty(); }
YParticles3D::SamplingFilter YParticles3D::get_sampling_filter() const { return sampling_filter; }
void YParticles3D::set_rendering_layer(uint32_t p_value) {
	rendering_layer = p_value;
	if (_instance.is_valid()) {
		RenderingServer::get_singleton()->instance_set_layer_mask(_instance, rendering_layer);
	}
	if (_trail_instance.is_valid()) {
		RenderingServer::get_singleton()->instance_set_layer_mask(_trail_instance, rendering_layer);
	}
}
uint32_t YParticles3D::get_rendering_layer() const { return rendering_layer; }
void YParticles3D::set_visibility_aabb(const AABB &p_value) {
	visibility_aabb = p_value;
	if (_multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_custom_aabb(_multimesh, visibility_aabb);
	}
	_yparticles3d_update_gizmos_if_editor(this);
}
AABB YParticles3D::get_visibility_aabb() const { return visibility_aabb; }
void YParticles3D::set_override_material(const Ref<Material> &p_value) { override_material = p_value; _mark_material_dirty(); }
Ref<Material> YParticles3D::get_override_material() const { return override_material; }
void YParticles3D::set_custom_mesh(const Ref<Mesh> &p_value) { custom_mesh = p_value; _mark_core_dirty(); }
Ref<Mesh> YParticles3D::get_custom_mesh() const { return custom_mesh; }
SIMPLE_SETGET(float, playback_speed, _mark_core_dirty())
void YParticles3D::set_paused(bool p_value) { paused = p_value; }
bool YParticles3D::is_paused() const { return paused; }
void YParticles3D::set_fixed_fps(int p_value) { fixed_fps = MAX(0, p_value); _fixed_fps_remainder = 0.0; }
int YParticles3D::get_fixed_fps() const { return fixed_fps; }
bool YParticles3D::is_playing() const { return _playing; }
int YParticles3D::get_visible_particle_count() const { return _visible_count; }
float YParticles3D::get_simulation_time() const { return MAX(_emission_time, 0.0f); }

#undef SIMPLE_SETGET

void YParticles3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play", "clear_on_play"), &YParticles3D::play, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("stop", "clear"), &YParticles3D::stop, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("clear", "stop"), &YParticles3D::clear, DEFVAL(false));

	ADD_GROUP("Main", "");
	ClassDB::bind_method(D_METHOD("set_duration", "value"), &YParticles3D::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &YParticles3D::get_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");

	ClassDB::bind_method(D_METHOD("set_start_lifetime_mode", "value"), &YParticles3D::set_start_lifetime_mode);
	ClassDB::bind_method(D_METHOD("get_start_lifetime_mode"), &YParticles3D::get_start_lifetime_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_lifetime_mode", PROPERTY_HINT_ENUM, "Constant,Random,Curve"), "set_start_lifetime_mode", "get_start_lifetime_mode");

	ClassDB::bind_method(D_METHOD("set_start_lifetime_constant", "value"), &YParticles3D::set_start_lifetime_constant);
	ClassDB::bind_method(D_METHOD("get_start_lifetime_constant"), &YParticles3D::get_start_lifetime_constant);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_lifetime_constant"), "set_start_lifetime_constant", "get_start_lifetime_constant");

	ClassDB::bind_method(D_METHOD("set_start_lifetime_random", "value"), &YParticles3D::set_start_lifetime_random);
	ClassDB::bind_method(D_METHOD("get_start_lifetime_random"), &YParticles3D::get_start_lifetime_random);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_lifetime_random"), "set_start_lifetime_random", "get_start_lifetime_random");

	ClassDB::bind_method(D_METHOD("set_start_lifetime_curve", "value"), &YParticles3D::set_start_lifetime_curve);
	ClassDB::bind_method(D_METHOD("get_start_lifetime_curve"), &YParticles3D::get_start_lifetime_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_lifetime_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_lifetime_curve", "get_start_lifetime_curve");

	ClassDB::bind_method(D_METHOD("set_start_speed_mode", "value"), &YParticles3D::set_start_speed_mode);
	ClassDB::bind_method(D_METHOD("get_start_speed_mode"), &YParticles3D::get_start_speed_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_speed_mode", PROPERTY_HINT_ENUM, "Constant,Random"), "set_start_speed_mode", "get_start_speed_mode");

	ClassDB::bind_method(D_METHOD("set_start_speed_constant", "value"), &YParticles3D::set_start_speed_constant);
	ClassDB::bind_method(D_METHOD("get_start_speed_constant"), &YParticles3D::get_start_speed_constant);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_speed_constant"), "set_start_speed_constant", "get_start_speed_constant");

	ClassDB::bind_method(D_METHOD("set_start_speed_random", "value"), &YParticles3D::set_start_speed_random);
	ClassDB::bind_method(D_METHOD("get_start_speed_random"), &YParticles3D::get_start_speed_random);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_speed_random"), "set_start_speed_random", "get_start_speed_random");

	ClassDB::bind_method(D_METHOD("set_gravity", "value"), &YParticles3D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &YParticles3D::get_gravity);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");

	ClassDB::bind_method(D_METHOD("set_start_size_mode", "value"), &YParticles3D::set_start_size_mode);
	ClassDB::bind_method(D_METHOD("get_start_size_mode"), &YParticles3D::get_start_size_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_size_mode", PROPERTY_HINT_ENUM, "Constant,Random,Curve,SquareRandom,TwoCurves,SeparateAxesConstant,SeparateAxesRandom,SeparateAxesCurve,SeparateAxesTwoCurves"), "set_start_size_mode", "get_start_size_mode");

	ClassDB::bind_method(D_METHOD("set_start_size_constant", "value"), &YParticles3D::set_start_size_constant);
	ClassDB::bind_method(D_METHOD("get_start_size_constant"), &YParticles3D::get_start_size_constant);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_size_constant"), "set_start_size_constant", "get_start_size_constant");

	ClassDB::bind_method(D_METHOD("set_start_size_random", "value"), &YParticles3D::set_start_size_random);
	ClassDB::bind_method(D_METHOD("get_start_size_random"), &YParticles3D::get_start_size_random);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR4, "start_size_random"), "set_start_size_random", "get_start_size_random");

	ClassDB::bind_method(D_METHOD("set_start_size_curve", "value"), &YParticles3D::set_start_size_curve);
	ClassDB::bind_method(D_METHOD("get_start_size_curve"), &YParticles3D::get_start_size_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_curve", "get_start_size_curve");

	ClassDB::bind_method(D_METHOD("set_start_size_curve_min", "value"), &YParticles3D::set_start_size_curve_min);
	ClassDB::bind_method(D_METHOD("get_start_size_curve_min"), &YParticles3D::get_start_size_curve_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_curve_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_curve_min", "get_start_size_curve_min");

	ClassDB::bind_method(D_METHOD("set_start_size_curve_max", "value"), &YParticles3D::set_start_size_curve_max);
	ClassDB::bind_method(D_METHOD("get_start_size_curve_max"), &YParticles3D::get_start_size_curve_max);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_curve_max", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_curve_max", "get_start_size_curve_max");

	ClassDB::bind_method(D_METHOD("set_start_size_square_random", "value"), &YParticles3D::set_start_size_square_random);
	ClassDB::bind_method(D_METHOD("get_start_size_square_random"), &YParticles3D::get_start_size_square_random);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_size_square_random"), "set_start_size_square_random", "get_start_size_square_random");

	ClassDB::bind_method(D_METHOD("set_start_size_constant_3d", "value"), &YParticles3D::set_start_size_constant_3d);
	ClassDB::bind_method(D_METHOD("get_start_size_constant_3d"), &YParticles3D::get_start_size_constant_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_size_constant_3d"), "set_start_size_constant_3d", "get_start_size_constant_3d");
	ClassDB::bind_method(D_METHOD("set_start_size_random_min_3d", "value"), &YParticles3D::set_start_size_random_min_3d);
	ClassDB::bind_method(D_METHOD("get_start_size_random_min_3d"), &YParticles3D::get_start_size_random_min_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_size_random_min_3d"), "set_start_size_random_min_3d", "get_start_size_random_min_3d");
	ClassDB::bind_method(D_METHOD("set_start_size_random_max_3d", "value"), &YParticles3D::set_start_size_random_max_3d);
	ClassDB::bind_method(D_METHOD("get_start_size_random_max_3d"), &YParticles3D::get_start_size_random_max_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_size_random_max_3d"), "set_start_size_random_max_3d", "get_start_size_random_max_3d");
	ClassDB::bind_method(D_METHOD("set_start_size_x_curve", "value"), &YParticles3D::set_start_size_x_curve);
	ClassDB::bind_method(D_METHOD("get_start_size_x_curve"), &YParticles3D::get_start_size_x_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_x_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_x_curve", "get_start_size_x_curve");
	ClassDB::bind_method(D_METHOD("set_start_size_y_curve", "value"), &YParticles3D::set_start_size_y_curve);
	ClassDB::bind_method(D_METHOD("get_start_size_y_curve"), &YParticles3D::get_start_size_y_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_y_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_y_curve", "get_start_size_y_curve");
	ClassDB::bind_method(D_METHOD("set_start_size_z_curve", "value"), &YParticles3D::set_start_size_z_curve);
	ClassDB::bind_method(D_METHOD("get_start_size_z_curve"), &YParticles3D::get_start_size_z_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_z_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_z_curve", "get_start_size_z_curve");
	ClassDB::bind_method(D_METHOD("set_start_size_x_curve_min", "value"), &YParticles3D::set_start_size_x_curve_min);
	ClassDB::bind_method(D_METHOD("get_start_size_x_curve_min"), &YParticles3D::get_start_size_x_curve_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_x_curve_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_x_curve_min", "get_start_size_x_curve_min");
	ClassDB::bind_method(D_METHOD("set_start_size_y_curve_min", "value"), &YParticles3D::set_start_size_y_curve_min);
	ClassDB::bind_method(D_METHOD("get_start_size_y_curve_min"), &YParticles3D::get_start_size_y_curve_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_y_curve_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_y_curve_min", "get_start_size_y_curve_min");
	ClassDB::bind_method(D_METHOD("set_start_size_z_curve_min", "value"), &YParticles3D::set_start_size_z_curve_min);
	ClassDB::bind_method(D_METHOD("get_start_size_z_curve_min"), &YParticles3D::get_start_size_z_curve_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_size_z_curve_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_size_z_curve_min", "get_start_size_z_curve_min");

	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_mode", "value"), &YParticles3D::set_start_rotation_degrees_mode);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_mode"), &YParticles3D::get_start_rotation_degrees_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_rotation_degrees_mode", PROPERTY_HINT_ENUM, "Constant,Random,Curve,SeparateAxesConstant,SeparateAxesRandom,SeparateAxesCurve"), "set_start_rotation_degrees_mode", "get_start_rotation_degrees_mode");

	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_constant", "value"), &YParticles3D::set_start_rotation_degrees_constant);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_constant"), &YParticles3D::get_start_rotation_degrees_constant);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_rotation_degrees_constant", PROPERTY_HINT_RANGE, "-180,180,0.01"), "set_start_rotation_degrees_constant", "get_start_rotation_degrees_constant");

	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_random", "value"), &YParticles3D::set_start_rotation_degrees_random);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_random"), &YParticles3D::get_start_rotation_degrees_random);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_rotation_degrees_random"), "set_start_rotation_degrees_random", "get_start_rotation_degrees_random");

	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_curve", "value"), &YParticles3D::set_start_rotation_degrees_curve);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_curve"), &YParticles3D::get_start_rotation_degrees_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_rotation_degrees_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_rotation_degrees_curve", "get_start_rotation_degrees_curve");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_constant_3d", "value"), &YParticles3D::set_start_rotation_degrees_constant_3d);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_constant_3d"), &YParticles3D::get_start_rotation_degrees_constant_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_rotation_degrees_constant_3d"), "set_start_rotation_degrees_constant_3d", "get_start_rotation_degrees_constant_3d");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_random_min_3d", "value"), &YParticles3D::set_start_rotation_degrees_random_min_3d);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_random_min_3d"), &YParticles3D::get_start_rotation_degrees_random_min_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_rotation_degrees_random_min_3d"), "set_start_rotation_degrees_random_min_3d", "get_start_rotation_degrees_random_min_3d");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_random_max_3d", "value"), &YParticles3D::set_start_rotation_degrees_random_max_3d);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_random_max_3d"), &YParticles3D::get_start_rotation_degrees_random_max_3d);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_rotation_degrees_random_max_3d"), "set_start_rotation_degrees_random_max_3d", "get_start_rotation_degrees_random_max_3d");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_x_curve", "value"), &YParticles3D::set_start_rotation_degrees_x_curve);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_x_curve"), &YParticles3D::get_start_rotation_degrees_x_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_rotation_degrees_x_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_rotation_degrees_x_curve", "get_start_rotation_degrees_x_curve");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_y_curve", "value"), &YParticles3D::set_start_rotation_degrees_y_curve);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_y_curve"), &YParticles3D::get_start_rotation_degrees_y_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_rotation_degrees_y_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_rotation_degrees_y_curve", "get_start_rotation_degrees_y_curve");
	ClassDB::bind_method(D_METHOD("set_start_rotation_degrees_z_curve", "value"), &YParticles3D::set_start_rotation_degrees_z_curve);
	ClassDB::bind_method(D_METHOD("get_start_rotation_degrees_z_curve"), &YParticles3D::get_start_rotation_degrees_z_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_rotation_degrees_z_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_start_rotation_degrees_z_curve", "get_start_rotation_degrees_z_curve");

	ClassDB::bind_method(D_METHOD("set_use_world_space", "value"), &YParticles3D::set_use_world_space);
	ClassDB::bind_method(D_METHOD("is_using_world_space"), &YParticles3D::is_using_world_space);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_world_space"), "set_use_world_space", "is_using_world_space");

	ADD_GROUP("Play Behavior", "");
	ClassDB::bind_method(D_METHOD("set_play_on_start", "value"), &YParticles3D::set_play_on_start);
	ClassDB::bind_method(D_METHOD("get_play_on_start"), &YParticles3D::get_play_on_start);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "play_on_start"), "set_play_on_start", "get_play_on_start");

	ClassDB::bind_method(D_METHOD("set_loop", "value"), &YParticles3D::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &YParticles3D::get_loop);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");

	ClassDB::bind_method(D_METHOD("set_play_in_reverse", "value"), &YParticles3D::set_play_in_reverse);
	ClassDB::bind_method(D_METHOD("get_play_in_reverse"), &YParticles3D::get_play_in_reverse);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "play_in_reverse"), "set_play_in_reverse", "get_play_in_reverse");

	ClassDB::bind_method(D_METHOD("set_start_delay", "value"), &YParticles3D::set_start_delay);
	ClassDB::bind_method(D_METHOD("get_start_delay"), &YParticles3D::get_start_delay);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_delay"), "set_start_delay", "get_start_delay");

	ClassDB::bind_method(D_METHOD("set_start_delay_percentage", "value"), &YParticles3D::set_start_delay_percentage);
	ClassDB::bind_method(D_METHOD("get_start_delay_percentage"), &YParticles3D::get_start_delay_percentage);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_delay_percentage", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_start_delay_percentage", "get_start_delay_percentage");

	ClassDB::bind_method(D_METHOD("set_destroy_on_finish", "value"), &YParticles3D::set_destroy_on_finish);
	ClassDB::bind_method(D_METHOD("get_destroy_on_finish"), &YParticles3D::get_destroy_on_finish);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "destroy_on_finish"), "set_destroy_on_finish", "get_destroy_on_finish");

	ClassDB::bind_method(D_METHOD("set_debugging", "value"), &YParticles3D::set_debugging);
	ClassDB::bind_method(D_METHOD("get_debugging"), &YParticles3D::get_debugging);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debugging"), "set_debugging", "get_debugging");

	ClassDB::bind_method(D_METHOD("set_playback_speed", "value"), &YParticles3D::set_playback_speed);
	ClassDB::bind_method(D_METHOD("get_playback_speed"), &YParticles3D::get_playback_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback_speed"), "set_playback_speed", "get_playback_speed");

	ClassDB::bind_method(D_METHOD("set_paused", "value"), &YParticles3D::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &YParticles3D::is_paused);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "is_paused");

	ClassDB::bind_method(D_METHOD("set_fixed_fps", "value"), &YParticles3D::set_fixed_fps);
	ClassDB::bind_method(D_METHOD("get_fixed_fps"), &YParticles3D::get_fixed_fps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_fps", PROPERTY_HINT_RANGE, "0,240,1"), "set_fixed_fps", "get_fixed_fps");

	ClassDB::bind_method(D_METHOD("get_simulation_time"), &YParticles3D::get_simulation_time);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simulation_time", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_simulation_time");

	ADD_GROUP("Emission", "");
	ClassDB::bind_method(D_METHOD("set_emitting", "value"), &YParticles3D::set_emitting);
	ClassDB::bind_method(D_METHOD("is_emitting"), &YParticles3D::is_emitting);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting", PROPERTY_HINT_GROUP_ENABLE), "set_emitting", "is_emitting");

	ClassDB::bind_method(D_METHOD("set_max_particles", "value"), &YParticles3D::set_max_particles);
	ClassDB::bind_method(D_METHOD("get_max_particles"), &YParticles3D::get_max_particles);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_particles", PROPERTY_HINT_RANGE, "1,100000,1"), "set_max_particles", "get_max_particles");

	ClassDB::bind_method(D_METHOD("set_max_emissions_per_frame", "value"), &YParticles3D::set_max_emissions_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_emissions_per_frame"), &YParticles3D::get_max_emissions_per_frame);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_emissions_per_frame", PROPERTY_HINT_RANGE, "1,100000,1"), "set_max_emissions_per_frame", "get_max_emissions_per_frame");

	ClassDB::bind_method(D_METHOD("set_rate_over_time_mode", "value"), &YParticles3D::set_rate_over_time_mode);
	ClassDB::bind_method(D_METHOD("get_rate_over_time_mode"), &YParticles3D::get_rate_over_time_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rate_over_time_mode", PROPERTY_HINT_ENUM, "Constant,Curve"), "set_rate_over_time_mode", "get_rate_over_time_mode");

	ClassDB::bind_method(D_METHOD("set_rate_over_time", "value"), &YParticles3D::set_rate_over_time);
	ClassDB::bind_method(D_METHOD("get_rate_over_time"), &YParticles3D::get_rate_over_time);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate_over_time"), "set_rate_over_time", "get_rate_over_time");

	ClassDB::bind_method(D_METHOD("set_rate_over_time_curve", "value"), &YParticles3D::set_rate_over_time_curve);
	ClassDB::bind_method(D_METHOD("get_rate_over_time_curve"), &YParticles3D::get_rate_over_time_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rate_over_time_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rate_over_time_curve", "get_rate_over_time_curve");

	ClassDB::bind_method(D_METHOD("set_rate_over_distance", "value"), &YParticles3D::set_rate_over_distance);
	ClassDB::bind_method(D_METHOD("get_rate_over_distance"), &YParticles3D::get_rate_over_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate_over_distance"), "set_rate_over_distance", "get_rate_over_distance");

	ClassDB::bind_method(D_METHOD("set_bursts", "value"), &YParticles3D::set_bursts);
	ClassDB::bind_method(D_METHOD("get_bursts"), &YParticles3D::get_bursts);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bursts", PROPERTY_HINT_ARRAY_TYPE, "Dictionary", PROPERTY_USAGE_DEFAULT), "set_bursts", "get_bursts");

	ADD_GROUP("Shape", "");
	ClassDB::bind_method(D_METHOD("set_enable_shape", "value"), &YParticles3D::set_enable_shape);
	ClassDB::bind_method(D_METHOD("get_enable_shape"), &YParticles3D::get_enable_shape);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_shape", PROPERTY_HINT_GROUP_ENABLE), "set_enable_shape", "get_enable_shape");

	ClassDB::bind_method(D_METHOD("set_shape_type", "value"), &YParticles3D::set_shape_type);
	ClassDB::bind_method(D_METHOD("get_shape_type"), &YParticles3D::get_shape_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape_type", PROPERTY_HINT_ENUM, "Cone,Sphere,Hemisphere,Box,Circle,Edge,Mesh"), "set_shape_type", "get_shape_type");

	ClassDB::bind_method(D_METHOD("set_radius", "value"), &YParticles3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &YParticles3D::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");

	ClassDB::bind_method(D_METHOD("set_radius_thickness", "value"), &YParticles3D::set_radius_thickness);
	ClassDB::bind_method(D_METHOD("get_radius_thickness"), &YParticles3D::get_radius_thickness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius_thickness", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_radius_thickness", "get_radius_thickness");

	ClassDB::bind_method(D_METHOD("set_angle", "value"), &YParticles3D::set_angle);
	ClassDB::bind_method(D_METHOD("get_angle"), &YParticles3D::get_angle);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angle"), "set_angle", "get_angle");

	ClassDB::bind_method(D_METHOD("set_box_extents", "value"), &YParticles3D::set_box_extents);
	ClassDB::bind_method(D_METHOD("get_box_extents"), &YParticles3D::get_box_extents);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "box_extents"), "set_box_extents", "get_box_extents");

	ClassDB::bind_method(D_METHOD("set_emission_mesh", "value"), &YParticles3D::set_emission_mesh);
	ClassDB::bind_method(D_METHOD("get_emission_mesh"), &YParticles3D::get_emission_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "emission_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_emission_mesh", "get_emission_mesh");

	ClassDB::bind_method(D_METHOD("set_emission_mesh_scale", "value"), &YParticles3D::set_emission_mesh_scale);
	ClassDB::bind_method(D_METHOD("get_emission_mesh_scale"), &YParticles3D::get_emission_mesh_scale);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_mesh_scale"), "set_emission_mesh_scale", "get_emission_mesh_scale");

	ClassDB::bind_method(D_METHOD("set_random_direction", "value"), &YParticles3D::set_random_direction);
	ClassDB::bind_method(D_METHOD("get_random_direction"), &YParticles3D::get_random_direction);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "random_direction", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_random_direction", "get_random_direction");

	ClassDB::bind_method(D_METHOD("set_spherize_direction", "value"), &YParticles3D::set_spherize_direction);
	ClassDB::bind_method(D_METHOD("get_spherize_direction"), &YParticles3D::get_spherize_direction);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spherize_direction", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_spherize_direction", "get_spherize_direction");

	ClassDB::bind_method(D_METHOD("set_emit_from", "value"), &YParticles3D::set_emit_from);
	ClassDB::bind_method(D_METHOD("get_emit_from"), &YParticles3D::get_emit_from);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emit_from", PROPERTY_HINT_ENUM, "Base,Volume"), "set_emit_from", "get_emit_from");

	ClassDB::bind_method(D_METHOD("set_shape_length", "value"), &YParticles3D::set_shape_length);
	ClassDB::bind_method(D_METHOD("get_shape_length"), &YParticles3D::get_shape_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shape_length"), "set_shape_length", "get_shape_length");

	ClassDB::bind_method(D_METHOD("set_arc_degrees", "value"), &YParticles3D::set_arc_degrees);
	ClassDB::bind_method(D_METHOD("get_arc_degrees"), &YParticles3D::get_arc_degrees);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arc_degrees"), "set_arc_degrees", "get_arc_degrees");

	ClassDB::bind_method(D_METHOD("set_arc_mode", "value"), &YParticles3D::set_arc_mode);
	ClassDB::bind_method(D_METHOD("get_arc_mode"), &YParticles3D::get_arc_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "arc_mode", PROPERTY_HINT_ENUM, "Random,Loop,PingPong,BurstSpread"), "set_arc_mode", "get_arc_mode");

	ClassDB::bind_method(D_METHOD("set_arc_spread", "value"), &YParticles3D::set_arc_spread);
	ClassDB::bind_method(D_METHOD("get_arc_spread"), &YParticles3D::get_arc_spread);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arc_spread"), "set_arc_spread", "get_arc_spread");

	ClassDB::bind_method(D_METHOD("set_arc_speed_mode", "value"), &YParticles3D::set_arc_speed_mode);
	ClassDB::bind_method(D_METHOD("get_arc_speed_mode"), &YParticles3D::get_arc_speed_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "arc_speed_mode", PROPERTY_HINT_ENUM, "Constant,Curve"), "set_arc_speed_mode", "get_arc_speed_mode");

	ClassDB::bind_method(D_METHOD("set_arc_speed_constant", "value"), &YParticles3D::set_arc_speed_constant);
	ClassDB::bind_method(D_METHOD("get_arc_speed_constant"), &YParticles3D::get_arc_speed_constant);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arc_speed_constant"), "set_arc_speed_constant", "get_arc_speed_constant");

	ClassDB::bind_method(D_METHOD("set_arc_speed_curve", "value"), &YParticles3D::set_arc_speed_curve);
	ClassDB::bind_method(D_METHOD("get_arc_speed_curve"), &YParticles3D::get_arc_speed_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "arc_speed_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_arc_speed_curve", "get_arc_speed_curve");

	ClassDB::bind_method(D_METHOD("set_direction_in_world_space", "value"), &YParticles3D::set_direction_in_world_space);
	ClassDB::bind_method(D_METHOD("is_direction_in_world_space"), &YParticles3D::is_direction_in_world_space);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "direction_in_world_space"), "set_direction_in_world_space", "is_direction_in_world_space");

	ClassDB::bind_method(D_METHOD("set_invert_direction", "value"), &YParticles3D::set_invert_direction);
	ClassDB::bind_method(D_METHOD("is_direction_inverted"), &YParticles3D::is_direction_inverted);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "invert_direction"), "set_invert_direction", "is_direction_inverted");

	ClassDB::bind_method(D_METHOD("set_position_offset", "value"), &YParticles3D::set_position_offset);
	ClassDB::bind_method(D_METHOD("get_position_offset"), &YParticles3D::get_position_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position_offset"), "set_position_offset", "get_position_offset");

	ClassDB::bind_method(D_METHOD("set_rotation_offset", "value"), &YParticles3D::set_rotation_offset);
	ClassDB::bind_method(D_METHOD("get_rotation_offset"), &YParticles3D::get_rotation_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation_offset"), "set_rotation_offset", "get_rotation_offset");

	ADD_GROUP("Size Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_size_over_lifetime", "value"), &YParticles3D::set_enable_size_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_size_over_lifetime"), &YParticles3D::get_enable_size_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_size_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_size_over_lifetime", "get_enable_size_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_size_over_lifetime", "value"), &YParticles3D::set_size_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_size_over_lifetime"), &YParticles3D::get_size_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "size_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_size_over_lifetime", "get_size_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_size_over_lifetime_min", "value"), &YParticles3D::set_size_over_lifetime_min);
	ClassDB::bind_method(D_METHOD("get_size_over_lifetime_min"), &YParticles3D::get_size_over_lifetime_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "size_over_lifetime_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_size_over_lifetime_min", "get_size_over_lifetime_min");

	ClassDB::bind_method(D_METHOD("set_width_over_lifetime", "value"), &YParticles3D::set_width_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_width_over_lifetime"), &YParticles3D::get_width_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "width_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_width_over_lifetime", "get_width_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_height_over_lifetime", "value"), &YParticles3D::set_height_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_height_over_lifetime"), &YParticles3D::get_height_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_height_over_lifetime", "get_height_over_lifetime");
	ClassDB::bind_method(D_METHOD("set_depth_over_lifetime", "value"), &YParticles3D::set_depth_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_depth_over_lifetime"), &YParticles3D::get_depth_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "depth_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_depth_over_lifetime", "get_depth_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_size_over_lifetime_use_two_curves", "value"), &YParticles3D::set_size_over_lifetime_use_two_curves);
	ClassDB::bind_method(D_METHOD("get_size_over_lifetime_use_two_curves"), &YParticles3D::get_size_over_lifetime_use_two_curves);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "size_over_lifetime_use_two_curves"), "set_size_over_lifetime_use_two_curves", "get_size_over_lifetime_use_two_curves");

	ADD_GROUP("Velocity Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_velocity_over_lifetime", "value"), &YParticles3D::set_enable_velocity_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_velocity_over_lifetime"), &YParticles3D::get_enable_velocity_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_velocity_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_velocity_over_lifetime", "get_enable_velocity_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_mode", "value"), &YParticles3D::set_velocity_over_lifetime_mode);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_mode"), &YParticles3D::get_velocity_over_lifetime_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "velocity_over_lifetime_mode", PROPERTY_HINT_ENUM, "Curve,Separate Axes"), "set_velocity_over_lifetime_mode", "get_velocity_over_lifetime_mode");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime", "value"), &YParticles3D::set_velocity_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime"), &YParticles3D::get_velocity_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime", "get_velocity_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_min", "value"), &YParticles3D::set_velocity_over_lifetime_min);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_min"), &YParticles3D::get_velocity_over_lifetime_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_min", "get_velocity_over_lifetime_min");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_x", "value"), &YParticles3D::set_velocity_over_lifetime_x);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_x"), &YParticles3D::get_velocity_over_lifetime_x);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_x", "get_velocity_over_lifetime_x");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_x_min", "value"), &YParticles3D::set_velocity_over_lifetime_x_min);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_x_min"), &YParticles3D::get_velocity_over_lifetime_x_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_x_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_x_min", "get_velocity_over_lifetime_x_min");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_y", "value"), &YParticles3D::set_velocity_over_lifetime_y);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_y"), &YParticles3D::get_velocity_over_lifetime_y);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_y", "get_velocity_over_lifetime_y");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_y_min", "value"), &YParticles3D::set_velocity_over_lifetime_y_min);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_y_min"), &YParticles3D::get_velocity_over_lifetime_y_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_y_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_y_min", "get_velocity_over_lifetime_y_min");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_z", "value"), &YParticles3D::set_velocity_over_lifetime_z);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_z"), &YParticles3D::get_velocity_over_lifetime_z);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_z", "get_velocity_over_lifetime_z");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_z_min", "value"), &YParticles3D::set_velocity_over_lifetime_z_min);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_z_min"), &YParticles3D::get_velocity_over_lifetime_z_min);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_over_lifetime_z_min", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_velocity_over_lifetime_z_min", "get_velocity_over_lifetime_z_min");

	ClassDB::bind_method(D_METHOD("set_offset_over_lifetime", "value"), &YParticles3D::set_offset_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_offset_over_lifetime"), &YParticles3D::get_offset_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "offset_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_offset_over_lifetime", "get_offset_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_velocity_over_lifetime_use_two_curves", "value"), &YParticles3D::set_velocity_over_lifetime_use_two_curves);
	ClassDB::bind_method(D_METHOD("get_velocity_over_lifetime_use_two_curves"), &YParticles3D::get_velocity_over_lifetime_use_two_curves);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "velocity_over_lifetime_use_two_curves"), "set_velocity_over_lifetime_use_two_curves", "get_velocity_over_lifetime_use_two_curves");

	ClassDB::bind_method(D_METHOD("set_velocity_in_world_space", "value"), &YParticles3D::set_velocity_in_world_space);
	ClassDB::bind_method(D_METHOD("get_velocity_in_world_space"), &YParticles3D::get_velocity_in_world_space);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "velocity_in_world_space"), "set_velocity_in_world_space", "get_velocity_in_world_space");

	ADD_GROUP("Force Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_force_over_lifetime", "value"), &YParticles3D::set_enable_force_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_force_over_lifetime"), &YParticles3D::get_enable_force_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_force_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_force_over_lifetime", "get_enable_force_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_mode", "value"), &YParticles3D::set_force_over_lifetime_mode);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_mode"), &YParticles3D::get_force_over_lifetime_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "force_over_lifetime_mode", PROPERTY_HINT_ENUM, "Curve,Separate Axes,Constant Vector,Random Vector"), "set_force_over_lifetime_mode", "get_force_over_lifetime_mode");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime", "value"), &YParticles3D::set_force_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime"), &YParticles3D::get_force_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "force_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_force_over_lifetime", "get_force_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_x", "value"), &YParticles3D::set_force_over_lifetime_x);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_x"), &YParticles3D::get_force_over_lifetime_x);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "force_over_lifetime_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_force_over_lifetime_x", "get_force_over_lifetime_x");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_y", "value"), &YParticles3D::set_force_over_lifetime_y);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_y"), &YParticles3D::get_force_over_lifetime_y);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "force_over_lifetime_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_force_over_lifetime_y", "get_force_over_lifetime_y");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_z", "value"), &YParticles3D::set_force_over_lifetime_z);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_z"), &YParticles3D::get_force_over_lifetime_z);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "force_over_lifetime_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_force_over_lifetime_z", "get_force_over_lifetime_z");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_constant", "value"), &YParticles3D::set_force_over_lifetime_constant);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_constant"), &YParticles3D::get_force_over_lifetime_constant);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "force_over_lifetime_constant"), "set_force_over_lifetime_constant", "get_force_over_lifetime_constant");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_random_min", "value"), &YParticles3D::set_force_over_lifetime_random_min);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_random_min"), &YParticles3D::get_force_over_lifetime_random_min);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "force_over_lifetime_random_min"), "set_force_over_lifetime_random_min", "get_force_over_lifetime_random_min");

	ClassDB::bind_method(D_METHOD("set_force_over_lifetime_random_max", "value"), &YParticles3D::set_force_over_lifetime_random_max);
	ClassDB::bind_method(D_METHOD("get_force_over_lifetime_random_max"), &YParticles3D::get_force_over_lifetime_random_max);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "force_over_lifetime_random_max"), "set_force_over_lifetime_random_max", "get_force_over_lifetime_random_max");

	ClassDB::bind_method(D_METHOD("set_force_in_world_space", "value"), &YParticles3D::set_force_in_world_space);
	ClassDB::bind_method(D_METHOD("get_force_in_world_space"), &YParticles3D::get_force_in_world_space);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "force_in_world_space"), "set_force_in_world_space", "get_force_in_world_space");

	ADD_GROUP("Limit Velocity Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_limit_velocity_over_lifetime", "value"), &YParticles3D::set_enable_limit_velocity_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_limit_velocity_over_lifetime"), &YParticles3D::get_enable_limit_velocity_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_limit_velocity_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_limit_velocity_over_lifetime", "get_enable_limit_velocity_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_separate_axis", "value"), &YParticles3D::set_limit_velocity_over_lifetime_separate_axis);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_separate_axis"), &YParticles3D::get_limit_velocity_over_lifetime_separate_axis);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "limit_velocity_over_lifetime_separate_axis"), "set_limit_velocity_over_lifetime_separate_axis", "get_limit_velocity_over_lifetime_separate_axis");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_mode", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_mode);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_mode"), &YParticles3D::get_limit_velocity_over_lifetime_speed_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "limit_velocity_over_lifetime_speed_mode", PROPERTY_HINT_ENUM, "Constant,Curve,SeparateAxes,SeparateAxesCurves"), "set_limit_velocity_over_lifetime_speed_mode", "get_limit_velocity_over_lifetime_speed_mode");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed"), &YParticles3D::get_limit_velocity_over_lifetime_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_velocity_over_lifetime_speed", PROPERTY_HINT_RANGE, "0,200,0.01,or_greater"), "set_limit_velocity_over_lifetime_speed", "get_limit_velocity_over_lifetime_speed");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_curve", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_curve);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_curve"), &YParticles3D::get_limit_velocity_over_lifetime_speed_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "limit_velocity_over_lifetime_speed_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_limit_velocity_over_lifetime_speed_curve", "get_limit_velocity_over_lifetime_speed_curve");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_axis", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_axis);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_axis"), &YParticles3D::get_limit_velocity_over_lifetime_speed_axis);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "limit_velocity_over_lifetime_speed_axis"), "set_limit_velocity_over_lifetime_speed_axis", "get_limit_velocity_over_lifetime_speed_axis");
	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_x_curve", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_x_curve);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_x_curve"), &YParticles3D::get_limit_velocity_over_lifetime_speed_x_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "limit_velocity_over_lifetime_speed_x_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_limit_velocity_over_lifetime_speed_x_curve", "get_limit_velocity_over_lifetime_speed_x_curve");
	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_y_curve", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_y_curve);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_y_curve"), &YParticles3D::get_limit_velocity_over_lifetime_speed_y_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "limit_velocity_over_lifetime_speed_y_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_limit_velocity_over_lifetime_speed_y_curve", "get_limit_velocity_over_lifetime_speed_y_curve");
	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_speed_z_curve", "value"), &YParticles3D::set_limit_velocity_over_lifetime_speed_z_curve);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_speed_z_curve"), &YParticles3D::get_limit_velocity_over_lifetime_speed_z_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "limit_velocity_over_lifetime_speed_z_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_limit_velocity_over_lifetime_speed_z_curve", "get_limit_velocity_over_lifetime_speed_z_curve");

	ClassDB::bind_method(D_METHOD("set_limit_velocity_over_lifetime_dampen", "value"), &YParticles3D::set_limit_velocity_over_lifetime_dampen);
	ClassDB::bind_method(D_METHOD("get_limit_velocity_over_lifetime_dampen"), &YParticles3D::get_limit_velocity_over_lifetime_dampen);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_velocity_over_lifetime_dampen", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_limit_velocity_over_lifetime_dampen", "get_limit_velocity_over_lifetime_dampen");

	ADD_GROUP("Rotation Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_rotation_over_lifetime", "value"), &YParticles3D::set_enable_rotation_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_rotation_over_lifetime"), &YParticles3D::get_enable_rotation_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_rotation_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_rotation_over_lifetime", "get_enable_rotation_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_rotation_over_lifetime", "value"), &YParticles3D::set_rotation_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_rotation_over_lifetime"), &YParticles3D::get_rotation_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rotation_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rotation_over_lifetime", "get_rotation_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_rotation_over_lifetime_axis", "value"), &YParticles3D::set_rotation_over_lifetime_axis);
	ClassDB::bind_method(D_METHOD("get_rotation_over_lifetime_axis"), &YParticles3D::get_rotation_over_lifetime_axis);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation_over_lifetime_axis"), "set_rotation_over_lifetime_axis", "get_rotation_over_lifetime_axis");

	ADD_GROUP("Rotation By Speed", "");
	ClassDB::bind_method(D_METHOD("set_enable_rotation_by_speed", "value"), &YParticles3D::set_enable_rotation_by_speed);
	ClassDB::bind_method(D_METHOD("get_enable_rotation_by_speed"), &YParticles3D::get_enable_rotation_by_speed);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_rotation_by_speed", PROPERTY_HINT_GROUP_ENABLE), "set_enable_rotation_by_speed", "get_enable_rotation_by_speed");
	ClassDB::bind_method(D_METHOD("set_rotation_by_speed_mode", "value"), &YParticles3D::set_rotation_by_speed_mode);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed_mode"), &YParticles3D::get_rotation_by_speed_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_by_speed_mode", PROPERTY_HINT_ENUM, "Curve,SeparateAxes"), "set_rotation_by_speed_mode", "get_rotation_by_speed_mode");

	ClassDB::bind_method(D_METHOD("set_rotation_by_speed", "value"), &YParticles3D::set_rotation_by_speed);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed"), &YParticles3D::get_rotation_by_speed);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rotation_by_speed", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rotation_by_speed", "get_rotation_by_speed");
	ClassDB::bind_method(D_METHOD("set_rotation_by_speed_x", "value"), &YParticles3D::set_rotation_by_speed_x);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed_x"), &YParticles3D::get_rotation_by_speed_x);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rotation_by_speed_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rotation_by_speed_x", "get_rotation_by_speed_x");
	ClassDB::bind_method(D_METHOD("set_rotation_by_speed_y", "value"), &YParticles3D::set_rotation_by_speed_y);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed_y"), &YParticles3D::get_rotation_by_speed_y);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rotation_by_speed_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rotation_by_speed_y", "get_rotation_by_speed_y");
	ClassDB::bind_method(D_METHOD("set_rotation_by_speed_z", "value"), &YParticles3D::set_rotation_by_speed_z);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed_z"), &YParticles3D::get_rotation_by_speed_z);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rotation_by_speed_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_rotation_by_speed_z", "get_rotation_by_speed_z");

	ClassDB::bind_method(D_METHOD("set_rotation_by_speed_range", "value"), &YParticles3D::set_rotation_by_speed_range);
	ClassDB::bind_method(D_METHOD("get_rotation_by_speed_range"), &YParticles3D::get_rotation_by_speed_range);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "rotation_by_speed_range"), "set_rotation_by_speed_range", "get_rotation_by_speed_range");

	ADD_GROUP("Inherit Velocity", "");
	ClassDB::bind_method(D_METHOD("set_enable_inherit_velocity", "value"), &YParticles3D::set_enable_inherit_velocity);
	ClassDB::bind_method(D_METHOD("get_enable_inherit_velocity"), &YParticles3D::get_enable_inherit_velocity);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_inherit_velocity", PROPERTY_HINT_GROUP_ENABLE), "set_enable_inherit_velocity", "get_enable_inherit_velocity");
	ClassDB::bind_method(D_METHOD("set_inherit_velocity_mode", "value"), &YParticles3D::set_inherit_velocity_mode);
	ClassDB::bind_method(D_METHOD("get_inherit_velocity_mode"), &YParticles3D::get_inherit_velocity_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "inherit_velocity_mode", PROPERTY_HINT_ENUM, "CurrentConstant,CurrentCurve,InitialConstant,InitialCurve"), "set_inherit_velocity_mode", "get_inherit_velocity_mode");
	ClassDB::bind_method(D_METHOD("set_inherit_velocity_multiplier", "value"), &YParticles3D::set_inherit_velocity_multiplier);
	ClassDB::bind_method(D_METHOD("get_inherit_velocity_multiplier"), &YParticles3D::get_inherit_velocity_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inherit_velocity_multiplier"), "set_inherit_velocity_multiplier", "get_inherit_velocity_multiplier");
	ClassDB::bind_method(D_METHOD("set_inherit_velocity_curve", "value"), &YParticles3D::set_inherit_velocity_curve);
	ClassDB::bind_method(D_METHOD("get_inherit_velocity_curve"), &YParticles3D::get_inherit_velocity_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "inherit_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_inherit_velocity_curve", "get_inherit_velocity_curve");

	ClassDB::bind_method(D_METHOD("set_orbit_over_lifetime", "value"), &YParticles3D::set_orbit_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_orbit_over_lifetime"), &YParticles3D::get_orbit_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "orbit_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_orbit_over_lifetime", "get_orbit_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_orbit_around_axis", "value"), &YParticles3D::set_orbit_around_axis);
	ClassDB::bind_method(D_METHOD("get_orbit_around_axis"), &YParticles3D::get_orbit_around_axis);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "orbit_around_axis"), "set_orbit_around_axis", "get_orbit_around_axis");

	ADD_GROUP("Color Over Lifetime", "");
	ClassDB::bind_method(D_METHOD("set_enable_color_over_lifetime", "value"), &YParticles3D::set_enable_color_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_enable_color_over_lifetime"), &YParticles3D::get_enable_color_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_color_over_lifetime", PROPERTY_HINT_GROUP_ENABLE), "set_enable_color_over_lifetime", "get_enable_color_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_color_over_lifetime", "value"), &YParticles3D::set_color_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_color_over_lifetime"), &YParticles3D::get_color_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_color_over_lifetime", "get_color_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_color_over_lifetime_secondary", "value"), &YParticles3D::set_color_over_lifetime_secondary);
	ClassDB::bind_method(D_METHOD("get_color_over_lifetime_secondary"), &YParticles3D::get_color_over_lifetime_secondary);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_over_lifetime_secondary", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_color_over_lifetime_secondary", "get_color_over_lifetime_secondary");

	ClassDB::bind_method(D_METHOD("set_alpha_over_lifetime", "value"), &YParticles3D::set_alpha_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_alpha_over_lifetime"), &YParticles3D::get_alpha_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "alpha_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_alpha_over_lifetime", "get_alpha_over_lifetime");

	ClassDB::bind_method(D_METHOD("set_alpha_over_lifetime_secondary", "value"), &YParticles3D::set_alpha_over_lifetime_secondary);
	ClassDB::bind_method(D_METHOD("get_alpha_over_lifetime_secondary"), &YParticles3D::get_alpha_over_lifetime_secondary);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "alpha_over_lifetime_secondary", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_alpha_over_lifetime_secondary", "get_alpha_over_lifetime_secondary");

	ClassDB::bind_method(D_METHOD("set_color_over_lifetime_use_two_gradients", "value"), &YParticles3D::set_color_over_lifetime_use_two_gradients);
	ClassDB::bind_method(D_METHOD("get_color_over_lifetime_use_two_gradients"), &YParticles3D::get_color_over_lifetime_use_two_gradients);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "color_over_lifetime_use_two_gradients"), "set_color_over_lifetime_use_two_gradients", "get_color_over_lifetime_use_two_gradients");

	ClassDB::bind_method(D_METHOD("set_starting_hue", "value"), &YParticles3D::set_starting_hue);
	ClassDB::bind_method(D_METHOD("get_starting_hue"), &YParticles3D::get_starting_hue);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "starting_hue", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_starting_hue", "get_starting_hue");

	ClassDB::bind_method(D_METHOD("set_hue_variation", "value"), &YParticles3D::set_hue_variation);
	ClassDB::bind_method(D_METHOD("get_hue_variation"), &YParticles3D::get_hue_variation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hue_variation", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_hue_variation", "get_hue_variation");


	ADD_GROUP("Noise", "");
	ClassDB::bind_method(D_METHOD("set_enable_noise", "value"), &YParticles3D::set_enable_noise);
	ClassDB::bind_method(D_METHOD("get_enable_noise"), &YParticles3D::get_enable_noise);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_noise", PROPERTY_HINT_GROUP_ENABLE), "set_enable_noise", "get_enable_noise");

	ClassDB::bind_method(D_METHOD("set_noise_strength_mode", "value"), &YParticles3D::set_noise_strength_mode);
	ClassDB::bind_method(D_METHOD("get_noise_strength_mode"), &YParticles3D::get_noise_strength_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_strength_mode", PROPERTY_HINT_ENUM, "Constant,Curve,Separate Axes"), "set_noise_strength_mode", "get_noise_strength_mode");

	ClassDB::bind_method(D_METHOD("set_noise_strength", "value"), &YParticles3D::set_noise_strength);
	ClassDB::bind_method(D_METHOD("get_noise_strength"), &YParticles3D::get_noise_strength);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_strength", PROPERTY_HINT_RANGE, "0,50,0.01,or_greater"), "set_noise_strength", "get_noise_strength");

	ClassDB::bind_method(D_METHOD("set_noise_strength_curve", "value"), &YParticles3D::set_noise_strength_curve);
	ClassDB::bind_method(D_METHOD("get_noise_strength_curve"), &YParticles3D::get_noise_strength_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_strength_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_noise_strength_curve", "get_noise_strength_curve");

	ClassDB::bind_method(D_METHOD("set_noise_strength_x", "value"), &YParticles3D::set_noise_strength_x);
	ClassDB::bind_method(D_METHOD("get_noise_strength_x"), &YParticles3D::get_noise_strength_x);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_strength_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_noise_strength_x", "get_noise_strength_x");

	ClassDB::bind_method(D_METHOD("set_noise_strength_y", "value"), &YParticles3D::set_noise_strength_y);
	ClassDB::bind_method(D_METHOD("get_noise_strength_y"), &YParticles3D::get_noise_strength_y);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_strength_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_noise_strength_y", "get_noise_strength_y");

	ClassDB::bind_method(D_METHOD("set_noise_strength_z", "value"), &YParticles3D::set_noise_strength_z);
	ClassDB::bind_method(D_METHOD("get_noise_strength_z"), &YParticles3D::get_noise_strength_z);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_strength_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_noise_strength_z", "get_noise_strength_z");

	ClassDB::bind_method(D_METHOD("set_noise_scale", "value"), &YParticles3D::set_noise_scale);
	ClassDB::bind_method(D_METHOD("get_noise_scale"), &YParticles3D::get_noise_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_scale", PROPERTY_HINT_RANGE, "0.001,50,0.001,or_greater"), "set_noise_scale", "get_noise_scale");

	ClassDB::bind_method(D_METHOD("set_noise_scroll_speed", "value"), &YParticles3D::set_noise_scroll_speed);
	ClassDB::bind_method(D_METHOD("get_noise_scroll_speed"), &YParticles3D::get_noise_scroll_speed);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "noise_scroll_speed"), "set_noise_scroll_speed", "get_noise_scroll_speed");

	ClassDB::bind_method(D_METHOD("set_noise_position_amount", "value"), &YParticles3D::set_noise_position_amount);
	ClassDB::bind_method(D_METHOD("get_noise_position_amount"), &YParticles3D::get_noise_position_amount);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_position_amount", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_noise_position_amount", "get_noise_position_amount");

	ClassDB::bind_method(D_METHOD("set_noise_rotation_amount", "value"), &YParticles3D::set_noise_rotation_amount);
	ClassDB::bind_method(D_METHOD("get_noise_rotation_amount"), &YParticles3D::get_noise_rotation_amount);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_rotation_amount", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_noise_rotation_amount", "get_noise_rotation_amount");

	ClassDB::bind_method(D_METHOD("set_noise_size_amount", "value"), &YParticles3D::set_noise_size_amount);
	ClassDB::bind_method(D_METHOD("get_noise_size_amount"), &YParticles3D::get_noise_size_amount);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_size_amount", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_noise_size_amount", "get_noise_size_amount");

	ClassDB::bind_method(D_METHOD("set_noise_octaves", "value"), &YParticles3D::set_noise_octaves);
	ClassDB::bind_method(D_METHOD("get_noise_octaves"), &YParticles3D::get_noise_octaves);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_octaves", PROPERTY_HINT_RANGE, "1,8,1"), "set_noise_octaves", "get_noise_octaves");

	ClassDB::bind_method(D_METHOD("set_noise_lacunarity", "value"), &YParticles3D::set_noise_lacunarity);
	ClassDB::bind_method(D_METHOD("get_noise_lacunarity"), &YParticles3D::get_noise_lacunarity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_lacunarity", PROPERTY_HINT_RANGE, "1,8,0.01"), "set_noise_lacunarity", "get_noise_lacunarity");

	ADD_GROUP("Attractor", "");
	ClassDB::bind_method(D_METHOD("set_enable_attractor", "value"), &YParticles3D::set_enable_attractor);
	ClassDB::bind_method(D_METHOD("get_enable_attractor"), &YParticles3D::get_enable_attractor);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_attractor", PROPERTY_HINT_GROUP_ENABLE), "set_enable_attractor", "get_enable_attractor");
	ClassDB::bind_method(D_METHOD("set_attraction_target_mode", "value"), &YParticles3D::set_attraction_target_mode);
	ClassDB::bind_method(D_METHOD("get_attraction_target_mode"), &YParticles3D::get_attraction_target_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "attraction_target_mode", PROPERTY_HINT_ENUM, "Global Position,Node3D"), "set_attraction_target_mode", "get_attraction_target_mode");
	ClassDB::bind_method(D_METHOD("set_attractor_position", "value"), &YParticles3D::set_attractor_position);
	ClassDB::bind_method(D_METHOD("get_attractor_position"), &YParticles3D::get_attractor_position);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "attractor_position"), "set_attractor_position", "get_attractor_position");
	ClassDB::bind_method(D_METHOD("set_attraction_target", "value"), &YParticles3D::set_attraction_target);
	ClassDB::bind_method(D_METHOD("get_attraction_target"), &YParticles3D::get_attraction_target);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "attraction_target", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_attraction_target", "get_attraction_target");
	ClassDB::bind_method(D_METHOD("set_attraction_over_lifetime", "value"), &YParticles3D::set_attraction_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_attraction_over_lifetime"), &YParticles3D::get_attraction_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "attraction_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_attraction_over_lifetime", "get_attraction_over_lifetime");

	ADD_GROUP("Collision", "");
	ClassDB::bind_method(D_METHOD("set_enable_collision", "value"), &YParticles3D::set_enable_collision);
	ClassDB::bind_method(D_METHOD("get_enable_collision"), &YParticles3D::get_enable_collision);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_collision", PROPERTY_HINT_GROUP_ENABLE), "set_enable_collision", "get_enable_collision");

	ClassDB::bind_method(D_METHOD("set_collision_layer", "value"), &YParticles3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &YParticles3D::get_collision_layer);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");

	ClassDB::bind_method(D_METHOD("set_collision_radius_scale", "value"), &YParticles3D::set_collision_radius_scale);
	ClassDB::bind_method(D_METHOD("get_collision_radius_scale"), &YParticles3D::get_collision_radius_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_radius_scale", PROPERTY_HINT_RANGE, "0,4,0.001,or_greater"), "set_collision_radius_scale", "get_collision_radius_scale");

	ClassDB::bind_method(D_METHOD("set_collision_dampen", "value"), &YParticles3D::set_collision_dampen);
	ClassDB::bind_method(D_METHOD("get_collision_dampen"), &YParticles3D::get_collision_dampen);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_dampen", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_collision_dampen", "get_collision_dampen");

	ClassDB::bind_method(D_METHOD("set_collision_bounce", "value"), &YParticles3D::set_collision_bounce);
	ClassDB::bind_method(D_METHOD("get_collision_bounce"), &YParticles3D::get_collision_bounce);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_bounce", PROPERTY_HINT_RANGE, "0,2,0.001,or_greater"), "set_collision_bounce", "get_collision_bounce");

	ClassDB::bind_method(D_METHOD("set_collision_lifetime_loss", "value"), &YParticles3D::set_collision_lifetime_loss);
	ClassDB::bind_method(D_METHOD("get_collision_lifetime_loss"), &YParticles3D::get_collision_lifetime_loss);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_lifetime_loss", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_collision_lifetime_loss", "get_collision_lifetime_loss");

	ClassDB::bind_method(D_METHOD("set_collision_min_kill_speed", "value"), &YParticles3D::set_collision_min_kill_speed);
	ClassDB::bind_method(D_METHOD("get_collision_min_kill_speed"), &YParticles3D::get_collision_min_kill_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_min_kill_speed", PROPERTY_HINT_RANGE, "0,50,0.01,or_greater"), "set_collision_min_kill_speed", "get_collision_min_kill_speed");

	ClassDB::bind_method(D_METHOD("set_collision_quality", "value"), &YParticles3D::set_collision_quality);
	ClassDB::bind_method(D_METHOD("get_collision_quality"), &YParticles3D::get_collision_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_quality", PROPERTY_HINT_ENUM, "High,Medium,Low"), "set_collision_quality", "get_collision_quality");

	ClassDB::bind_method(D_METHOD("set_collision_voxel_size", "value"), &YParticles3D::set_collision_voxel_size);
	ClassDB::bind_method(D_METHOD("get_collision_voxel_size"), &YParticles3D::get_collision_voxel_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_voxel_size", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater"), "set_collision_voxel_size", "get_collision_voxel_size");

	ADD_GROUP("Sub Emitters", "");
	ClassDB::bind_method(D_METHOD("set_enable_sub_emitters", "value"), &YParticles3D::set_enable_sub_emitters);
	ClassDB::bind_method(D_METHOD("get_enable_sub_emitters"), &YParticles3D::get_enable_sub_emitters);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_sub_emitters", PROPERTY_HINT_GROUP_ENABLE), "set_enable_sub_emitters", "get_enable_sub_emitters");

	ClassDB::bind_method(D_METHOD("set_sub_emitters", "value"), &YParticles3D::set_sub_emitters);
	ClassDB::bind_method(D_METHOD("get_sub_emitters"), &YParticles3D::get_sub_emitters);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "sub_emitters"), "set_sub_emitters", "get_sub_emitters");

	ADD_GROUP("Texture Sheet Animation", "");
	ClassDB::bind_method(D_METHOD("set_texture_sheet_enabled", "value"), &YParticles3D::set_texture_sheet_enabled);
	ClassDB::bind_method(D_METHOD("get_texture_sheet_enabled"), &YParticles3D::get_texture_sheet_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "texture_sheet_enabled", PROPERTY_HINT_GROUP_ENABLE), "set_texture_sheet_enabled", "get_texture_sheet_enabled");
	ClassDB::bind_method(D_METHOD("set_h_frames", "value"), &YParticles3D::set_h_frames);
	ClassDB::bind_method(D_METHOD("get_h_frames"), &YParticles3D::get_h_frames);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "h_frames", PROPERTY_HINT_RANGE, "1,128,1"), "set_h_frames", "get_h_frames");

	ClassDB::bind_method(D_METHOD("set_v_frames", "value"), &YParticles3D::set_v_frames);
	ClassDB::bind_method(D_METHOD("get_v_frames"), &YParticles3D::get_v_frames);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "v_frames", PROPERTY_HINT_RANGE, "1,128,1"), "set_v_frames", "get_v_frames");

	ClassDB::bind_method(D_METHOD("set_tiles_mode", "value"), &YParticles3D::set_tiles_mode);
	ClassDB::bind_method(D_METHOD("get_tiles_mode"), &YParticles3D::get_tiles_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tiles_mode", PROPERTY_HINT_ENUM, "WholeSheet,SingleRow"), "set_tiles_mode", "get_tiles_mode");

	ClassDB::bind_method(D_METHOD("set_use_random_starting_tile", "value"), &YParticles3D::set_use_random_starting_tile);
	ClassDB::bind_method(D_METHOD("get_use_random_starting_tile"), &YParticles3D::get_use_random_starting_tile);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_random_starting_tile"), "set_use_random_starting_tile", "get_use_random_starting_tile");

	ClassDB::bind_method(D_METHOD("set_start_index_tile", "value"), &YParticles3D::set_start_index_tile);
	ClassDB::bind_method(D_METHOD("get_start_index_tile"), &YParticles3D::get_start_index_tile);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_index_tile", PROPERTY_HINT_RANGE, "0,127,1"), "set_start_index_tile", "get_start_index_tile");

	ClassDB::bind_method(D_METHOD("set_animation_cycles", "value"), &YParticles3D::set_animation_cycles);
	ClassDB::bind_method(D_METHOD("get_animation_cycles"), &YParticles3D::get_animation_cycles);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_cycles"), "set_animation_cycles", "get_animation_cycles");

	ClassDB::bind_method(D_METHOD("set_frame_over_time", "value"), &YParticles3D::set_frame_over_time);
	ClassDB::bind_method(D_METHOD("get_frame_over_time"), &YParticles3D::get_frame_over_time);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "frame_over_time", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_frame_over_time", "get_frame_over_time");

	ADD_GROUP("Trails", "");
	ClassDB::bind_method(D_METHOD("set_enable_trails", "value"), &YParticles3D::set_enable_trails);
	ClassDB::bind_method(D_METHOD("get_enable_trails"), &YParticles3D::get_enable_trails);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_trails", PROPERTY_HINT_GROUP_ENABLE), "set_enable_trails", "get_enable_trails");
	ClassDB::bind_method(D_METHOD("set_trail_ratio", "value"), &YParticles3D::set_trail_ratio);
	ClassDB::bind_method(D_METHOD("get_trail_ratio"), &YParticles3D::get_trail_ratio);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "trail_ratio", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_trail_ratio", "get_trail_ratio");
	ClassDB::bind_method(D_METHOD("set_trail_lifetime_mode", "value"), &YParticles3D::set_trail_lifetime_mode);
	ClassDB::bind_method(D_METHOD("get_trail_lifetime_mode"), &YParticles3D::get_trail_lifetime_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "trail_lifetime_mode", PROPERTY_HINT_ENUM, "Constant,Curve"), "set_trail_lifetime_mode", "get_trail_lifetime_mode");
	ClassDB::bind_method(D_METHOD("set_trail_lifetime", "value"), &YParticles3D::set_trail_lifetime);
	ClassDB::bind_method(D_METHOD("get_trail_lifetime"), &YParticles3D::get_trail_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "trail_lifetime", PROPERTY_HINT_RANGE, "0,10,0.001,or_greater"), "set_trail_lifetime", "get_trail_lifetime");
	ClassDB::bind_method(D_METHOD("set_trail_lifetime_curve", "value"), &YParticles3D::set_trail_lifetime_curve);
	ClassDB::bind_method(D_METHOD("get_trail_lifetime_curve"), &YParticles3D::get_trail_lifetime_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "trail_lifetime_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_trail_lifetime_curve", "get_trail_lifetime_curve");
	ClassDB::bind_method(D_METHOD("set_trail_min_vertex_distance", "value"), &YParticles3D::set_trail_min_vertex_distance);
	ClassDB::bind_method(D_METHOD("get_trail_min_vertex_distance"), &YParticles3D::get_trail_min_vertex_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "trail_min_vertex_distance", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"), "set_trail_min_vertex_distance", "get_trail_min_vertex_distance");
	ClassDB::bind_method(D_METHOD("set_trail_world_space", "value"), &YParticles3D::set_trail_world_space);
	ClassDB::bind_method(D_METHOD("get_trail_world_space"), &YParticles3D::get_trail_world_space);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_world_space"), "set_trail_world_space", "get_trail_world_space");
	ClassDB::bind_method(D_METHOD("set_trail_die_with_particles", "value"), &YParticles3D::set_trail_die_with_particles);
	ClassDB::bind_method(D_METHOD("get_trail_die_with_particles"), &YParticles3D::get_trail_die_with_particles);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_die_with_particles"), "set_trail_die_with_particles", "get_trail_die_with_particles");
	ClassDB::bind_method(D_METHOD("set_trail_size_affects_width", "value"), &YParticles3D::set_trail_size_affects_width);
	ClassDB::bind_method(D_METHOD("get_trail_size_affects_width"), &YParticles3D::get_trail_size_affects_width);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_size_affects_width"), "set_trail_size_affects_width", "get_trail_size_affects_width");
	ClassDB::bind_method(D_METHOD("set_trail_size_affects_lifetime", "value"), &YParticles3D::set_trail_size_affects_lifetime);
	ClassDB::bind_method(D_METHOD("get_trail_size_affects_lifetime"), &YParticles3D::get_trail_size_affects_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_size_affects_lifetime"), "set_trail_size_affects_lifetime", "get_trail_size_affects_lifetime");
	ClassDB::bind_method(D_METHOD("set_trail_inherit_particle_color", "value"), &YParticles3D::set_trail_inherit_particle_color);
	ClassDB::bind_method(D_METHOD("get_trail_inherit_particle_color"), &YParticles3D::get_trail_inherit_particle_color);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_inherit_particle_color"), "set_trail_inherit_particle_color", "get_trail_inherit_particle_color");
	ClassDB::bind_method(D_METHOD("set_trail_texture_mode", "value"), &YParticles3D::set_trail_texture_mode);
	ClassDB::bind_method(D_METHOD("get_trail_texture_mode"), &YParticles3D::get_trail_texture_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "trail_texture_mode", PROPERTY_HINT_ENUM, "Stretch,Tile,RepeatPerSegment,DistributePerSegment"), "set_trail_texture_mode", "get_trail_texture_mode");
	ClassDB::bind_method(D_METHOD("set_trail_color_over_lifetime", "value"), &YParticles3D::set_trail_color_over_lifetime);
	ClassDB::bind_method(D_METHOD("get_trail_color_over_lifetime"), &YParticles3D::get_trail_color_over_lifetime);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "trail_color_over_lifetime", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_trail_color_over_lifetime", "get_trail_color_over_lifetime");
	ClassDB::bind_method(D_METHOD("set_trail_color_over_trail", "value"), &YParticles3D::set_trail_color_over_trail);
	ClassDB::bind_method(D_METHOD("get_trail_color_over_trail"), &YParticles3D::get_trail_color_over_trail);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "trail_color_over_trail", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_trail_color_over_trail", "get_trail_color_over_trail");
	ClassDB::bind_method(D_METHOD("set_trail_width_over_trail", "value"), &YParticles3D::set_trail_width_over_trail);
	ClassDB::bind_method(D_METHOD("get_trail_width_over_trail"), &YParticles3D::get_trail_width_over_trail);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "trail_width_over_trail", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_trail_width_over_trail", "get_trail_width_over_trail");
	ClassDB::bind_method(D_METHOD("set_trail_texture", "value"), &YParticles3D::set_trail_texture);
	ClassDB::bind_method(D_METHOD("get_trail_texture"), &YParticles3D::get_trail_texture);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "trail_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_trail_texture", "get_trail_texture");

	ADD_GROUP("Rendering", "");
	ClassDB::bind_method(D_METHOD("set_particle_texture", "value"), &YParticles3D::set_particle_texture);
	ClassDB::bind_method(D_METHOD("get_particle_texture"), &YParticles3D::get_particle_texture);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "particle_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_particle_texture", "get_particle_texture");

	ClassDB::bind_method(D_METHOD("set_use_start_color_gradient", "value"), &YParticles3D::set_use_start_color_gradient);
	ClassDB::bind_method(D_METHOD("get_use_start_color_gradient"), &YParticles3D::get_use_start_color_gradient);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "start_color_mode"), "set_use_start_color_gradient", "get_use_start_color_gradient");

	ClassDB::bind_method(D_METHOD("set_start_color_gradient", "value"), &YParticles3D::set_start_color_gradient);
	ClassDB::bind_method(D_METHOD("get_start_color_gradient"), &YParticles3D::get_start_color_gradient);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_color_gradient", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_start_color_gradient", "get_start_color_gradient");

	ClassDB::bind_method(D_METHOD("set_start_color_gradient_secondary", "value"), &YParticles3D::set_start_color_gradient_secondary);
	ClassDB::bind_method(D_METHOD("get_start_color_gradient_secondary"), &YParticles3D::get_start_color_gradient_secondary);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "start_color_gradient_secondary", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_start_color_gradient_secondary", "get_start_color_gradient_secondary");

	ClassDB::bind_method(D_METHOD("set_start_color_use_two_gradients", "value"), &YParticles3D::set_start_color_use_two_gradients);
	ClassDB::bind_method(D_METHOD("get_start_color_use_two_gradients"), &YParticles3D::get_start_color_use_two_gradients);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "start_color_use_two_gradients"), "set_start_color_use_two_gradients", "get_start_color_use_two_gradients");

	ClassDB::bind_method(D_METHOD("set_tint_color", "value"), &YParticles3D::set_tint_color);
	ClassDB::bind_method(D_METHOD("get_tint_color"), &YParticles3D::get_tint_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_color"), "set_tint_color", "get_tint_color");

	ClassDB::bind_method(D_METHOD("set_billboard_mode", "value"), &YParticles3D::set_billboard_mode);
	ClassDB::bind_method(D_METHOD("get_billboard_mode"), &YParticles3D::get_billboard_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard_mode", PROPERTY_HINT_ENUM, "Standard,Stretched,None,Vertical,StretchedVertical"), "set_billboard_mode", "get_billboard_mode");

	ClassDB::bind_method(D_METHOD("set_render_alignment", "value"), &YParticles3D::set_render_alignment);
	ClassDB::bind_method(D_METHOD("get_render_alignment"), &YParticles3D::get_render_alignment);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_alignment", PROPERTY_HINT_ENUM, "View,World,Local,Facing,Velocity"), "set_render_alignment", "get_render_alignment");

	ClassDB::bind_method(D_METHOD("set_velocity_stretch", "value"), &YParticles3D::set_velocity_stretch);
	ClassDB::bind_method(D_METHOD("get_velocity_stretch"), &YParticles3D::get_velocity_stretch);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity_stretch"), "set_velocity_stretch", "get_velocity_stretch");

	ClassDB::bind_method(D_METHOD("set_length_stretch", "value"), &YParticles3D::set_length_stretch);
	ClassDB::bind_method(D_METHOD("get_length_stretch"), &YParticles3D::get_length_stretch);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length_stretch"), "set_length_stretch", "get_length_stretch");

	ClassDB::bind_method(D_METHOD("set_align_to_velocity", "value"), &YParticles3D::set_align_to_velocity);
	ClassDB::bind_method(D_METHOD("get_align_to_velocity"), &YParticles3D::get_align_to_velocity);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "align_to_velocity"), "set_align_to_velocity", "get_align_to_velocity");

	ClassDB::bind_method(D_METHOD("set_align_offset_degrees", "value"), &YParticles3D::set_align_offset_degrees);
	ClassDB::bind_method(D_METHOD("get_align_offset_degrees"), &YParticles3D::get_align_offset_degrees);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "align_offset_degrees", PROPERTY_HINT_RANGE, "-360,360,0.1,suffix:deg"), "set_align_offset_degrees", "get_align_offset_degrees");

	ClassDB::bind_method(D_METHOD("set_blend_mode", "value"), &YParticles3D::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &YParticles3D::get_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Mix,Add,Subtract,Multiply,PremultipliedAlpha"), "set_blend_mode", "get_blend_mode");

	ClassDB::bind_method(D_METHOD("set_render_priority", "value"), &YParticles3D::set_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_priority"), &YParticles3D::get_render_priority);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority"), "set_render_priority", "get_render_priority");

	ClassDB::bind_method(D_METHOD("set_sampling_filter", "value"), &YParticles3D::set_sampling_filter);
	ClassDB::bind_method(D_METHOD("get_sampling_filter"), &YParticles3D::get_sampling_filter);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sampling_filter", PROPERTY_HINT_ENUM, "Linear,Nearest"), "set_sampling_filter", "get_sampling_filter");

	ClassDB::bind_method(D_METHOD("set_rendering_layer", "value"), &YParticles3D::set_rendering_layer);
	ClassDB::bind_method(D_METHOD("get_rendering_layer"), &YParticles3D::get_rendering_layer);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rendering_layer", PROPERTY_HINT_LAYERS_3D_RENDER), "set_rendering_layer", "get_rendering_layer");

	ClassDB::bind_method(D_METHOD("set_visibility_aabb", "value"), &YParticles3D::set_visibility_aabb);
	ClassDB::bind_method(D_METHOD("get_visibility_aabb"), &YParticles3D::get_visibility_aabb);
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "visibility_aabb"), "set_visibility_aabb", "get_visibility_aabb");

	ClassDB::bind_method(D_METHOD("set_override_material", "value"), &YParticles3D::set_override_material);
	ClassDB::bind_method(D_METHOD("get_override_material"), &YParticles3D::get_override_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "override_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_override_material", "get_override_material");

	ClassDB::bind_method(D_METHOD("set_custom_mesh", "value"), &YParticles3D::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &YParticles3D::get_custom_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_custom_mesh", "get_custom_mesh");

	ADD_SIGNAL(MethodInfo("finished_burst"));

	BIND_ENUM_CONSTANT(BLEND_MODE_MIX);
	BIND_ENUM_CONSTANT(BLEND_MODE_ADD);
	BIND_ENUM_CONSTANT(BLEND_MODE_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_MODE_MULTIPLY);
	BIND_ENUM_CONSTANT(BLEND_MODE_PREMULTIPLIED_ALPHA);

	BIND_ENUM_CONSTANT(SAMPLING_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(SAMPLING_FILTER_NEAREST);

	BIND_ENUM_CONSTANT(BILLBOARD_MODE_STANDARD);
	BIND_ENUM_CONSTANT(BILLBOARD_MODE_STRETCHED);
	BIND_ENUM_CONSTANT(BILLBOARD_MODE_DISABLED);
	BIND_ENUM_CONSTANT(BILLBOARD_MODE_VERTICAL);
	BIND_ENUM_CONSTANT(BILLBOARD_MODE_STRETCHED_VERTICAL);

	BIND_ENUM_CONSTANT(TRAIL_TEXTURE_MODE_STRETCH);
	BIND_ENUM_CONSTANT(TRAIL_TEXTURE_MODE_TILE);
	BIND_ENUM_CONSTANT(TRAIL_TEXTURE_MODE_REPEAT_PER_SEGMENT);
	BIND_ENUM_CONSTANT(TRAIL_TEXTURE_MODE_DISTRIBUTE_PER_SEGMENT);

	BIND_ENUM_CONSTANT(TEXTURE_SHEET_TILES_WHOLE_SHEET);
	BIND_ENUM_CONSTANT(TEXTURE_SHEET_TILES_SINGLE_ROW);

	BIND_ENUM_CONSTANT(EMISSION_SHAPE_CONE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_HEMISPHERE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_CIRCLE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_EDGE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_MESH);

	BIND_ENUM_CONSTANT(EMIT_FROM_BASE);
	BIND_ENUM_CONSTANT(EMIT_FROM_VOLUME);

	BIND_ENUM_CONSTANT(ARC_MODE_RANDOM);
	BIND_ENUM_CONSTANT(ARC_MODE_LOOP);
	BIND_ENUM_CONSTANT(ARC_MODE_PING_PONG);
	BIND_ENUM_CONSTANT(ARC_MODE_BURST_SPREAD);
}
