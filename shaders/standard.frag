#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_OES_standard_derivatives : require

layout (location = 0) in flat int vsBaseInstance;
layout (location = 1) in flat int vsIsSkybox;
layout (location = 2) in vec3 vsNORMAL;
layout (location = 3) in vec2 vsTexcoord_0;
layout (location = 4) in vec3 vsSkyboxUVW;
layout (location = 5) in vec3 vsViewDir;
layout (location = 6) in vec3 vsViewSpaceVertPos;
layout (location = 7) in mat3 vsTBN;


layout (location = 0) out vec4 outFrag_Color;

layout(set = 3, binding = 0) uniform samplerCube samplerCubeMap;
layout(set = 3, binding = 1) uniform sampler   samp;
layout(set = 3, binding = 1) uniform texture2D textures[];

struct MaterialData
{
    int base_color_texture_idx;
    int emissive_texture_idx;
    int normal_texture_idx;
    int metallic_roughness_texture_idx;
    float tiling_x;
    float tiling_y;
    float metallic_factor;
    float roughness_factor;
    vec4 base_color_factor;
};

layout(set = 2, binding = 0)  buffer SSBO {
    MaterialData data[];
} material;

const vec3 lightColor = vec3(.3, .3, .3);
const float lightPower = 17.0;
const vec3 ambientColor = vec3(0.9, 0.9, 0.9); // ?
const vec3 specColor = vec3(.04, .04, .04);
const float shininess = 2.0;
const float screenGamma = 1.0; // Assume the monitor is calibrated to the sRGB color space

void main()
{
    // If it's a skybox, just sample it and return
    if (vsIsSkybox == int(1)) {
        outFrag_Color = texture(samplerCubeMap, vsSkyboxUVW);
        // outFrag_Color = vec4(0.6,0.1,0.4, 1.0);
        return;
    }


    MaterialData mat = material.data[vsBaseInstance];

    int baseColorMapIdx = mat.base_color_texture_idx;
    int normalMapIdx = mat.normal_texture_idx;

    vec2 tiledTexCoord = vec2(vsTexcoord_0.x * mat.tiling_x, vsTexcoord_0.y * mat.tiling_y);




    vec4 baseColor;
    if (baseColorMapIdx > -1) {
        baseColor = texture(sampler2D(textures[baseColorMapIdx], samp), tiledTexCoord) * mat.base_color_factor;
    }
    else
    {
        baseColor = mat.base_color_factor;
    }

    vec3 sampledTangentNormal;
    if (normalMapIdx > -1) {
        sampledTangentNormal = texture(sampler2D(textures[normalMapIdx], samp), tiledTexCoord).xyz;
    } else {
        sampledTangentNormal = vec3(1);
    }

    // vec3 lightDir = vec3(-.40, 1.5, -1.8);
    vec3 lightDir = vec3(.5, 2.5, -.5);
    lightDir=normalize(lightDir);
    // // float reflectance = normalize(dot(vsViewDir, vsNORMAL));
    // float reflectance = normalize(dot(vec3(.10, 1.3, -1.8), vsNORMAL));

    // vec4 lightColor = vec4(.5,.5,.5, 1);
    // vec4 diffuse = reflectance * lightColor;

    // float ambientStrength = .3;
    // vec4 ambient = ambientStrength * baseColor;

    // vec4 lighting = diffuse + ambient * 4.5;
    // vec4 lt = step(.2, diffuse) * ambient;


    // vec4 finalColor;

    // finalColor = baseColor  * (lighting + lt);

    // outFrag_Color = finalColor;
    vec3 normalInterp = sampledTangentNormal * vsNORMAL;

    vec3 normal = normalize(normalInterp);
    float distance = length(lightDir);
    distance = distance * distance;
    lightDir = normalize(lightDir);


    float lambertian = max(dot(lightDir, normal), 0.0);
    float specular = 0.1;

    if (lambertian > 0.5) {
        vec3 viewDir = normalize(vsViewSpaceVertPos);

        vec3 halfDir = normalize(lightDir );
        float specAngle = max(dot(halfDir, normal), 0.0);
        specular = pow(specAngle, shininess);

    }

    vec3 colorLinear = ambientColor * vec3(baseColor).xyz +
                     vec3(baseColor).xyz * lambertian * lightColor * lightPower / distance +
                     specColor * specular * lightColor * lightPower / distance;
    // apply gamma correction (assume ambientColor, diffuseColor and specColor
    // have been linearized, i.e. have no gamma correction in them)
    vec3 colorGammaCorrected = pow(colorLinear, vec3(1.0 / screenGamma));
    outFrag_Color = vec4(colorGammaCorrected, 1.0);

}