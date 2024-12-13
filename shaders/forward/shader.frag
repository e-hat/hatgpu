#version 460

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inCameraPos;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform CameraBuffer{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec3 position;
} cameraData;

struct DirLight
{
    vec3 direction;
    vec3 color;
};
layout (std140, set = 0, binding = 2) uniform DirLightBuffer
{
    DirLight dirLight;
};

struct PointLight
{
    vec3 position;
    vec3 color;
};
// A big list of point lights in the scene
layout (std140, set = 0, binding = 3) readonly buffer LightBuffer
{
    PointLight lights[];
} lightBuffer;

layout (set = 1, binding = 0) uniform sampler2D albedoTexture;
layout (set = 1, binding = 1) uniform sampler2D metalnessRoughnessTexture;

// uses a "physically-based" shading model
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

vec3 gammaCorrection(vec3 v);
vec3 reinhardTonemap(vec3 v);

const float PI = 3.14159265359;
const float gamma = 1.8;
const float exposure = 1.0;
const vec3 ambient = vec3(0.2);

void main()
{
    vec4 albedoRgba = texture(albedoTexture, inTexCoord);
    vec3 albedo = albedoRgba.rgb;
    float alpha = albedoRgba.a;
    vec2 metalnessRoughnessCombined = texture(metalnessRoughnessTexture, inTexCoord).rg;
    float metalness = metalnessRoughnessCombined.x;
    float roughness = metalnessRoughnessCombined.y;

    // DIRECTIONAL LIGHT
    vec3 N = normalize(inNormal);
    vec3 V = normalize(inCameraPos - inWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);

    vec3 lightColor = dirLight.color;

    // reflectance equation
    vec3 Lo = vec3(0.0);

    //Variables common to BRDFs
    vec3 lightDir = normalize(dirLight.direction);
    vec3 halfway  = normalize(lightDir + V);
    float nDotV = max(dot(N, V), 0.0);
    float nDotL = max(dot(N, lightDir), 0.0);
    vec3 radianceIn = vec3(3.);

    //Cook-Torrance BRDF
    float NDF = DistributionGGX(N, halfway, roughness);
    float G   = GeometrySmith(N, V, lightDir, roughness);
    vec3  F   = fresnelSchlick(max(dot(halfway, V), 0.0), F0);

    //Finding specular and diffuse component
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specular = numerator / max (denominator, 0.0001);

    Lo += (kD * (albedo / PI) + specular ) * radianceIn * nDotL;

    // POINT LIGHTS
    for (uint i = 0; i < lightBuffer.lights.length(); ++i)
    {
        vec3 lightPosition = lightBuffer.lights[i].position.xyz;
        vec3 lightColor = lightBuffer.lights[i].color.rgb;

        // calculate per-light radiance
        vec3 L = normalize(lightPosition - inWorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPosition - inWorldPos);
        float attenuation = 1.f / (distance * distance) * 5.f;
        vec3 radiance     = lightColor * attenuation;

        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness;

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;

        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = ambient * albedo;
    vec3 color = ambient + Lo;

    color = reinhardTonemap(color);
    color = gammaCorrection(color);

    outColor = vec4(color, alpha);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 reinhardTonemap(vec3 v) {
    return v / (v + vec3(1.0));
}

vec3 gammaCorrection(vec3 v) {
    return pow(v, vec3(gamma));
}

