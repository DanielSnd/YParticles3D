#ifdef POINT_FILTERING
uniform sampler2D albedo_texture : source_color, repeat_disable, filter_nearest;
#else
uniform sampler2D albedo_texture : source_color, repeat_disable, filter_linear_mipmap;
#endif

uniform sampler2D gradient_texture : source_color, hint_default_white, repeat_disable, filter_linear;
uniform sampler2D secondary_gradient_texture : source_color, hint_default_white, repeat_disable, filter_linear;
uniform sampler2D start_color_gradient_texture : source_color, hint_default_white, repeat_disable, filter_linear;

instance uniform int billboard_mode = 0;
instance uniform int render_alignment = 0;
instance uniform int align_to_velocity = 0;
instance uniform float align_offset_radians = 0.0;
instance uniform int use_start_color_gradient = 0;
instance uniform int use_secondary_gradient = 0;
instance uniform int use_color_random_factor = 0;
instance uniform int particles_anim_h_frames : hint_range(1, 128) = 1;
instance uniform int particles_anim_v_frames : hint_range(1, 128) = 1;
instance uniform vec4 tint_color:source_color = vec4(1.0);

vec3 hue_change(vec3 input_color, float shift_hue) {
	vec3 color_hsv;
	{
		vec3 c = input_color;
		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
		float d = q.x - min(q.w, q.y);
		float e = 1.0e-10;
		color_hsv=vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
	}
	color_hsv.x = mod((color_hsv.x + shift_hue), 1.0f);
	vec3 color_rgb;
	{
		vec3 c = color_hsv;
		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
		color_rgb=c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
	}
	return color_rgb.rgb;
}

bool build_velocity_aligned_basis(vec3 velocity_dir, vec3 face_dir, vec3 fallback_right, out vec3 aligned_right, out vec3 aligned_up) {
	vec3 projected_velocity = velocity_dir - face_dir * dot(velocity_dir, face_dir);
	float projected_length = length(projected_velocity);
	if (projected_length <= 0.0001) {
		aligned_right = fallback_right;
		aligned_up = normalize(cross(face_dir, aligned_right));
		return false;
	}

	aligned_up = projected_velocity / projected_length;
	aligned_right = normalize(cross(aligned_up, face_dir));
	aligned_up = normalize(cross(face_dir, aligned_right));
	return true;
}

void build_facing_basis(vec3 face_dir, vec3 up_reference, vec3 fallback_right, out vec3 aligned_right, out vec3 aligned_up) {
	vec3 right_candidate = cross(up_reference, face_dir);
	if (length(right_candidate) <= 0.0001) {
		right_candidate = fallback_right;
	}
	aligned_right = normalize(right_candidate);
	aligned_up = normalize(cross(face_dir, aligned_right));
}

