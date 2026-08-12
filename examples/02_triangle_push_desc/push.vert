#version 450
#extension GL_EXT_buffer_reference    : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 pos;
    vec3 color;
};

layout(set = 0,binding = 0, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};


layout(set  = 0, binding = 1 ) uniform Transform{
  mat4 mvp;
} xform;

layout(location = 0) out vec3 outColor;

void main() {
    Vertex vtx  =vertices[gl_VertexIndex];
    gl_Position = xform.mvp * vec4(vtx.pos, 1.0);
    outColor    = vtx.color;
}
