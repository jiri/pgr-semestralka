#version 330 core

uniform mat4 MVP;

layout (location = 0) in vec3 position;

uniform sampler2D tex;

out vec3 vpos;

void main() {
  float h = texture(tex, position.xy / 2048.0).x * 500;
  gl_Position = MVP * vec4(position.xy, h, 1.0);
  vpos = position;
}
