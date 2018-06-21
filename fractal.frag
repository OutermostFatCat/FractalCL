#version 330

in vec2 v_tex_coord;

out vec4 outputColor;

uniform sampler2D u_texture;

void main()
{
	vec4 tex_color = texture(u_texture, v_tex_coord);
	outputColor = tex_color;
}
