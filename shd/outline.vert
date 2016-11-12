#version 330 core

uniform mat4 MVP;

uniform sampler2D heightmap;
uniform float map_size;
uniform float height;

layout (location = 0) in vec3 position;

out vec3 vpos;

void main() {
  float h = texture(heightmap, position.xy / map_size).z * height;
  gl_Position = MVP * vec4(position.xy, h, 1.0);
  vpos = position;
}
