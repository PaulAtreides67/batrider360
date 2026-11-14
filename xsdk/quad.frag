#version 450
// The gadget canvas and the manual's pages: the texture times the brightness.
// test 24 (lag fix): xsdk's compiler runs both arms of every data branch and unrolls loops, so test 23's one shader
// with all five FILTERs and six SCREEN COLORs made every pixel of every quad pay for all of them (~1100 lines of
// microcode: the GPU drew 5-15 frames a second and the Guide lagged with it). Now one short shader a filter
// (quad_sharp/near/bilin/cubic/lanc.frag), none with branches or loops; this one only for the canvas and the pages.
// Push constants (main.c set_look, the same block in every .frag): a = (brightness, scan depth, outline 0/1, the
// board's lines), b = (1/BR_W, 1/BR_H, BR_W, BR_H), r/g/k = SCREEN COLOR's matrix rows (xyz) and offsets (w).
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

void main()
{
	vec4 c = texture(u_texture, v_uv);
	o_color = vec4(c.rgb * look.a.x, c.a);
}
