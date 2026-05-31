#ifndef YPARTICLES3D_H
#define YPARTICLES3D_H

#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

using namespace godot;

class YParticles3D : public Node3D {
	GDCLASS(YParticles3D, Node3D);

public:
	enum BlendMode {
		BLEND_MODE_MIX,
		BLEND_MODE_ADD,
		BLEND_MODE_SUBTRACT,
		BLEND_MODE_MULTIPLY,
		BLEND_MODE_PREMULTIPLIED_ALPHA,
	};

	enum SamplingFilter {
		SAMPLING_FILTER_LINEAR = 0,
		SAMPLING_FILTER_NEAREST = 1,
	};

	enum BillboardMode {
		BILLBOARD_MODE_STANDARD = 0,
		BILLBOARD_MODE_STRETCHED = 1,
		BILLBOARD_MODE_DISABLED = 2,
		BILLBOARD_MODE_VERTICAL = 3,
		BILLBOARD_MODE_STRETCHED_VERTICAL = 4,
	};

	enum RenderAlignment {
		RENDER_ALIGNMENT_VIEW = 0,
		RENDER_ALIGNMENT_WORLD = 1,
		RENDER_ALIGNMENT_LOCAL = 2,
		RENDER_ALIGNMENT_FACING = 3,
		RENDER_ALIGNMENT_VELOCITY = 4,
	};

	enum TrailTextureMode {
		TRAIL_TEXTURE_MODE_STRETCH = 0,
		TRAIL_TEXTURE_MODE_TILE = 1,
		TRAIL_TEXTURE_MODE_REPEAT_PER_SEGMENT = 2,
		TRAIL_TEXTURE_MODE_DISTRIBUTE_PER_SEGMENT = 3,
	};

	enum TextureSheetTiles {
		TEXTURE_SHEET_TILES_WHOLE_SHEET,
		TEXTURE_SHEET_TILES_SINGLE_ROW,
	};

	enum EmissionShape {
		EMISSION_SHAPE_CONE,
		EMISSION_SHAPE_SPHERE,
		EMISSION_SHAPE_HEMISPHERE,
		EMISSION_SHAPE_BOX,
		EMISSION_SHAPE_CIRCLE,
		EMISSION_SHAPE_EDGE,
		EMISSION_SHAPE_MESH,
	};

	enum EmitFrom {
		EMIT_FROM_BASE,
		EMIT_FROM_VOLUME,
	};

	enum ArcMode {
		ARC_MODE_RANDOM,
		ARC_MODE_LOOP,
		ARC_MODE_PING_PONG,
		ARC_MODE_BURST_SPREAD,
	};

	enum CollisionQuality {
		COLLISION_QUALITY_HIGH,
		COLLISION_QUALITY_MEDIUM,
		COLLISION_QUALITY_LOW,
	};

	enum AttractionTargetMode {
		ATTRACTION_TARGET_MODE_GLOBAL_POSITION,
		ATTRACTION_TARGET_MODE_NODE3D,
	};

	enum SubEmitterCondition {
		SUB_EMITTER_CONDITION_BIRTH,
		SUB_EMITTER_CONDITION_COLLISION,
		SUB_EMITTER_CONDITION_DEATH,
	};

	enum SubEmitterInheritFlags {
		SUB_EMITTER_INHERIT_NOTHING = 0,
		SUB_EMITTER_INHERIT_COLOR = 1 << 0,
		SUB_EMITTER_INHERIT_SIZE = 1 << 1,
		SUB_EMITTER_INHERIT_ROTATION = 1 << 2,
		SUB_EMITTER_INHERIT_LIFETIME = 1 << 3,
		SUB_EMITTER_INHERIT_DURATION = 1 << 4,
		SUB_EMITTER_INHERIT_EVERYTHING = SUB_EMITTER_INHERIT_COLOR | SUB_EMITTER_INHERIT_SIZE | SUB_EMITTER_INHERIT_ROTATION | SUB_EMITTER_INHERIT_LIFETIME | SUB_EMITTER_INHERIT_DURATION,
	};

private:
	struct Particle {
		bool dead = true;
		bool visible = true;
		bool trail_only = false;
		bool trail_enabled = false;
		Vector3 position;
		Vector3 base_velocity;
		Vector3 gravity_velocity;
		Vector3 force_velocity;
		Vector3 inherited_velocity;
		Vector3 direction = Vector3(0.0, 1.0, 0.0);
		Vector3 scale = Vector3(0.15, 0.15, 0.15);
		Vector3 rotation = Vector3();
		float hue_offset = 0.0f;
		float random_a = 0.0f;
		float random_b = 0.0f;
		float start_alpha = 1.0f;
		float distance = 0.0f;
		float lifetime = 1.0f;
		float creation_time = 0.0f;
		Vector3 creation_position;
		int index = 0;
		int slot_index = -1;
		Vector3 last_position;
		float burst_spot = 0.0f;
		int tilesheet_starting_tile = 0;
		float orbit_angle = 0.0f;
	};

	struct TrailPoint {
		Vector3 position;
		float expiry_time = 0.0f;
		float particle_normalized = 0.0f;
		float size = 1.0f;
		Color particle_color = Color(1, 1, 1, 1);
	};

	struct TrailState {
		Vector<TrailPoint> points;

		void clear() {
			points.clear();
		}
	};

	struct BurstInstance {
		float time = 0.0f;
		int min_particles = 10;
		int max_particles = 10;
		int min_cycles = 1;
		int max_cycles = 1;
	float particle_interval = 0.0f;
	float probability = 1.0f;

		int remaining_cycles = 0;
		float next_cycle_time = 0.0f;
		int particles_in_burst = 0;
		int current_burst_index = 0;

