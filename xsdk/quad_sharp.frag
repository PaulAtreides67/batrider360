#version 450
// test 24: the game picture, FILTER SHARP (the linear sampler on the doubled texture). See quad.frag for the push constants and why each filter has a shader of its
// own. All in main, each local assigned once: xsdk's compiler takes no function calls or reassigned locals.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look {
	vec4 a;
	vec4 b;
	vec4 r;
	vec4 g;
	vec4 k;
} look;

#define LW vec3(0.299, 0.587, 0.114)
// a board texel's centre, i + (dx, dy) on the board's grid
#define TAP(dx, dy) texture(u_texture, (i + vec2(dx, dy)) * look.b.xy)

void main()
{
	vec4 c = texture(u_texture, v_uv);
	// SCREEN COLOR (a matrix; OUTLINE: white edge lines on black (test 27b)), then scanlines and brightness
	vec3 rgb = vec3(dot(look.r.xyz, c.rgb) + look.r.w, dot(look.g.xyz, c.rgb) + look.g.w, dot(look.k.xyz, c.rgb) + look.k.w);
	// test 27b (user): OUTLINE = white edge lines on black, as ibara360's (3x3 Sobel on the luminance, one screen pixel apart (1/3 texel), on the nearest picture (test 27b rev 2: thin double lines, the user's reference)
	// look.a.z = 1 for OUTLINE replaces the colour, 0 keeps it)
	float s_tl = dot(texture(u_texture, (floor((v_uv + vec2(-look.b.x, -look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_t = dot(texture(u_texture, (floor((v_uv + vec2(0.0, -look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_tr = dot(texture(u_texture, (floor((v_uv + vec2(look.b.x, -look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_l = dot(texture(u_texture, (floor((v_uv + vec2(-look.b.x, 0.0) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_r = dot(texture(u_texture, (floor((v_uv + vec2(look.b.x, 0.0) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_bl = dot(texture(u_texture, (floor((v_uv + vec2(-look.b.x, look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_b = dot(texture(u_texture, (floor((v_uv + vec2(0.0, look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float s_br = dot(texture(u_texture, (floor((v_uv + vec2(look.b.x, look.b.y) * 0.3333) * look.b.zw) + 0.5) * look.b.xy).rgb, LW);
	float sx = (s_tr + 2.0 * s_r + s_br) - (s_tl + 2.0 * s_l + s_bl);
	float sy = (s_bl + 2.0 * s_b + s_br) - (s_tl + 2.0 * s_t + s_tr);
	float edge = smoothstep(0.10, 0.45, sqrt(sx * sx + sy * sy) * 0.25);
	vec3 lit = clamp(mix(rgb, vec3(edge), look.a.z), 0.0, 1.0);
	// test 25 (user): a menu texel has alpha 0 and is drawn plain (p.a = 0: no filter, colour, outline, scanlines, brightness)
	vec4 p = texture(u_texture, v_uv);
	float d = abs(fract(v_uv.y * look.a.w) - 0.5) * 2.0;
	o_color = vec4(mix(p.rgb, lit * (look.a.x * (1.0 - look.a.y * d * d)), p.a), 1.0);
}
