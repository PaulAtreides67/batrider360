#version 450
// A corner of the picture's quad: where it sits in clip space and where it reads the board's picture.
// test 23: UPSIDE DOWN turns the whole frame half round (turn.t.x = -1)
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_uv;
layout(location = 0) out vec2 v_uv;
layout(push_constant) uniform Turn {
	vec4 t;
} turn;

void main()
{
	gl_Position = vec4(a_position.xy * turn.t.x, a_position.z, 1.0);
	v_uv = a_uv;
}
