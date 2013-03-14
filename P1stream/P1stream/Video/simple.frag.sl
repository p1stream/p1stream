#version 150

uniform sampler2DRect u_Texture;
in vec2 v_TexCoords;
out vec4 o_FragColor;

void main(void) {
    o_FragColor = texture(u_Texture, v_TexCoords);
}