mat4 vertex_common(mat4 _MODEL_MATRIX, mat4 _INV_VIEW_MATRIX, mat4 _VIEW_MATRIX, vec4 _COLOR, vec4 instance_custom, vec2 _UV, vec2 particles_anim, out vec2 final_uv) {
	// Get the direction of motion from the model matrix's forward vector (Z axis)
	vec3 velocity_dir = normalize(_MODEL_MATRIX[2].xyz);
	
	// Check if instance_custom is all zeros
	bool use_default_scale = (dot(instance_custom, instance_custom) < 0.0001);
	
	// Get custom scales and parameters, fallback to defaults if all zeros
	float scale_x = use_default_scale ? 1.0 : instance_custom.y;
	float scale_y = use_default_scale ? 1.0 : instance_custom.z;
	float angle = use_default_scale ? align_offset_radians : instance_custom.x + align_offset_radians;
	float stretch = use_default_scale ? 1.0 : instance_custom.w;
	
	mat4 return_modelview_matrix;
	// Get view direction from the particle toward the camera.
	vec3 view_dir = normalize(_INV_VIEW_MATRIX[3].xyz - _MODEL_MATRIX[3].xyz);

	// Calculate how parallel the view is to the velocity
	float parallel_factor = abs(dot(velocity_dir, view_dir));

	// Setup billboard orientation vectors
	vec3 billboard_right = normalize(_INV_VIEW_MATRIX[0].xyz);
	vec3 billboard_up = normalize(_INV_VIEW_MATRIX[1].xyz);
	vec3 alignment_face_dir = view_dir;
	vec3 world_up = vec3(0.0, 1.0, 0.0);

	if (render_alignment == 1) {
		alignment_face_dir = vec3(0.0, 0.0, 1.0);
		billboard_right = vec3(1.0, 0.0, 0.0);
		billboard_up = vec3(0.0, 1.0, 0.0);
	} else if (render_alignment == 2) {
		alignment_face_dir = normalize(_MODEL_MATRIX[2].xyz);
		billboard_right = normalize(_MODEL_MATRIX[0].xyz);
		billboard_up = normalize(_MODEL_MATRIX[1].xyz);
	} else if (render_alignment == 3) {
		build_facing_basis(view_dir, world_up, billboard_right, billboard_right, billboard_up);
		alignment_face_dir = view_dir;
	} else if (render_alignment == 4 && length(velocity_dir) > 0.0001) {
		alignment_face_dir = velocity_dir;
		build_facing_basis(alignment_face_dir, normalize(_INV_VIEW_MATRIX[1].xyz), billboard_right, billboard_right, billboard_up);
	}

	if (billboard_mode == 2) {
		// NO BILLBOARD WANTED
		// Instead of rotating UVs, apply rotation to the model matrix
		float cos_a = cos(angle);
		float sin_a = sin(angle);

		// Create rotation matrix around Z axis
		mat4 rot_mat = mat4(
			vec4(cos_a, -sin_a, 0.0, 0.0),
			vec4(sin_a, cos_a, 0.0, 0.0),
			vec4(0.0, 0.0, 1.0, 0.0),
			vec4(0.0, 0.0, 0.0, 1.0)
		);

		// Use the original model matrix but apply custom scaling and rotation
		mat4 scale_mat = mat4(
			vec4(length(_MODEL_MATRIX[0].xyz) * scale_x, 0.0, 0.0, 0.0),
			vec4(0.0, length(_MODEL_MATRIX[1].xyz) * scale_y, 0.0, 0.0),
			vec4(0.0, 0.0, length(_MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));

		return_modelview_matrix = _VIEW_MATRIX * _MODEL_MATRIX * rot_mat * scale_mat;
	} else if (billboard_mode == 0) { // STANDARD BILLBOARDING
		if (render_alignment == 4) {
			billboard_right *= scale_x;
			billboard_up *= scale_y;
		} else if (align_to_velocity == 1){
			vec3 aligned_right;
			vec3 aligned_up;
			build_velocity_aligned_basis(velocity_dir, alignment_face_dir, billboard_right, aligned_right, aligned_up);
			billboard_right = aligned_right * scale_x;
			billboard_up = aligned_up * scale_y;
		} else {
			billboard_right *= scale_x;
			billboard_up *= scale_y;
		}

		// Apply rotation to the billboard
		float cos_a = cos(angle);
		float sin_a = sin(angle);
		vec3 rotated_right = billboard_right * cos_a - billboard_up * sin_a;
		vec3 rotated_up = billboard_right * sin_a + billboard_up * cos_a;
		billboard_right = rotated_right;
		billboard_up = rotated_up;

		// Simple billboard matrix
		mat4 mat_world = mat4(vec4(billboard_right, 0.0), vec4(billboard_up, 0.0), vec4(alignment_face_dir, 0.0), _MODEL_MATRIX[3]);
		// Preserve original scale
		mat4 scale_mat = mat4(vec4(length(_MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(_MODEL_MATRIX[1].xyz), 0.0, 0.0),
			vec4(0.0, 0.0, length(_MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));
		return_modelview_matrix = _VIEW_MATRIX * mat_world * scale_mat;
	} else if (billboard_mode == 3) {
		// VERTICAL BILLBOARDING - Y-axis constrained
		// Get direction to camera (in world space)
		vec3 to_camera = normalize(_INV_VIEW_MATRIX[3].xyz - _MODEL_MATRIX[3].xyz);
		// Project camera direction onto the XZ plane
		to_camera.y = 0.0;
		to_camera = normalize(to_camera);
		// Construct right vector perpendicular to world up and to_camera
		vec3 right = normalize(cross(world_up, to_camera));
		// Apply custom scales
		right *= scale_x;
		world_up *= scale_y;

		// Apply rotation to the billboard
		float cos_a = cos(angle);
		float sin_a = sin(angle);
		vec3 rotated_right = right * cos_a - world_up * sin_a;
		vec3 rotated_up = right * sin_a + world_up * cos_a;
		right = rotated_right;
		world_up = rotated_up;

		// Construct vertical billboard matrix
		mat4 mat_world = mat4(
			vec4(right, 0.0),
			vec4(world_up, 0.0),
			vec4(to_camera, 0.0),
			_MODEL_MATRIX[3]);
		// Preserve original scale
		mat4 scale_mat = mat4(
			vec4(length(_MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0),
			vec4(0.0, length(_MODEL_MATRIX[1].xyz), 0.0, 0.0),
			vec4(0.0, 0.0, length(_MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));
		return_modelview_matrix = _VIEW_MATRIX * mat_world * scale_mat;
	} else if (billboard_mode == 1) { // STRETCHED BILLBOARDING
		// Project velocity onto view plane
		vec3 projected_vel = velocity_dir - view_dir * dot(velocity_dir, view_dir);
		float proj_length = length(projected_vel);

		// Improved stretch factor calculation
		float view_aligned_factor = smoothstep(0.0, 0.3, proj_length);
		float effective_stretch = mix(stretch, stretch * 0.25, parallel_factor * parallel_factor);
		effective_stretch = mix(effective_stretch, stretch, view_aligned_factor);

		float abs_stretch = (effective_stretch);
		if (align_to_velocity == 1) {
			vec3 stretch_dir = normalize(mix(velocity_dir, projected_vel, view_aligned_factor));
			vec3 aligned_right;
			vec3 aligned_up;
			bool has_projected_velocity = build_velocity_aligned_basis(stretch_dir, alignment_face_dir, billboard_right, aligned_right, aligned_up);
			if (has_projected_velocity) {
				billboard_right = aligned_right * scale_x * (1.0 / sqrt(abs_stretch));
				billboard_up = aligned_up * scale_y * abs_stretch;
			} else {
				billboard_right = billboard_right * scale_x * (1.0 / sqrt(abs_stretch));
				billboard_up = billboard_up * scale_y * abs_stretch;
			}
		} else {
			billboard_right *= scale_x * (1.0 / sqrt(abs_stretch));
			billboard_up *= scale_y * abs_stretch;
		}

		float additional_angle = angle;
		float cos_a = cos(additional_angle);
		float sin_a = sin(additional_angle);
		vec3 rotated_right = billboard_right * cos_a - billboard_up * sin_a;
		vec3 rotated_up = billboard_right * sin_a + billboard_up * cos_a;
		billboard_right = rotated_right;
		billboard_up = rotated_up;

		// Construct final billboard matrix
		mat4 mat_world = mat4(
			vec4(billboard_right, 0.0),
			vec4(billboard_up, 0.0),
			vec4(alignment_face_dir, 0.0),
			_MODEL_MATRIX[3]);

		// Preserve original scale
		mat4 scale_mat = mat4(
			vec4(length(_MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0),
			vec4(0.0, length(_MODEL_MATRIX[1].xyz), 0.0, 0.0),
			vec4(0.0, 0.0, length(_MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));

		return_modelview_matrix = _VIEW_MATRIX * mat_world * scale_mat;
	} else if (billboard_mode == 4) { // STRETCHED BILLBOARDING VERTICAL
		// Project velocity onto view plane
		vec3 projected_vel = velocity_dir - view_dir * dot(velocity_dir, view_dir);
		// Calculate stretch direction
		// Get world up vector (assuming Y is up)
		billboard_up = vec3(0.0, 1.0, 0.0);
		// Get direction to camera (in world space)
		vec3 to_camera = normalize(_INV_VIEW_MATRIX[3].xyz - _MODEL_MATRIX[3].xyz);
		// Project camera direction onto the XZ plane
		to_camera.y = 0.0;
		to_camera = normalize(to_camera);
		// Construct right vector perpendicular to world up and to_camera
		vec3 billboard_right = normalize(cross(billboard_up, to_camera));
		// Apply custom scales to the billboard vectors
		billboard_right *= scale_x;
		billboard_up *= scale_y;
		// Rotate and stretch the vectors
		billboard_up *= stretch;
		billboard_right *= (1.0 / sqrt(stretch));

		// Apply particle rotation
		float additional_angle = angle;
		if (align_to_velocity == 1) {
			vec3 stretch_dir = normalize(mix(velocity_dir, projected_vel, 0.5));
			vec3 aligned_right;
			vec3 aligned_up;
			bool has_projected_velocity = build_velocity_aligned_basis(stretch_dir, alignment_face_dir, billboard_right, aligned_right, aligned_up);
			if (has_projected_velocity) {
				billboard_right = aligned_right * (scale_x / sqrt(stretch));
				billboard_up = aligned_up * (scale_y * stretch);
			}
		}
		float cos_a = cos(additional_angle);
		float sin_a = sin(additional_angle);
		vec3 rotated_right = billboard_right * cos_a - billboard_up * sin_a;
		vec3 rotated_up = billboard_right * sin_a + billboard_up * cos_a;
		billboard_right = rotated_right;
		billboard_up = rotated_up;

		// Construct final billboard matrix
		mat4 mat_world = mat4(vec4(billboard_right, 0.0), vec4(billboard_up, 0.0), vec4(alignment_face_dir, 0.0), _MODEL_MATRIX[3]);

		// Preserve original scale
		mat4 scale_mat = mat4(vec4(length(_MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(_MODEL_MATRIX[1].xyz), 0.0, 0.0),
			vec4(0.0, 0.0, length(_MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));

		return_modelview_matrix = _VIEW_MATRIX * mat_world * scale_mat;
	}

	// Now we use unmodified UVs for animation
	final_uv = _UV;

	// Animation frames handling
	if (_COLOR.a > -0.5 && particles_anim.x > 0.0 && particles_anim.y > 0.0) {
	    float h_frames = particles_anim.x;
	    float v_frames = particles_anim.y;
	    float particle_total_frames = float(particles_anim.x * particles_anim.y);
	    if (_COLOR.a >= 1.0) {
	        // Single row mode
	        // Extract row and frame progress
	        float row = floor(_COLOR.a);
	        float frame_progress = fract(_COLOR.a);
	        // Calculate frame within row
	        float particle_frame = floor(frame_progress * h_frames);
	        // Apply UV transformation
	        final_uv /= vec2(h_frames, v_frames);
	        final_uv += vec2(
	            mod(particle_frame, h_frames) / h_frames,
	            row / v_frames
	        );
	    } else {
	        // Whole sheet mode
	        float frame = _COLOR.a * particle_total_frames;
	        float current_frame = floor(frame);
	        // Apply UV transformation
	        final_uv /= vec2(h_frames, v_frames);
	        final_uv += vec2(
	            mod(current_frame, h_frames) / h_frames,
	            floor(current_frame / h_frames) / v_frames
	        );
	    }
	}
	
	return return_modelview_matrix;
}
