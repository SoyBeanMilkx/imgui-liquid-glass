#version 450

layout(push_constant) uniform GlassPush {
  vec4 bounds;
  vec4 radii;
  vec4 refraction;
  vec4 filterParams;
  vec4 tint;
  vec4 framebuffer;
  vec4 deformation;
  vec4 lighting;
  vec4 light;
} pc;

layout(location = 0) out vec2 vPixelCoordinate;

void main() {
  const vec2 corners[6] = vec2[6](
      vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
      vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
  vec2 local = corners[gl_VertexIndex];
  float drawExtent = max(pc.light.z, 1.0);
  vec4 drawBounds = pc.bounds + vec4(vec2(-drawExtent), vec2(drawExtent));
  vec2 position = mix(drawBounds.xy, drawBounds.zw, local);
  vec2 ndc = position / pc.framebuffer.xy * 2.0 - 1.0;
  gl_Position = vec4(ndc, 0.0, 1.0);
  vPixelCoordinate = position;
}
