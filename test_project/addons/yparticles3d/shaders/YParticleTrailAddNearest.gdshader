shader_type spatial;

render_mode blend_add, unshaded, depth_draw_never, skip_vertex_transform, cull_disabled;

uniform sampler2D albedo_texture : source_color, repeat_enable, filter_nearest;

void vertex() {
	vec3 p = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 t = (MODELVIEW_MATRIX * vec4(NORMAL, 0.0)).xyz;
	VERTEX = p + UV.y * normalize(cross(p, t));
	NORMAL = (VIEW_MATRIX * vec4(0, 1, 0, 0)).xyz;
	UV.y = (sign(UV.y) + 1.0) / 2.0;
}

void fragment() {
	vec4 tex = texture(albedo_texture, UV);
	ALBEDO = COLOR.rgb * tex.rgb;
	ALPHA = COLOR.a * tex.a;
}
