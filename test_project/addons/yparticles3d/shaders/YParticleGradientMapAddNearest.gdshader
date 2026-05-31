shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, diffuse_burley, specular_disabled, shadows_disabled;
#define POINT_FILTERING
#include "yparticle_include.gdshaderinc"

varying vec2 rotated_uv;


void vertex() {
	vec2 get_rotated_uv = UV;
	MODELVIEW_MATRIX = vertex_common(MODEL_MATRIX, INV_VIEW_MATRIX, VIEW_MATRIX, COLOR, INSTANCE_CUSTOM, UV, vec2(float(particles_anim_h_frames), float(particles_anim_v_frames)), get_rotated_uv);
	MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
	rotated_uv = get_rotated_uv;
}

void fragment() {
	vec4 input_color = texture(albedo_texture, rotated_uv);
	float value = (input_color.r + input_color.g + input_color.b) / 3.0;
	vec4 output_color = texture(gradient_texture, vec2(COLOR.g, 0.1)) * vec4(input_color.rgb,1.0);
	if (use_secondary_gradient == 1) {
		vec4 secondary_color = texture(secondary_gradient_texture, vec2(COLOR.g, 0.1)) * vec4(input_color.rgb,1.0);
		output_color = mix(output_color, secondary_color, clamp(COLOR.r, 0.0, 1.0));
	}
	if (use_start_color_gradient == 1) {
		vec4 start_color = texture(start_color_gradient_texture, vec2(clamp(COLOR.r,0.001,0.998), 0.5));
		output_color.rgb *= start_color.rgb;
	}

	float hue_shift = use_color_random_factor == 1 ? 0.0 : COLOR.r;
	ALBEDO = hue_change(output_color.rgb * tint_color.rgb, hue_shift);
	ALPHA = min(min(min(input_color.a * output_color.a, value),output_color.a),tint_color.a);
}