		void initialize(RandomNumberGenerator *p_rng);
		void reset_burst(RandomNumberGenerator *p_rng);
		int process(float p_current_time, RandomNumberGenerator *p_rng, int &r_start_index, int &r_total_in_burst);
	};

	struct CollisionCacheEntry {
		bool has_plane = false;
		bool known_empty = false;
		Plane plane;
	};

	static void _bind_methods();
	void _notification(int p_what);

	RID _multimesh;
	RID _instance;
	RID _trail_mesh;
	RID _trail_instance;

	Vector<Particle> _particles;
	Vector<TrailState> _trail_states;
	PackedFloat32Array _buffer;
	Vector<BurstInstance> _active_bursts;

	Ref<RandomNumberGenerator> _rng;
	Ref<FastNoiseLite> _noise_generator;
	Ref<Material> _shared_material;
	Ref<Material> _trail_material;

	bool _playing = false;
	bool _material_dirty = true;
	bool _core_params_dirty = true;
	bool _shared_material_enabled = true;
	float _time = 0.0f;
	float _emission_time = 0.0f;
	double _fixed_fps_remainder = 0.0;
	Vector2 _emission_accumulator;
	Vector3 _last_position;
	bool _has_last_position = false;
	Transform3D _last_transform;
	int _visible_count = 0;
	float _current_arc_rotation = 0.0f;
	int _arc_direction = 1;
	int _collision_queries_used_this_frame = 0;
	float _trail_debug_next_time = 0.0f;
	HashMap<int64_t, CollisionCacheEntry> _collision_plane_cache;
	Vector<Vector3> _emission_mesh_face_centers;
	bool _emission_mesh_cache_dirty = true;
	bool _pending_sub_emitter_inherit = false;
	bool _spawned_as_sub_emitter_instance = false;
	Color _pending_sub_emitter_color_multiplier = Color(1, 1, 1, 1);
	Vector3 _pending_sub_emitter_size_multiplier = Vector3(1, 1, 1);
	Vector3 _pending_sub_emitter_rotation_offset = Vector3();
	float _pending_sub_emitter_lifetime_multiplier = 1.0f;
	bool _pending_sub_emitter_inherit_duration = false;
	float _pending_sub_emitter_source_time = 0.0f;
	float _pending_sub_emitter_source_emission_time = 0.0f;
	Vector3 _emitter_velocity_world = Vector3();

	float duration = 1.0f;
	int start_lifetime_mode = 0;
	float start_lifetime_constant = 1.0f;
	Vector2 start_lifetime_random = Vector2(0.5f, 1.0f);
	Ref<Curve> start_lifetime_curve;

	int start_speed_mode = 0;
	float start_speed_constant = 5.0f;
	Vector2 start_speed_random = Vector2(1.0f, 5.0f);

	Vector3 gravity;

	int start_size_mode = 0;
	Vector2 start_size_constant = Vector2(0.15f, 0.15f);
	Vector4 start_size_random = Vector4(0.5f, 0.5f, 1.5f, 1.5f);
	Ref<Curve> start_size_curve;
	Ref<Curve> start_size_curve_min;
	Ref<Curve> start_size_curve_max;
	Vector2 start_size_square_random = Vector2(0.15f, 0.3f);
	Vector3 start_size_constant_3d = Vector3(0.15f, 0.15f, 0.15f);
	Vector3 start_size_random_min_3d = Vector3(0.15f, 0.15f, 0.15f);
	Vector3 start_size_random_max_3d = Vector3(0.3f, 0.3f, 0.3f);
	Ref<Curve> start_size_x_curve;
	Ref<Curve> start_size_y_curve;
	Ref<Curve> start_size_z_curve;
	Ref<Curve> start_size_x_curve_min;
	Ref<Curve> start_size_y_curve_min;
	Ref<Curve> start_size_z_curve_min;

	int start_rotation_degrees_mode = 0;
	float start_rotation_degrees_constant = 0.0f;
	Vector2 start_rotation_degrees_random = Vector2(0.0f, 90.0f);
	Ref<Curve> start_rotation_degrees_curve;
	Vector3 start_rotation_degrees_constant_3d = Vector3();
	Vector3 start_rotation_degrees_random_min_3d = Vector3();
	Vector3 start_rotation_degrees_random_max_3d = Vector3(90.0f, 90.0f, 90.0f);
	Ref<Curve> start_rotation_degrees_x_curve;
	Ref<Curve> start_rotation_degrees_y_curve;
	Ref<Curve> start_rotation_degrees_z_curve;

	bool use_world_space = false;
	bool play_on_start = true;
	bool loop = true;
	bool play_in_reverse = false;
	float start_delay = 0.0f;
	float start_delay_percentage = 0.0f;
	bool destroy_on_finish = false;
	bool debugging = false;

	bool emitting = true;
	int max_particles = 400;
	int max_emissions_per_frame = 100;
	int rate_over_time_mode = 0;
	float rate_over_time = 10.0f;
	Ref<Curve> rate_over_time_curve;
	float rate_over_distance = 10.0f;
	Array bursts;

	bool enable_shape = false;
	EmissionShape shape_type = EMISSION_SHAPE_CONE;
	float radius = 0.0f;
	float radius_thickness = 1.0f;
	float angle = 0.0f;
	Vector3 box_extents = Vector3(1.0f, 1.0f, 1.0f);
	Ref<Mesh> emission_mesh;
	Vector3 emission_mesh_scale = Vector3(1.0f, 1.0f, 1.0f);
	float random_direction = 0.0f;
	float spherize_direction = 0.0f;
	EmitFrom emit_from = EMIT_FROM_BASE;
	float shape_length = 1.0f;
	float arc_degrees = 360.0f;
	ArcMode arc_mode = ARC_MODE_RANDOM;
	float arc_spread = 0.0f;
	int arc_speed_mode = 0;
	float arc_speed_constant = 1.0f;
	Ref<Curve> arc_speed_curve;
	bool direction_in_world_space = false;
	bool invert_direction = false;
	Vector3 position_offset;
	Vector3 rotation_offset;

