#version 450
// The board's picture, a gadget canvas or a manual page. Test 8: the host menu's look from the push constants:
// look.a = (brightness, scanline depth, soft 0..1, the board's lines across the texture's v), look.b.xy = one texel.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look {
	vec4 a;
	vec4 b;
} look;

void main()
{
	vec4 c = texture(u_texture, v_uv);
	vec4 s = texture(u_texture, v_uv + look.b.xy) + texture(u_texture, v_uv - look.b.xy) +
		texture(u_texture, v_uv + vec2(look.b.x, -look.b.y)) + texture(u_texture, v_uv + vec2(-look.b.x, look.b.y));
	c = mix(c, (c + s * 0.25) * 0.5, look.a.z);
	float d = abs(fract(v_uv.y * look.a.w) - 0.5) * 2.0;
	o_color = vec4(c.rgb * (look.a.x * (1.0 - look.a.y * d * d)), c.a);
}
