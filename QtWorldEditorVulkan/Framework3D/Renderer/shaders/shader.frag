#version 450

layout(location = 0) in vec2 vFragTexCoord;
layout(location = 1) in vec3 vWorldNormal;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 albedo = texture(texSampler, vFragTexCoord).rgb;

	// TODO: Move to the rendering layer
    // SET THE LIGHT DIRECTION RIGHT HERE (world space)
    vec3 lightDir = normalize(vec3(0.3, -1.0, 0.2));

    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(-lightDir);

    float NdotL = max(dot(N, L), 0.0);

    float ambient = 0.15;
    vec3 color = albedo * (ambient + NdotL);

    outColor = vec4(color, 1.0);
}