	bool enable_size_over_lifetime = false;
	Ref<Curve> size_over_lifetime;
	Ref<Curve> size_over_lifetime_min;
	Ref<Curve> width_over_lifetime;
	Ref<Curve> height_over_lifetime;
	Ref<Curve> depth_over_lifetime;
	bool size_over_lifetime_use_two_curves = false;

	bool enable_velocity_over_lifetime = false;
	int velocity_over_lifetime_mode = 0;
	Ref<Curve> velocity_over_lifetime;
	Ref<Curve> velocity_over_lifetime_min;
	Ref<Curve> velocity_over_lifetime_x;
	Ref<Curve> velocity_over_lifetime_x_min;
	Ref<Curve> velocity_over_lifetime_y;
	Ref<Curve> velocity_over_lifetime_y_min;
	Ref<Curve> velocity_over_lifetime_z;
	Ref<Curve> velocity_over_lifetime_z_min;
	Ref<Curve> offset_over_lifetime;
	bool velocity_over_lifetime_use_two_curves = false;
	bool velocity_in_world_space = false;

	bool enable_force_over_lifetime = false;
	int force_over_lifetime_mode = 0;
	Ref<Curve> force_over_lifetime;
	Ref<Curve> force_over_lifetime_x;
	Ref<Curve> force_over_lifetime_y;
	Ref<Curve> force_over_lifetime_z;
	Vector3 force_over_lifetime_constant = Vector3();
	Vector3 force_over_lifetime_random_min = Vector3();
	Vector3 force_over_lifetime_random_max = Vector3();
	bool force_in_world_space = false;

	bool enable_limit_velocity_over_lifetime = false;
	int limit_velocity_over_lifetime_speed_mode = 0;
	float limit_velocity_over_lifetime_speed = 1.0f;
	Ref<Curve> limit_velocity_over_lifetime_speed_curve;
	Vector3 limit_velocity_over_lifetime_speed_axis = Vector3(1.0f, 1.0f, 1.0f);
	Ref<Curve> limit_velocity_over_lifetime_speed_x_curve;
	Ref<Curve> limit_velocity_over_lifetime_speed_y_curve;
	Ref<Curve> limit_velocity_over_lifetime_speed_z_curve;
	float limit_velocity_over_lifetime_dampen = 1.0f;

	bool enable_noise = false;
	float noise_strength = 0.0f;
	int noise_strength_mode = 0;
	Ref<Curve> noise_strength_curve;
	Ref<Curve> noise_strength_x;
	Ref<Curve> noise_strength_y;
	Ref<Curve> noise_strength_z;
	float noise_scale = 1.0f;
	Vector3 noise_scroll_speed;
	float noise_position_amount = 1.0f;
	float noise_rotation_amount = 0.0f;
	float noise_size_amount = 0.0f;
	int noise_octaves = 3;
	float noise_lacunarity = 2.0f;

	bool enable_attractor = false;
	int attraction_target_mode = ATTRACTION_TARGET_MODE_GLOBAL_POSITION;
	Vector3 attractor_position;
	NodePath attraction_target;
	Ref<Curve> attraction_over_lifetime;

	bool enable_collision = false;
	uint32_t collision_layer = 1;
	float collision_radius_scale = 1.0f;
	float collision_dampen = 0.0f;
	float collision_bounce = 0.0f;
	float collision_lifetime_loss = 0.0f;
	float collision_min_kill_speed = 0.0f;
	CollisionQuality collision_quality = COLLISION_QUALITY_MEDIUM;
	float collision_voxel_size = 0.5f;

	bool enable_sub_emitters = false;
	Array sub_emitters;
	Vector<uint64_t> _spawned_sub_emitter_instances;

	bool enable_rotation_over_lifetime = false;
	Ref<Curve> rotation_over_lifetime;
	Ref<Curve> orbit_over_lifetime;
	Vector3 orbit_around_axis = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 rotation_over_lifetime_axis = Vector3(0.0f, 0.0f, 1.0f);
	bool enable_rotation_by_speed = false;
	int rotation_by_speed_mode = 0;
	Ref<Curve> rotation_by_speed;
	Ref<Curve> rotation_by_speed_x;
	Ref<Curve> rotation_by_speed_y;
	Ref<Curve> rotation_by_speed_z;
	Vector2 rotation_by_speed_range = Vector2(0.0f, 1.0f);

	bool enable_inherit_velocity = false;
	int inherit_velocity_mode = 0;
	float inherit_velocity_multiplier = 1.0f;
	Ref<Curve> inherit_velocity_curve;

	bool enable_color_over_lifetime = false;
	Ref<GradientTexture1D> color_over_lifetime;
	Ref<GradientTexture1D> color_over_lifetime_secondary;
	Ref<Curve> alpha_over_lifetime;
	Ref<Curve> alpha_over_lifetime_secondary;
	bool color_over_lifetime_use_two_gradients = false;
	bool use_start_color_gradient = false;
	Ref<GradientTexture1D> start_color_gradient;
	Ref<GradientTexture1D> start_color_gradient_secondary;
	bool start_color_use_two_gradients = false;
	float starting_hue = 0.0f;
	float hue_variation = 0.0f;

