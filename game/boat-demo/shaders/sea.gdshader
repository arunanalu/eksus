shader_type canvas_item;

uniform vec4 deep_color : source_color = vec4(0.015, 0.12, 0.25, 1.0);
uniform vec4 horizon_color : source_color = vec4(0.03, 0.38, 0.60, 1.0);
uniform vec4 sun_color : source_color = vec4(1.0, 0.79, 0.38, 1.0);
uniform float wave_scale = 8.0;
uniform float far_speed = 0.28;
uniform float near_speed = 0.66;
uniform vec2 wind_dir = vec2(0.0, -1.0);

float band(float value, float edge, float softness) {
	return smoothstep(edge - softness, edge + softness, value);
}

void fragment() {
	vec2 uv = UV;
	float t = TIME;
	vec2 direction = normalize(wind_dir);
	vec2 sideways = vec2(-direction.y, direction.x);
	float along_wind = dot(uv - vec2(0.5), direction);
	float across_wind = dot(uv - vec2(0.5), sideways);
	float far_wave = sin((along_wind * wave_scale + t * far_speed) * 6.283) * 0.0045;
	float cross_wave = sin((along_wind * 2.1 - across_wind * 3.4 + t * 0.18) * 6.283) * 0.0035;
	float near_wave = sin((along_wind * 5.5 + across_wind * 1.8 - t * near_speed) * 6.283) * 0.0025;
	float water = far_wave + cross_wave + near_wave;
	float depth = clamp(uv.y + water * 3.0, 0.0, 1.0);
	vec3 color = mix(horizon_color.rgb, deep_color.rgb, depth);
	float ripples = 0.5 + 0.5 * sin(across_wind * 5.0 + along_wind * 1.2 - t * (0.12 + near_speed * 0.16));
	color += vec3(0.10, 0.27, 0.34) * ripples * (0.035 + 0.06 * uv.y);
	float wind_wisps = 0.5 + 0.5 * sin(along_wind * 13.0 - t * (0.32 + far_speed) + sin(across_wind * 4.0));
	color = mix(color, vec3(0.68, 0.88, 0.94), wind_wisps * 0.035 * smoothstep(0.2, 0.8, uv.y));
	float glint = pow(max(0.0, sin(along_wind * 9.0 - across_wind * 2.5 - t * 0.28)), 28.0);
	color = mix(color, sun_color.rgb, glint * 0.06 * uv.y);
	float vignette = smoothstep(1.15, 0.18, length(uv - vec2(0.5, 0.48)));
	COLOR = vec4(color * (0.78 + vignette * 0.22), 1.0);
}
