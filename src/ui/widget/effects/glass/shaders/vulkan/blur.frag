#version 450
#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D uSource;

layout(push_constant) uniform BlurPush {
  vec2 inverseSourceResolution;
  vec2 direction;
  float centerWeight;
  float pairCount;
  float passKind;
  float padding;
  float pairWeights[12];
  float pairOffsets[12];
} pc;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

#include "../common/blur_kernel.glslinc"

void main() {
  if (pc.passKind < 0.5) {
    outColor = sampleBoxDown(uSource, vUv, pc.inverseSourceResolution);
  } else {
    outColor = sampleGaussian(
        uSource, vUv, pc.inverseSourceResolution, pc.direction,
        pc.centerWeight, pc.pairCount, pc.pairWeights, pc.pairOffsets);
  }
}
