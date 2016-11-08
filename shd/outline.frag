#version 330 core

in vec3 vpos;

out vec4 color;

uniform sampler2D tex;
uniform sampler2D tex2;

void main() {
  /* color = vec4(0.898, 0.867, 0.796, 1.0); */
  color = texture(tex, vpos.xy / 2048.0) * texture(tex2, vpos.xy / 128.0);
}
