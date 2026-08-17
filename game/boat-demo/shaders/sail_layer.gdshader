shader_type canvas_item;

uniform float sail_amount = 1.0;

void fragment() {
	vec4 sample_color = texture(TEXTURE, UV);
	float cream = smoothstep(0.62, 0.84, sample_color.r) * smoothstep(0.50, 0.77, sample_color.g) * (1.0 - smoothstep(0.48, 0.74, sample_color.b));
	// A vela permanece presa ao topo do mastro e perde área de baixo para cima.
	float lower_edge = mix(0.10, 0.70, sail_amount);
	float reef = 1.0 - smoothstep(lower_edge, lower_edge + 0.012, UV.y);
	sample_color.a *= cream * reef;
	COLOR = sample_color;
}