	bool texture_sheet_enabled = false;
	int h_frames = 1;
	int v_frames = 1;
	TextureSheetTiles tiles_mode = TEXTURE_SHEET_TILES_WHOLE_SHEET;
	bool use_random_starting_tile = true;
	int start_index_tile = 0;
	float animation_cycles = 1.0f;
	Ref<Curve> frame_over_time;

	Ref<Texture2D> particle_texture;
	bool enable_trails = false;
	float trail_ratio = 1.0f;
	int trail_lifetime_mode = 0;
	float trail_lifetime = 1.0f;
	Ref<Curve> trail_lifetime_curve;
	float trail_min_vertex_distance = 0.1f;
	bool trail_world_space = false;
	bool trail_die_with_particles = true;
	bool trail_size_affects_width = true;
	bool trail_size_affects_lifetime = false;
	bool trail_inherit_particle_color = true;
	TrailTextureMode trail_texture_mode = TRAIL_TEXTURE_MODE_STRETCH;
	Ref<GradientTexture1D> trail_color_over_lifetime;
	Ref<GradientTexture1D> trail_color_over_trail;
	Ref<Curve> trail_width_over_trail;
	Ref<Texture2D> trail_texture;
	Color tint_color = Color(1, 1, 1, 1);
	BillboardMode billboard_mode = BILLBOARD_MODE_STANDARD;
	RenderAlignment render_alignment = RENDER_ALIGNMENT_VIEW;
	float velocity_stretch = 0.0f;
	float length_stretch = 0.0f;
	bool align_to_velocity = false;
	float align_offset_degrees = 0.0f;
	BlendMode blend_mode = BLEND_MODE_MIX;
	int render_priority = 0;
	SamplingFilter sampling_filter = SAMPLING_FILTER_LINEAR;
	uint32_t rendering_layer = 1;
	AABB visibility_aabb;
	Ref<Material> override_material;
	Ref<Mesh> custom_mesh;

	float playback_speed = 1.0f;
	bool paused = false;
	int fixed_fps = 0;

	float _sample_curve(const Ref<Curve> &p_curve, float p_t, float p_default) const;
	float _sample_emission_rate() const;
	float _pick_start_lifetime() const;
	float _pick_start_speed() const;
	Vector3 _pick_start_size(const Particle &p_particle) const;
	Vector3 _pick_start_rotation_degrees(const Particle &p_particle) const;
	Color _sample_particle_color(const Particle &p_particle, float p_normalized) const;
	float _sample_alpha_over_lifetime(const Particle &p_particle, float p_normalized) const;
	float _sample_inherit_velocity_multiplier(float p_normalized) const;
	Dictionary _make_default_sub_emitter_entry() const;
	int _normalize_sub_emitter_inherit_flags(int p_flags) const;
	Vector3 _get_random_unit_vector() const;
	float _next_arc_angle(float p_burst_spot);
	Vector3 _sample_velocity_over_lifetime(const Particle &p_particle, const Vector3 &p_base_velocity, const Vector3 &p_fallback_direction, float p_normalized) const;
	Vector3 _sample_force_over_lifetime(const Particle &p_particle, float p_normalized, const Basis &p_global_basis) const;
	Vector3 _sample_limit_velocity_over_lifetime(const Vector3 &p_velocity, float p_normalized) const;
	Vector3 _sample_noise_strength(float p_normalized) const;
	float _sample_fbm_noise(const Vector3 &p_position, const Vector3 &p_offset) const;
	Vector3 _sample_noise_velocity(const Vector3 &p_position) const;
	Vector3 _get_attraction_target_position() const;
	Vector3 _sample_particle_scale(const Particle &p_particle, float p_normalized) const;
	float _sample_trail_lifetime(float p_normalized) const;
	int _get_collision_query_budget() const;
	int64_t _get_collision_voxel_key(const Vector3 &p_position) const;
	bool _lookup_collision_plane(const Vector3 &p_position, Plane &r_plane) const;
	bool _is_collision_voxel_known_empty(const Vector3 &p_position) const;
	void _store_collision_plane(const Vector3 &p_position, const Plane &p_plane);
	void _store_collision_empty(const Vector3 &p_position);
	bool _query_collision_plane(const Vector3 &p_from, const Vector3 &p_to, Plane &r_plane, Vector3 &r_point);
	void _apply_particle_collision(Particle &r_particle, Vector3 &r_world_position, Vector3 &r_world_velocity, float p_normalized, double p_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv);
	bool _is_registered_sub_emitter_node(const YParticles3D *p_candidate) const;
	bool _is_sub_emitter_template_for_parent() const;
	void _cleanup_spawned_sub_emitter_instances();
	void _prepare_sub_emitter_instance(YParticles3D *p_instance, const Particle &p_particle, float p_normalized) const;
	void _trigger_sub_emitters(SubEmitterCondition p_event, const Particle &p_particle, float p_normalized, const Vector3 &p_world_position);

	RID _create_shared_quad_mesh();
	RID _get_particle_mesh() const;
	Ref<Material> _create_material() const;
	Ref<Material> _create_trail_material() const;
	void _update_material();
	void _update_trail_material();
	void _create_multimesh();
	void _create_trail_rendering();
	void _free_rendering();
	void _clear_buffer();
	void _update_instance_transform();
	void _update_trail_instance_transform();
	void _update_material_shader_params();
	void _update_trail_mesh(const Basis &p_global_basis, const Vector3 &p_global_position);
	void _clear_trails();

