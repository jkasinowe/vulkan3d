#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Vertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
};

#ifdef VS
VS
layout (location = 0) out vec3 outColor;
layout (buffer_reference, scalar) readonly buffer VertexBuffer {
  Vertex vertices[];
};
layout (push_constant) uniform PushConstants {
  VertexBuffer vertexBuffer;
} pc;
#endif

#ifdef FS
FS
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 fragColor;
#endif

struct Player {
  vec3 position;
};
				  

struct Sphere {
  vec3 position;
  float radius;
};

layout(scalar, binding = 0) uniform UniformBufferObject {
  layout(row_major)mat4 model;
  layout(row_major)mat4 view;
  layout(row_major)mat4 projection;
  vec2 screenSize;
  float time;
} ubo;

float aspectRatio = (ubo.screenSize.x/ubo.screenSize.y);

#ifdef VS
void vertex() {
  Vertex vertex = pc.vertexBuffer.vertices[gl_VertexIndex];
  gl_Position = ubo.projection*ubo.view*ubo.model*vec4(vertex.position, 1.0);
}
#endif

#ifdef FS
void fragment() {
  fragColor = vec4(1.0, sin(ubo.time)*0.5+0.5, 0.0, 1.0);
}
#endif


