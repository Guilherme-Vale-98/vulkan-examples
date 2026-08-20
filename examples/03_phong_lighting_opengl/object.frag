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
    vec4 lightColor;
    vec4 viewPos;
} scene;

layout(location = 0) in vec3 Normal;
layout(location = 1) in vec3 FragPos;

layout(location = 0) out vec4 outColor;

void main() {
    float ambientStrength = 0.1f;
    float specularStrength = 0.5f;
    vec3 ambient = ambientStrength * scene.lightColor.rgb;

    vec3 lightDir = normalize(scene.lightPos.xyz - FragPos);
    vec3 viewDir = normalize(scene.viewPos.xyz - FragPos);
    vec3 norm = normalize(Normal);
    vec3 reflectDir = reflect(-lightDir, norm);
    float diff = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * scene.lightColor.rgb;
    vec3 diffuse = diff * scene.lightColor.rgb;
    vec3 result = (ambient + diffuse + specular) * cube.objectColor.rgb;
    outColor = vec4(result, 1.0);
}
