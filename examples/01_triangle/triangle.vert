#version 450
#extension GL_EXT_buffer_reference    : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 pos;
    vec3 color;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex v[];
};

layout(push_constant, scalar) uniform Push {
    mat4         mvp;
    VertexBuffer vertices;
} pc;

layout(location = 0) out vec3 outColor;

void main() {
    Vertex vtx  = pc.vertices.v[gl_VertexIndex];
    gl_Position = pc.mvp * vec4(vtx.pos, 1.0);
    outColor    = vtx.color;
}
