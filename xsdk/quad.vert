#version 450
// A corner of the picture's quad: where it sits in clip space and where it reads the board's picture.
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_uv;
layout(location = 0) out vec2 v_uv;

void main()
{
	gl_Position = vec4(a_position, 1.0);
	v_uv = a_uv;
}
