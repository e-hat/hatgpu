#version 460

layout(location = 0) out vec4 outColor;

#define COLORS_LENGTH 3
vec3 colors[COLORS_LENGTH] = {
  vec3(1.0, 0.0, 0.0),
  vec3(0.0, 1.0, 0.0),
  vec3(0.0, 0.5, 1.0),
};

void main() {
  outColor = vec4(colors[(gl_PrimitiveID / 12) % COLORS_LENGTH], 1.0);
}
