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
} scene;

layout(set = 0, binding = 3, scalar) uniform Material{
    vec3 ambient;
    vec3 specular;
    vec3 diffuse;
    float shininess;
} material;

layout(location = 0) in vec3 Normal;
layout(location = 1) in vec3 FragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 ambient = material.ambient * scene.lightAmbient.rgb;


    vec3 lightDir = normalize(scene.lightPos.xyz - FragPos);
    vec3 viewDir = normalize(scene.viewPos.xyz - FragPos);
    vec3 norm = normalize(Normal);

    vec3 reflectDir = reflect(-lightDir, norm);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * material.diffuse * scene.lightDiffuse.rgb;


    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = material.specular * spec * scene.lightSpecular.rgb;
    vec3 result = (ambient + diffuse + specular);
    outColor = vec4(result, 1.0);
}
