#version 430

in vec2 vUV;

layout(binding=0) uniform sampler2D samMeanA;
layout(binding=1) uniform sampler2D samMeanB;
layout(binding=2) uniform sampler2D samGuide;

layout(location = 0) out vec4 fragData;

void main(void)
{
    vec3  meanA   = texture2D(samMeanA, vUV).rgb;
    vec3  meanB   = texture2D(samMeanB, vUV).rgb;
    float I       = texture2D(samGuide, vUV).r;

    fragData = vec4(meanA * I + meanB, 1.0);
}