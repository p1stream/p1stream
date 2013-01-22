#version 150

uniform sampler2DRect u_Texture;
in vec2 a_Position;
in vec2 a_TexCoords;
out vec2 v_TexCoords;

void main(void) {
    gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0);
    v_TexCoords = a_TexCoords * textureSize(u_Texture);
}