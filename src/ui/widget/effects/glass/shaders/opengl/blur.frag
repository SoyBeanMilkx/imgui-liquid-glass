#version 300 es
precision highp float;

uniform sampler2D uSource;
uniform vec2 uInverseSourceResolution;
uniform vec2 uDirection;
uniform float uCenterWeight;
uniform float uPairCount;
uniform float uPassKind;
uniform float uPairWeights[12];
uniform float uPairOffsets[12];

in vec2 vUv;
out vec4 outColor;

// The runtime shader loader prepends common/blur_kernel.glslinc here.

void main() {
  if (uPassKind < 0.5) {
    outColor = sampleBoxDown(uSource, vUv, uInverseSourceResolution);
  } else {
    outColor = sampleGaussian(
        uSource, vUv, uInverseSourceResolution, uDirection,
        uCenterWeight, uPairCount, uPairWeights, uPairOffsets);
  }
}
