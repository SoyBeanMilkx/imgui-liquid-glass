#version 300 es
precision highp float;

// Adapted from AndroidLiquidGlassView@9cdd915; (c) 2025 QmDeve, MIT.

uniform sampler2D uBackdrop;
uniform sampler2D uRawBackdrop;
uniform vec4 uBounds;
uniform vec4 uRadii;
uniform vec4 uRefraction;
uniform vec4 uFilter;
uniform vec4 uTint;
uniform vec4 uFramebuffer;
uniform vec4 uDeformation;
uniform vec4 uLighting;
uniform vec4 uLight;

in vec2 vPixelCoordinate;
out vec4 outColor;

// Inject common glass modules here.

vec4 sampleContent(vec2 pixelCoordinate) {
  vec2 halfTexel = 0.5 / vec2(textureSize(uBackdrop, 0));
  vec2 uv = clamp(pixelCoordinate / uFramebuffer.xy, halfTexel,
                  vec2(1.0) - halfTexel);
  uv.y = 1.0 - uv.y;
  float blurMix = clamp(uDeformation.z, 0.0, 1.0);
  if (blurMix >= 0.999)
    return texture(uBackdrop, uv);
  if (blurMix <= 0.001)
    return texture(uRawBackdrop, uv);
  return mix(texture(uRawBackdrop, uv), texture(uBackdrop, uv), blurMix);
}

vec4 applyColorFilter(vec4 color) {
  color = filterLiquidGlassColor(color, uFilter.x, uFilter.y,
                                 uFilter.z, uTint);
  color.a = uFilter.w;
  return color;
}

void main() {
  vec2 size = max(uBounds.zw - uBounds.xy, vec2(1.0));
  vec2 halfSize = size * 0.5;
  vec2 centeredCoordinate = vPixelCoordinate - (uBounds.xy + halfSize);
  bool isCircle = uFramebuffer.w > 0.5;
  vec2 shapeHalfSize = jellyHalfSize(halfSize, uDeformation.xy);
  vec2 shapeCoordinate =
      jellyCoordinate(centeredCoordinate, halfSize, uDeformation.xy);
  float radius = min(radiusAt(shapeCoordinate, uRadii),
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
  float shadow = uLighting.w > 0.0 && signedDistance < uLight.z
                     ? glassOuterShadow(max(signedDistance, 0.0),
                                        shapeGradient, uLighting, uLight)
                     : 0.0;
  if (coverage <= 0.0) {
    if (shadow <= 0.001)
      discard;
    outColor = vec4(0.0, 0.0, 0.0, shadow * uFilter.w);
    return;
  }

  // Partially covered pixels outside the edge are shaded as edge pixels.
  float shadeDistance = min(signedDistance, -0.001);
  float refractionHeight = max(uRefraction.x, 0.001);
  vec4 color;
  if (-shadeDistance >= refractionHeight) {
    color = sampleContent(vPixelCoordinate);
  } else {
    float depth = 1.0 - (-shadeDistance / refractionHeight);
    float displacement = circleMap(depth) * uRefraction.y;
    vec2 depthGradient = safeNormalize(shapeCoordinate, shapeGradient);
    vec2 gradient = safeNormalize(
        shapeGradient + uRefraction.z * depthGradient, shapeGradient);
    vec2 refractedCoordinate = vPixelCoordinate + displacement * gradient;
    float dispersionIntensity =
        uRefraction.w *
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
                                 uLighting, uLight);
  // Shadow only fills the uncovered portion of edge pixels.
  float glassAlpha = color.a * coverage;
  float alpha = glassAlpha + shadow * uFilter.w * (1.0 - coverage);
  outColor = vec4(color.rgb * (glassAlpha / max(alpha, 1e-6)), alpha);
}
