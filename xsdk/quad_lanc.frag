#version 450
// test 24: the game picture, FILTER LANCZOS (Lanczos-2, 4x4 taps on the board's grid, the weights worked out once an axis). See quad.frag for the push constants and why each filter has a shader of its
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
	vec2 p = v_uv * look.b.zw - 0.5;
	vec2 i = floor(p);
	vec2 f = p - i;
	// L(x) = 2 sin(pi x) sin(pi x / 2) / (pi x)^2 at x = -1-f, -f, 1-f, 2-f: sin(pi x) is +-sin(pi f) and sin(pi x / 2)
	// +-sin or +-cos of pi f / 2, so an axis takes three sines (test 23 worked out 64 a pixel)
	const float PI = 3.14159265358979323846;
	const float N = 2.0 / (PI * PI);
	vec2 s = sin(PI * f);
	vec2 sh = sin(PI * 0.5 * f);
	vec2 ch = cos(PI * 0.5 * f);
	vec2 w0 = -N * s * ch / ((1.0 + f) * (1.0 + f));
	vec2 w1 = mix(N * s * sh / max(f * f, vec2(1e-6)), vec2(1.0), step(f, vec2(0.001)));
	vec2 w2 = mix(N * s * ch / max((1.0 - f) * (1.0 - f), vec2(1e-6)), vec2(1.0), step(vec2(0.999), f));
	vec2 w3 = -N * s * sh / ((2.0 - f) * (2.0 - f));
	vec4 wx = vec4(w0.x, w1.x, w2.x, w3.x);
	vec4 wy = vec4(w0.y, w1.y, w2.y, w3.y);
	vec4 acc = (TAP(-0.5, -0.5) * wx.x + TAP(0.5, -0.5) * wx.y + TAP(1.5, -0.5) * wx.z + TAP(2.5, -0.5) * wx.w) * wy.x
		+ (TAP(-0.5, 0.5) * wx.x + TAP(0.5, 0.5) * wx.y + TAP(1.5, 0.5) * wx.z + TAP(2.5, 0.5) * wx.w) * wy.y
		+ (TAP(-0.5, 1.5) * wx.x + TAP(0.5, 1.5) * wx.y + TAP(1.5, 1.5) * wx.z + TAP(2.5, 1.5) * wx.w) * wy.z
		+ (TAP(-0.5, 2.5) * wx.x + TAP(0.5, 2.5) * wx.y + TAP(1.5, 2.5) * wx.z + TAP(2.5, 2.5) * wx.w) * wy.w;
	vec4 c = clamp(acc / (dot(wx, vec4(1.0)) * dot(wy, vec4(1.0))), 0.0, 1.0);
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
	// test 25 (user): a menu texel has alpha 0 and is drawn plain (pl.a = 0: no filter, colour, outline, scanlines, brightness)
	vec4 pl = texture(u_texture, v_uv);   // (not p: p is the board coordinate here)
	float d = abs(fract(v_uv.y * look.a.w) - 0.5) * 2.0;
	o_color = vec4(mix(pl.rgb, lit * (look.a.x * (1.0 - look.a.y * d * d)), pl.a), 1.0);
}
