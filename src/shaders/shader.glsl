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
layout (location = 1) out vec3 outPosition;
layout (location = 2) out vec3 outNormal;
layout (buffer_reference, scalar) readonly buffer VertexBuffer {
  Vertex vertices[];
};
layout (buffer_reference, scalar) readonly buffer IndexBuffer {
  uint indices[];
};
layout (buffer_reference, scalar) readonly buffer InstanceBuffer {
  mat4 model_matrix[];
};

layout (push_constant) uniform PushConstants {
  IndexBuffer indexBuffer;
  VertexBuffer vertexBuffer;
  InstanceBuffer instanceBuffer;
} pc;
#endif

#ifdef FS
FS
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 0) out vec4 fragColor;
#endif

layout(scalar, binding = 0) uniform UniformBufferObject {
  layout(row_major)mat4 model;
  layout(row_major)mat4 view;
  layout(row_major)mat4 projection;
  vec2 screenSize;
  float time;
  vec3 lightColor;
  vec3 lightPosition;
} ubo;

float aspectRatio = (ubo.screenSize.x/ubo.screenSize.y);

#ifdef VS
void vertex() {
  mat4 model = pc.instanceBuffer.model_matrix[gl_InstanceIndex];
  mat3 normal_matrix = transpose(inverse(mat3(model)));

  Vertex vertex = pc.vertexBuffer.vertices[pc.indexBuffer.indices[gl_VertexIndex]];
  vec4 world_position = model*vec4(vertex.position, 1.0);
  gl_Position = ubo.projection*ubo.view*world_position;
  gl_Position.y *= -1;
  outNormal = normalize(normal_matrix*vertex.normal);
  outPosition = world_position.xyz;
}
#endif

#ifdef FS
void fragment() {
  vec3 direction_to_light = normalize(ubo.lightPosition - inPosition);
  float light_factor = dot(direction_to_light, inNormal);
  fragColor = vec4(ubo.lightColor*light_factor*0.2, 1.0);
}
#endif