	void _initialize_particle(Particle &r_particle);
	void _initialize_cone_particle(Particle &r_particle);
	void _initialize_sphere_particle(Particle &r_particle, bool p_hemisphere);
	void _initialize_box_particle(Particle &r_particle);
	void _initialize_circle_particle(Particle &r_particle);
	void _initialize_edge_particle(Particle &r_particle);
	void _initialize_mesh_particle(Particle &r_particle);
	void _update_emission_mesh_cache();
	void _emit_particle(float p_burst_spot = 0.0f, bool p_has_override_position = false, const Vector3 &p_override_position = Vector3());
	void _update_particle(Particle &r_particle, float p_normalized, double p_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position);
	Vector3 _get_particle_world_position(const Particle &p_particle, const Basis &p_global_basis, const Vector3 &p_global_position) const;
	Vector3 _world_to_trail_space(const Vector3 &p_world_position, const Basis &p_global_basis_inv, const Vector3 &p_global_position) const;
	void _append_trail_point(Particle &r_particle, TrailState &r_trail_state, float p_normalized, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position);
	void _trim_trail_points(TrailState &r_trail_state);
	void _write_particle_to_buffer(const Particle &p_particle, float p_normalized, const Basis &p_global_basis, const Basis &p_global_basis_inv);
	void _process_simulation_step(double p_raw_delta, double p_scaled_delta, const Basis &p_global_basis, const Basis &p_global_basis_inv, const Vector3 &p_global_position);
	void _restart_bursts();
	BurstInstance _create_burst_instance(int p_index) const;
	void _finish();
	void _mark_core_dirty();
	void _mark_material_dirty();

public:
	YParticles3D();
	~YParticles3D();

	void play(bool p_clear_on_play = true);
	void stop(bool p_clear = false);
	void clear(bool p_stop = false);

	void set_duration(float p_value);
	float get_duration() const;

	void set_start_lifetime_mode(int p_value);
	int get_start_lifetime_mode() const;
	void set_start_lifetime_constant(float p_value);
	float get_start_lifetime_constant() const;
	void set_start_lifetime_random(Vector2 p_value);
	Vector2 get_start_lifetime_random() const;
	void set_start_lifetime_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_lifetime_curve() const;

	void set_start_speed_mode(int p_value);
	int get_start_speed_mode() const;
	void set_start_speed_constant(float p_value);
	float get_start_speed_constant() const;
	void set_start_speed_random(Vector2 p_value);
	Vector2 get_start_speed_random() const;

	void set_gravity(Vector3 p_value);
	Vector3 get_gravity() const;

