#version 300 es
precision highp float;

uniform vec4 uBounds;
uniform vec4 uLight;
uniform vec2 uFramebufferSize;

const vec2 corners[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

out vec2 vPixelCoordinate;

void main() {
  vec2 local = corners[gl_VertexID];
  float drawExtent = max(uLight.z, 1.0);
  vec4 drawBounds = uBounds + vec4(vec2(-drawExtent), vec2(drawExtent));
  vec2 position = mix(drawBounds.xy, drawBounds.zw, local);
  vec2 ndc = position / uFramebufferSize * 2.0 - 1.0;
  ndc.y = -ndc.y;
  gl_Position = vec4(ndc, 0.0, 1.0);
  vPixelCoordinate = position;
}
