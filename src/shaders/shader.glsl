#version 460
#ifdef VS
VS
layout (location = 0) out vec3 outColor;
#endif

#ifdef FS
FS
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 fragColor;
#endif

struct Sphere {
  vec3 position;
  float radius;
};

layout(binding = 0) uniform UniformBufferObject {
  vec2 screenSize;
  float time;
  vec3 cameraPosition;
  Sphere sphere;
} ubo;

const vec3 positions[4] = vec3[] (
				  vec3(-1, -1, 0.0),
				  vec3(-1,  1, 0.0),
				  vec3( 1,  1, 0.0),
				  vec3( 1, -1, 0.0)
				  );

const vec3 colors[4] = vec3[] (
			       vec3(1.0, 0.0, 0.0),
			       vec3(0.0, 1.0, 0.0),
			       vec3(0.0, 0.0, 1.0),
			       vec3(1.0, 1.0, 1.0)
			       );

const int indices[6] = int[] (0, 1, 2, 0, 2, 3);

#ifdef VS
void vertex() {
  gl_Position = vec4(positions[indices[gl_VertexIndex]], 1.0);
  outColor = colors[indices[gl_VertexIndex]];
}
#endif


#ifdef FS
void fragment() {
  vec2 uv = gl_FragCoord.xy/ubo.screenSize;
  fragColor = vec4(uv,  0.5 + sin(ubo.time*3)*0.5, 1.0);
}
#endif


