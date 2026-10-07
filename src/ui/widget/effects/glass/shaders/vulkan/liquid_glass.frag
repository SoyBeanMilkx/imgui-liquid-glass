#version 450
#extension GL_GOOGLE_include_directive : require

// Adapted from AndroidLiquidGlassView@9cdd915; (c) 2025 QmDeve, MIT.

layout(set = 0, binding = 0) uniform sampler2D uBackdrop;
layout(set = 0, binding = 1) uniform sampler2D uRawBackdrop;

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

layout(location = 0) in vec2 vPixelCoordinate;
layout(location = 0) out vec4 outColor;

#include "../common/glass_math.glslinc"
#include "../common/glass_color.glslinc"
#include "../common/glass_lighting.glslinc"

vec4 sampleContent(vec2 pixelCoordinate) {
  vec2 halfTexel = 0.5 / vec2(textureSize(uBackdrop, 0));
  vec2 uv = clamp(pixelCoordinate / pc.framebuffer.xy, halfTexel,
                  vec2(1.0) - halfTexel);
  float blurMix = clamp(pc.deformation.z, 0.0, 1.0);
  vec4 color;
  if (blurMix >= 0.999)
    color = texture(uBackdrop, uv);
  else if (blurMix <= 0.001)
    color = texture(uRawBackdrop, uv);
  else
    color = mix(texture(uRawBackdrop, uv), texture(uBackdrop, uv), blurMix);
  if (pc.framebuffer.z > 0.5)
    color.rgb = fromLinearSrgb(color.rgb);
  return color;
}

vec4 encodeForAttachment(vec4 color) {
  if (pc.framebuffer.z > 0.5)
    color.rgb = toLinearSrgb(color.rgb);
  color.a = pc.filterParams.w;
  return color;
}

vec4 applyColorFilter(vec4 color) {
  return filterLiquidGlassColor(color, pc.filterParams.x, pc.filterParams.y,
                                pc.filterParams.z, pc.tint);
}

void main() {
  vec2 size = max(pc.bounds.zw - pc.bounds.xy, vec2(1.0));
  vec2 halfSize = size * 0.5;
  vec2 centeredCoordinate = vPixelCoordinate - (pc.bounds.xy + halfSize);
  bool isCircle = pc.framebuffer.w > 0.5;
  vec2 shapeHalfSize = jellyHalfSize(halfSize, pc.deformation.xy);
  vec2 shapeCoordinate =
      jellyCoordinate(centeredCoordinate, halfSize, pc.deformation.xy);
  float radius = min(radiusAt(shapeCoordinate, pc.radii),
                     min(shapeHalfSize.x, shapeHalfSize.y));
  float signedDistance = isCircle
      ? sdCircle(shapeCoordinate, min(shapeHalfSize.x, shapeHalfSize.y))
      : sdRoundedRect(shapeCoordinate, shapeHalfSize, radius);
  float smoothRadius = min(max(radius * 1.5, 30.0),
                           min(shapeHalfSize.x, shapeHalfSize.y));
  vec2 shapeGradient = isCircle
      ? gradSdCircle(shapeCoordinate)
      : gradSdRoundedRect(shapeCoordinate, shapeHalfSize, smoothRadius);

  // Resolve derivatives and both sides of the edge before any early return.
  vec2 distanceGradient =
      vec2(dFdx(signedDistance), dFdy(signedDistance));
  float coverage = min(size.x, size.y) <= 128.0
                       ? pixelCoverage(signedDistance, distanceGradient)
                       : antialiasedCoverage(signedDistance);
  float shadow = pc.lighting.w > 0.0 && signedDistance < pc.light.z
                     ? glassOuterShadow(max(signedDistance, 0.0),
                                        shapeGradient, pc.lighting, pc.light)
                     : 0.0;
  if (coverage <= 0.0) {
    if (shadow <= 0.001)
      discard;
    outColor = vec4(0.0, 0.0, 0.0, shadow * pc.filterParams.w);
    return;
  }

  // Partially covered pixels outside the edge are shaded as edge pixels.
  float shadeDistance = min(signedDistance, -0.001);
  float refractionHeight = max(pc.refraction.x, 0.001);
  vec4 color;
  if (-shadeDistance >= refractionHeight) {
    color = sampleContent(vPixelCoordinate);
  } else {
    float depth = 1.0 - (-shadeDistance / refractionHeight);
    float displacement = circleMap(depth) * pc.refraction.y;
    vec2 depthGradient = safeNormalize(shapeCoordinate, shapeGradient);
    vec2 gradient = safeNormalize(
        shapeGradient + pc.refraction.z * depthGradient, shapeGradient);
    vec2 refractedCoordinate = vPixelCoordinate + displacement * gradient;
    float dispersionIntensity =
        pc.refraction.w *
        ((shapeCoordinate.x * shapeCoordinate.y) /
         max(shapeHalfSize.x * shapeHalfSize.y, 0.001));
    vec2 dispersedCoordinate =
        displacement * gradient * dispersionIntensity;

    color = vec4(0.0);
    vec4 red = sampleContent(refractedCoordinate + dispersedCoordinate);
    color.r += red.r / 3.5;
    color.a += red.a / 7.0;
    vec4 orange = sampleContent(
        refractedCoordinate + dispersedCoordinate * (2.0 / 3.0));
    color.r += orange.r / 3.5;
    color.g += orange.g / 7.0;
    color.a += orange.a / 7.0;
    vec4 yellow = sampleContent(
        refractedCoordinate + dispersedCoordinate * (1.0 / 3.0));
    color.r += yellow.r / 3.5;
    color.g += yellow.g / 3.5;
    color.a += yellow.a / 7.0;
    vec4 green = sampleContent(refractedCoordinate);
    color.g += green.g / 3.5;
    color.a += green.a / 7.0;
    vec4 cyan = sampleContent(
        refractedCoordinate - dispersedCoordinate * (1.0 / 3.0));
    color.g += cyan.g / 3.5;
    color.b += cyan.b / 3.0;
    color.a += cyan.a / 7.0;
    vec4 blue = sampleContent(
        refractedCoordinate - dispersedCoordinate * (2.0 / 3.0));
    color.b += blue.b / 3.0;
    color.a += blue.a / 7.0;
    vec4 purple = sampleContent(refractedCoordinate - dispersedCoordinate);
    color.r += purple.r / 7.0;
    color.b += purple.b / 3.0;
    color.a += purple.a / 7.0;
  }

  color = applyColorFilter(color);
  color.rgb = applyGlassLighting(color.rgb, signedDistance, shapeGradient,
                                 refractionHeight, distanceGradient,
                                 pc.lighting, pc.light);
  outColor = encodeForAttachment(color);
  // Shadow only fills the uncovered portion of edge pixels.
  float glassAlpha = outColor.a * coverage;
  float alpha = glassAlpha + shadow * pc.filterParams.w * (1.0 - coverage);
  outColor = vec4(outColor.rgb * (glassAlpha / max(alpha, 1e-6)), alpha);
}
