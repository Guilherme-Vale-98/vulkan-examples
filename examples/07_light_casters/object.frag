#version 460
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 1, scalar) uniform Cube {
    mat4 model;
    vec4 objectColor;
    mat3 normalMatrix;
} cube;

layout(set = 0, binding = 2, scalar) uniform Scene {
    mat4 viewProj;
    vec4 lightPos;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
    vec4 viewPos;
    float c;
    float l;
    float k;
} scene;

layout(set = 0, binding = 3, scalar) uniform Material{
    float shininess;
} material;

layout(set = 0, binding = 4) uniform sampler2D ContainerTex;
layout(set = 0, binding = 5) uniform sampler2D SpecularTex;
  
layout(location = 0) in vec3 Normal;
layout(location = 1) in vec3 FragPos;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 surfaceColor = texture(ContainerTex, uv).rgb;
    vec3 specularStrength = texture(SpecularTex, uv).rgb;

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(scene.viewPos.xyz - FragPos);
    vec3 ambient = surfaceColor * scene.lightAmbient.rgb;

    vec3 toLight = scene.lightPos.xyz - FragPos;
    float distance = length(toLight);
    vec3 lightDir = normalize(toLight);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * surfaceColor * scene.lightDiffuse.rgb;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(
        max(dot(viewDir, reflectDir), 0.0),
        material.shininess);

    vec3 specular = spec * specularStrength * scene.lightSpecular.rgb;


    float attenuation = 1.0 / (
        scene.c +
        scene.l * distance +
        scene.k* distance * distance
    );
    outColor = vec4((ambient + diffuse + specular) * attenuation, 1.0);
}
