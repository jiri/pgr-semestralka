#version 330 core

uniform mat4 projection;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 vColor;

void main() {
  gl_Position = projection * vec4(position + vec3(10.0, 10.0, 0.0), 1.0);
  vColor = color;
}
