#version 330 core

uniform sampler2D heightmap;
uniform float map_size;
/* uniform float height; */

in vec3 vpos;

out vec4 color;

void main() {
  /* color = vec4(0.898, 0.867, 0.796, 1.0); */
  /* color = vec4(vpos.zzz / height, 1.0); */
  color = texture(heightmap, vpos.xy / map_size);
}