	void set_start_size_mode(int p_value);
	int get_start_size_mode() const;
	void set_start_size_constant(Vector2 p_value);
	Vector2 get_start_size_constant() const;
	void set_start_size_random(Vector4 p_value);
	Vector4 get_start_size_random() const;
	void set_start_size_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_curve() const;
	void set_start_size_curve_min(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_curve_min() const;
	void set_start_size_curve_max(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_curve_max() const;
	void set_start_size_square_random(Vector2 p_value);
	Vector2 get_start_size_square_random() const;
	void set_start_size_constant_3d(Vector3 p_value);
	Vector3 get_start_size_constant_3d() const;
	void set_start_size_random_min_3d(Vector3 p_value);
	Vector3 get_start_size_random_min_3d() const;
	void set_start_size_random_max_3d(Vector3 p_value);
	Vector3 get_start_size_random_max_3d() const;
	void set_start_size_x_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_x_curve() const;
	void set_start_size_y_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_y_curve() const;
	void set_start_size_z_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_z_curve() const;
	void set_start_size_x_curve_min(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_x_curve_min() const;
	void set_start_size_y_curve_min(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_y_curve_min() const;
	void set_start_size_z_curve_min(const Ref<Curve> &p_value);
	Ref<Curve> get_start_size_z_curve_min() const;

	void set_start_rotation_degrees_mode(int p_value);
	int get_start_rotation_degrees_mode() const;
	void set_start_rotation_degrees_constant(float p_value);
	float get_start_rotation_degrees_constant() const;
	void set_start_rotation_degrees_random(Vector2 p_value);
	Vector2 get_start_rotation_degrees_random() const;
	void set_start_rotation_degrees_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_rotation_degrees_curve() const;
	void set_start_rotation_degrees_constant_3d(Vector3 p_value);
	Vector3 get_start_rotation_degrees_constant_3d() const;
	void set_start_rotation_degrees_random_min_3d(Vector3 p_value);
	Vector3 get_start_rotation_degrees_random_min_3d() const;
	void set_start_rotation_degrees_random_max_3d(Vector3 p_value);
	Vector3 get_start_rotation_degrees_random_max_3d() const;
	void set_start_rotation_degrees_x_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_rotation_degrees_x_curve() const;
	void set_start_rotation_degrees_y_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_rotation_degrees_y_curve() const;
	void set_start_rotation_degrees_z_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_start_rotation_degrees_z_curve() const;

	void set_use_world_space(bool p_value);
	bool is_using_world_space() const;

	void set_play_on_start(bool p_value);
	bool get_play_on_start() const;
	void set_loop(bool p_value);
	bool get_loop() const;
	void set_play_in_reverse(bool p_value);
	bool get_play_in_reverse() const;
	void set_start_delay(float p_value);
	float get_start_delay() const;
	void set_start_delay_percentage(float p_value);
	float get_start_delay_percentage() const;
	void set_destroy_on_finish(bool p_value);
	bool get_destroy_on_finish() const;
	void set_debugging(bool p_value);
	bool get_debugging() const;

	void set_emitting(bool p_value);
	bool is_emitting() const;
	void set_max_particles(int p_value);
	int get_max_particles() const;
	void set_max_emissions_per_frame(int p_value);
	int get_max_emissions_per_frame() const;
	void set_rate_over_time_mode(int p_value);
	int get_rate_over_time_mode() const;
	void set_rate_over_time(float p_value);
	float get_rate_over_time() const;
	void set_rate_over_time_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_rate_over_time_curve() const;
	void set_rate_over_distance(float p_value);
	float get_rate_over_distance() const;
	void set_bursts(const Array &p_value);
	Array get_bursts() const;

	void set_enable_shape(bool p_value);
	bool get_enable_shape() const;
	void set_shape_type(EmissionShape p_value);
	EmissionShape get_shape_type() const;
	void set_radius(float p_value);
	float get_radius() const;
	void set_radius_thickness(float p_value);
	float get_radius_thickness() const;
	void set_angle(float p_value);
	float get_angle() const;
	void set_box_extents(Vector3 p_value);
	Vector3 get_box_extents() const;
	void set_emission_mesh(const Ref<Mesh> &p_value);
	Ref<Mesh> get_emission_mesh() const;
	void set_emission_mesh_scale(Vector3 p_value);
	Vector3 get_emission_mesh_scale() const;
	void set_random_direction(float p_value);
	float get_random_direction() const;
	void set_spherize_direction(float p_value);
	float get_spherize_direction() const;
	void set_emit_from(EmitFrom p_value);
	EmitFrom get_emit_from() const;
	void set_shape_length(float p_value);
	float get_shape_length() const;
	void set_arc_degrees(float p_value);
	float get_arc_degrees() const;
	void set_arc_mode(ArcMode p_value);
	ArcMode get_arc_mode() const;
	void set_arc_spread(float p_value);
	float get_arc_spread() const;
	void set_arc_speed_mode(int p_value);
	int get_arc_speed_mode() const;
	void set_arc_speed_constant(float p_value);
	float get_arc_speed_constant() const;
	void set_arc_speed_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_arc_speed_curve() const;
	void set_direction_in_world_space(bool p_value);
	bool is_direction_in_world_space() const;
	void set_invert_direction(bool p_value);
	bool is_direction_inverted() const;
	void set_position_offset(Vector3 p_value);
	Vector3 get_position_offset() const;
	void set_rotation_offset(Vector3 p_value);
	Vector3 get_rotation_offset() const;

	void set_enable_size_over_lifetime(bool p_value);
	bool get_enable_size_over_lifetime() const;
	void set_size_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_size_over_lifetime() const;
	void set_size_over_lifetime_min(const Ref<Curve> &p_value);
	Ref<Curve> get_size_over_lifetime_min() const;
	void set_width_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_width_over_lifetime() const;
	void set_height_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_height_over_lifetime() const;
	void set_depth_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_depth_over_lifetime() const;
	void set_size_over_lifetime_use_two_curves(bool p_value);
	bool get_size_over_lifetime_use_two_curves() const;

	void set_enable_velocity_over_lifetime(bool p_value);
	bool get_enable_velocity_over_lifetime() const;
	void set_velocity_over_lifetime_mode(int p_value);
	int get_velocity_over_lifetime_mode() const;
	void set_velocity_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime() const;
	void set_velocity_over_lifetime_min(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_min() const;
	void set_velocity_over_lifetime_x(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_x() const;
	void set_velocity_over_lifetime_x_min(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_x_min() const;
	void set_velocity_over_lifetime_y(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_y() const;
	void set_velocity_over_lifetime_y_min(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_y_min() const;
	void set_velocity_over_lifetime_z(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_z() const;
	void set_velocity_over_lifetime_z_min(const Ref<Curve> &p_value);
	Ref<Curve> get_velocity_over_lifetime_z_min() const;
	void set_offset_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_offset_over_lifetime() const;
	void set_velocity_over_lifetime_use_two_curves(bool p_value);
	bool get_velocity_over_lifetime_use_two_curves() const;
	void set_velocity_in_world_space(bool p_value);
	bool get_velocity_in_world_space() const;

	void set_enable_force_over_lifetime(bool p_value);
	bool get_enable_force_over_lifetime() const;
	void set_force_over_lifetime_mode(int p_value);
	int get_force_over_lifetime_mode() const;
	void set_force_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_force_over_lifetime() const;
	void set_force_over_lifetime_x(const Ref<Curve> &p_value);
	Ref<Curve> get_force_over_lifetime_x() const;
	void set_force_over_lifetime_y(const Ref<Curve> &p_value);
	Ref<Curve> get_force_over_lifetime_y() const;
	void set_force_over_lifetime_z(const Ref<Curve> &p_value);
	Ref<Curve> get_force_over_lifetime_z() const;
	void set_force_over_lifetime_constant(Vector3 p_value);
	Vector3 get_force_over_lifetime_constant() const;
	void set_force_over_lifetime_random_min(Vector3 p_value);
	Vector3 get_force_over_lifetime_random_min() const;
	void set_force_over_lifetime_random_max(Vector3 p_value);
	Vector3 get_force_over_lifetime_random_max() const;
	void set_force_in_world_space(bool p_value);
	bool get_force_in_world_space() const;

	void set_enable_limit_velocity_over_lifetime(bool p_value);
	bool get_enable_limit_velocity_over_lifetime() const;
	void set_limit_velocity_over_lifetime_separate_axis(bool p_value);
	bool get_limit_velocity_over_lifetime_separate_axis() const;
	void set_limit_velocity_over_lifetime_speed_mode(int p_value);
	int get_limit_velocity_over_lifetime_speed_mode() const;
	void set_limit_velocity_over_lifetime_speed(float p_value);
	float get_limit_velocity_over_lifetime_speed() const;
	void set_limit_velocity_over_lifetime_speed_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_limit_velocity_over_lifetime_speed_curve() const;
	void set_limit_velocity_over_lifetime_speed_axis(Vector3 p_value);
	Vector3 get_limit_velocity_over_lifetime_speed_axis() const;
	void set_limit_velocity_over_lifetime_speed_x_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_limit_velocity_over_lifetime_speed_x_curve() const;
	void set_limit_velocity_over_lifetime_speed_y_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_limit_velocity_over_lifetime_speed_y_curve() const;
	void set_limit_velocity_over_lifetime_speed_z_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_limit_velocity_over_lifetime_speed_z_curve() const;
	void set_limit_velocity_over_lifetime_dampen(float p_value);
	float get_limit_velocity_over_lifetime_dampen() const;

	void set_enable_noise(bool p_value);
	bool get_enable_noise() const;
	void set_noise_strength(float p_value);
	float get_noise_strength() const;
	void set_noise_strength_mode(int p_value);
	int get_noise_strength_mode() const;
	void set_noise_strength_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_noise_strength_curve() const;
	void set_noise_strength_x(const Ref<Curve> &p_value);
	Ref<Curve> get_noise_strength_x() const;
	void set_noise_strength_y(const Ref<Curve> &p_value);
	Ref<Curve> get_noise_strength_y() const;
	void set_noise_strength_z(const Ref<Curve> &p_value);
	Ref<Curve> get_noise_strength_z() const;
	void set_noise_scale(float p_value);
	float get_noise_scale() const;
	void set_noise_scroll_speed(Vector3 p_value);
	Vector3 get_noise_scroll_speed() const;
	void set_noise_position_amount(float p_value);
	float get_noise_position_amount() const;
	void set_noise_rotation_amount(float p_value);
	float get_noise_rotation_amount() const;
	void set_noise_size_amount(float p_value);
	float get_noise_size_amount() const;
	void set_noise_octaves(int p_value);
	int get_noise_octaves() const;
	void set_noise_lacunarity(float p_value);
	float get_noise_lacunarity() const;

	void set_enable_attractor(bool p_value);
	bool get_enable_attractor() const;
	void set_attraction_target_mode(int p_value);
	int get_attraction_target_mode() const;
	void set_attractor_position(Vector3 p_value);
	Vector3 get_attractor_position() const;
	void set_attraction_target(const NodePath &p_value);
	NodePath get_attraction_target() const;
	void set_attraction_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_attraction_over_lifetime() const;

	void set_enable_collision(bool p_value);
	bool get_enable_collision() const;
	void set_collision_layer(uint32_t p_value);
	uint32_t get_collision_layer() const;
	void set_collision_radius_scale(float p_value);
	float get_collision_radius_scale() const;
	void set_collision_dampen(float p_value);
	float get_collision_dampen() const;
	void set_collision_bounce(float p_value);
	float get_collision_bounce() const;
	void set_collision_lifetime_loss(float p_value);
	float get_collision_lifetime_loss() const;
	void set_collision_min_kill_speed(float p_value);
	float get_collision_min_kill_speed() const;
	void set_collision_quality(int p_value);
	int get_collision_quality() const;
	void set_collision_voxel_size(float p_value);
	float get_collision_voxel_size() const;

	void set_enable_sub_emitters(bool p_value);
	bool get_enable_sub_emitters() const;
	void set_sub_emitters(const Array &p_value);
	Array get_sub_emitters() const;

	void set_enable_rotation_over_lifetime(bool p_value);
	bool get_enable_rotation_over_lifetime() const;
	void set_rotation_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_rotation_over_lifetime() const;
	void set_rotation_over_lifetime_axis(Vector3 p_value);
	Vector3 get_rotation_over_lifetime_axis() const;
	void set_enable_rotation_by_speed(bool p_value);
	bool get_enable_rotation_by_speed() const;
	void set_rotation_by_speed_mode(int p_value);
	int get_rotation_by_speed_mode() const;
	void set_rotation_by_speed(const Ref<Curve> &p_value);
	Ref<Curve> get_rotation_by_speed() const;
	void set_rotation_by_speed_x(const Ref<Curve> &p_value);
	Ref<Curve> get_rotation_by_speed_x() const;
	void set_rotation_by_speed_y(const Ref<Curve> &p_value);
	Ref<Curve> get_rotation_by_speed_y() const;
	void set_rotation_by_speed_z(const Ref<Curve> &p_value);
	Ref<Curve> get_rotation_by_speed_z() const;
	void set_rotation_by_speed_range(Vector2 p_value);
	Vector2 get_rotation_by_speed_range() const;

	void set_enable_inherit_velocity(bool p_value);
	bool get_enable_inherit_velocity() const;
	void set_inherit_velocity_mode(int p_value);
	int get_inherit_velocity_mode() const;
	void set_inherit_velocity_multiplier(float p_value);
	float get_inherit_velocity_multiplier() const;
	void set_inherit_velocity_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_inherit_velocity_curve() const;
	void set_orbit_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_orbit_over_lifetime() const;
	void set_orbit_around_axis(Vector3 p_value);
	Vector3 get_orbit_around_axis() const;

	void set_enable_color_over_lifetime(bool p_value);
	bool get_enable_color_over_lifetime() const;
	void set_color_over_lifetime(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_color_over_lifetime() const;
	void set_color_over_lifetime_secondary(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_color_over_lifetime_secondary() const;
	void set_alpha_over_lifetime(const Ref<Curve> &p_value);
	Ref<Curve> get_alpha_over_lifetime() const;
	void set_alpha_over_lifetime_secondary(const Ref<Curve> &p_value);
	Ref<Curve> get_alpha_over_lifetime_secondary() const;
	void set_color_over_lifetime_use_two_gradients(bool p_value);
	bool get_color_over_lifetime_use_two_gradients() const;
	void set_use_start_color_gradient(bool p_value);
	bool get_use_start_color_gradient() const;
	void set_start_color_gradient(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_start_color_gradient() const;
	void set_start_color_gradient_secondary(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_start_color_gradient_secondary() const;
	void set_start_color_use_two_gradients(bool p_value);
	bool get_start_color_use_two_gradients() const;
	void set_starting_hue(float p_value);
	float get_starting_hue() const;
	void set_hue_variation(float p_value);
	float get_hue_variation() const;

	void set_texture_sheet_enabled(bool p_value);
	bool get_texture_sheet_enabled() const;
	void set_h_frames(int p_value);
	int get_h_frames() const;
	void set_v_frames(int p_value);
	int get_v_frames() const;
	void set_tiles_mode(TextureSheetTiles p_value);
	TextureSheetTiles get_tiles_mode() const;
	void set_use_random_starting_tile(bool p_value);
	bool get_use_random_starting_tile() const;
	void set_start_index_tile(int p_value);
	int get_start_index_tile() const;
	void set_animation_cycles(float p_value);
	float get_animation_cycles() const;
	void set_frame_over_time(const Ref<Curve> &p_value);
	Ref<Curve> get_frame_over_time() const;

	void set_particle_texture(const Ref<Texture2D> &p_value);
	Ref<Texture2D> get_particle_texture() const;
	void set_enable_trails(bool p_value);
	bool get_enable_trails() const;
	void set_trail_ratio(float p_value);
	float get_trail_ratio() const;
	void set_trail_lifetime_mode(int p_value);
	int get_trail_lifetime_mode() const;
	void set_trail_lifetime(float p_value);
	float get_trail_lifetime() const;
	void set_trail_lifetime_curve(const Ref<Curve> &p_value);
	Ref<Curve> get_trail_lifetime_curve() const;
	void set_trail_min_vertex_distance(float p_value);
	float get_trail_min_vertex_distance() const;
	void set_trail_world_space(bool p_value);
	bool get_trail_world_space() const;
	void set_trail_die_with_particles(bool p_value);
	bool get_trail_die_with_particles() const;
	void set_trail_size_affects_width(bool p_value);
	bool get_trail_size_affects_width() const;
	void set_trail_size_affects_lifetime(bool p_value);
	bool get_trail_size_affects_lifetime() const;
	void set_trail_inherit_particle_color(bool p_value);
	bool get_trail_inherit_particle_color() const;
	void set_trail_texture_mode(TrailTextureMode p_value);
	TrailTextureMode get_trail_texture_mode() const;
	void set_trail_color_over_lifetime(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_trail_color_over_lifetime() const;
	void set_trail_color_over_trail(const Ref<GradientTexture1D> &p_value);
	Ref<GradientTexture1D> get_trail_color_over_trail() const;
	void set_trail_width_over_trail(const Ref<Curve> &p_value);
	Ref<Curve> get_trail_width_over_trail() const;
	void set_trail_texture(const Ref<Texture2D> &p_value);
	Ref<Texture2D> get_trail_texture() const;
	void set_tint_color(Color p_value);
	Color get_tint_color() const;
	void set_billboard_mode(BillboardMode p_value);
	BillboardMode get_billboard_mode() const;
	void set_render_alignment(RenderAlignment p_value);
	RenderAlignment get_render_alignment() const;
	void set_velocity_stretch(float p_value);
	float get_velocity_stretch() const;
	void set_length_stretch(float p_value);
	float get_length_stretch() const;
	void set_align_to_velocity(bool p_value);
	bool get_align_to_velocity() const;
	void set_align_offset_degrees(float p_value);
	float get_align_offset_degrees() const;
	void set_blend_mode(BlendMode p_value);
	BlendMode get_blend_mode() const;
	void set_render_priority(int p_value);
	int get_render_priority() const;
	void set_sampling_filter(SamplingFilter p_value);
	SamplingFilter get_sampling_filter() const;
	void set_rendering_layer(uint32_t p_value);
	uint32_t get_rendering_layer() const;
	void set_visibility_aabb(const AABB &p_value);
	AABB get_visibility_aabb() const;
	void set_override_material(const Ref<Material> &p_value);
	Ref<Material> get_override_material() const;
	void set_custom_mesh(const Ref<Mesh> &p_value);
	Ref<Mesh> get_custom_mesh() const;

	void set_playback_speed(float p_value);
	float get_playback_speed() const;
	void set_paused(bool p_value);
	bool is_paused() const;
	void set_fixed_fps(int p_value);
	int get_fixed_fps() const;
	bool is_playing() const;
	int get_visible_particle_count() const;
	float get_simulation_time() const;
};

VARIANT_ENUM_CAST(YParticles3D::BlendMode);
VARIANT_ENUM_CAST(YParticles3D::SamplingFilter);
VARIANT_ENUM_CAST(YParticles3D::BillboardMode);
VARIANT_ENUM_CAST(YParticles3D::RenderAlignment);
VARIANT_ENUM_CAST(YParticles3D::TrailTextureMode);
VARIANT_ENUM_CAST(YParticles3D::TextureSheetTiles);
VARIANT_ENUM_CAST(YParticles3D::EmissionShape);
VARIANT_ENUM_CAST(YParticles3D::EmitFrom);
VARIANT_ENUM_CAST(YParticles3D::ArcMode);
VARIANT_ENUM_CAST(YParticles3D::AttractionTargetMode);

#endif // YPARTICLES3D_H
