shader_type canvas_item;

void fragment() {
	vec4 sample_color = texture(TEXTURE, UV);
	float green_dominance = sample_color.g - max(sample_color.r, sample_color.b);
	float magenta_dominance = min(sample_color.r, sample_color.b) - sample_color.g;
	float key = max(smoothstep(0.58, 0.82, green_dominance), smoothstep(0.58, 0.82, magenta_dominance));
	sample_color.a *= 1.0 - key;
	COLOR = sample_color;
}
