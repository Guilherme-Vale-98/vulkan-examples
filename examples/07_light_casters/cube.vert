#version 460
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

layout(set = 0, binding = 0, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 0, binding = 1, scalar) uniform Cube {
    mat4 model;
    vec4 objectColor;
    mat3 normalMatrix; 
} cube;

layout(set = 0, binding = 2, scalar) uniform Scene {
    mat4 viewProj;
    vec4 lightDirection;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
    vec4 viewPos;
    float c;
    float l;
    float k;
} scene;


layout(location = 0) out vec3 Normal;
layout(location = 1) out vec3 FragPos;
layout(location = 2) out vec2 uv;

void main() {
    gl_Position = scene.viewProj * cube.model * vec4(vertices[gl_VertexIndex].pos, 1.0);
    Normal = cube.normalMatrix * vertices[gl_VertexIndex].normal;
    FragPos = vec3(cube.model * vec4(vertices[gl_VertexIndex].pos, 1.0));
    uv = vertices[gl_VertexIndex].uv; 
}
