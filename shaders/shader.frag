#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inCameraPos;

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler2D albedoTexture;
layout (set = 1, binding = 1) uniform sampler2D metalnessRoughnessTexture;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

vec3 gammaCorrection(vec3 v);
vec3 reinhardTonemap(vec3 v);

const float PI = 3.14159265359;
const float gamma = 1.8;
const float exposure = 1.0;

#define N_LIGHTS 10

void main()
{
    vec3 albedo = texture(albedoTexture, inTexCoord).rgb;
    vec2 metalnessRoughnessCombined = texture(metalnessRoughnessTexture, inTexCoord).rg;
    float metalness = metalnessRoughnessCombined.x;
    float roughness = metalnessRoughnessCombined.y;

    if (texture(albedoTexture, inTexCoord).a < 0.5) discard;

    vec3 N = normalize(inNormal);
    vec3 V = normalize(inCameraPos - inWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);

    vec3 lightPositions[] = {
        vec3(-6.f, -5.f, 0.0f),
        vec3(-5.f, -3.f, 0.f),
        vec3(-4.f, -5.f, -0.f),
        vec3(-3.f, -3.f, -0.f),
        vec3(-1.f, -5.f, -0.f),
        vec3(1.f, -3.f, 0.f),
        vec3(3.f, -5.f, 0.f),
        vec3(0.f, -2.5f, 0.5f),
        vec3(5.5f, -3.f, 0.5f),
    vec3(2.157111, -0.254994, -0.576244)

    };
    vec3 lightColor = vec3(1.0f);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    //Variables common to BRDFs
    vec3 lightDir = normalize(-vec3(0.5, 1.0, 0.2));
    vec3 halfway  = normalize(lightDir + V);
    float nDotV = max(dot(N, V), 0.0);
    float nDotL = max(dot(N, lightDir), 0.0);
    vec3 radianceIn = vec3(1.);

    //Cook-Torrance BRDF
    float NDF = DistributionGGX(N, halfway, roughness);
    float G   = GeometrySmith(N, V, lightDir, roughness);
    vec3  F   = fresnelSchlick(max(dot(halfway,V), 0.0), F0);

    //Finding specular and diffuse component
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specular = numerator / max (denominator, 0.0001);

    Lo += (kD * (albedo / PI) + specular ) * radianceIn * nDotL;

    for (int i = 0; i < N_LIGHTS; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - inWorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i] - inWorldPos);
        float attenuation = 1.0 / (distance * distance) * 10;
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

    vec3 ambient = vec3(0.00001) * albedo;
    vec3 color = ambient + Lo;

    color = reinhardTonemap(color);
    color = gammaCorrection(color);

    outColor = vec4(color, 1.0);
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