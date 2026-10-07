#version 300 es
precision highp float;

const vec2 positions[3] = vec2[3](
    vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
const vec2 uvs[3] = vec2[3](
    vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

out vec2 vUv;

void main() {
  gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
  vUv = uvs[gl_VertexID];
}
