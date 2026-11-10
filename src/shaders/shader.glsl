#version 460
#extension GL_EXT_scalar_block_layout : enable

#ifdef VS
VS
layout (location = 0) out vec3 outColor;
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
  vec2 screenSize;
  float time;
  vec3 player_position;
  vec3 sphere_position;
  float sphere_radius;
  vec3 light_direction;
  vec3 ambient_color;
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
struct Ray {
  vec3 origin;
  vec3 direction;
  vec3 normal;
  float len;
};

const float MAX_DISTANCE = 10.0;
const float MIN_DISTANCE = 0.001;
const int MAX_ITERATIONS = 50;

float distanceGet(inout Ray ray) {
  vec3 position = ray.origin + ray.direction*ray.len;
  vec3 vector_to = ubo.sphere_position - position;
  ray.normal = -normalize(vector_to);
  return length(vector_to) - ubo.sphere_radius;
}

bool march(inout Ray ray) {
  for (int i = 0; i < MAX_ITERATIONS; i++) {
    float dist = distanceGet(ray);
    ray.len += dist;
    if (dist < MIN_DISTANCE) return true;
    if (dist > MAX_DISTANCE) {
      ray.normal = vec3(0);
      return false;
    }
  }
  return false;
}

const vec4 BLACK = vec4(0, 0, 0, 1);
const vec4 WHITE = vec4(1, 1, 1, 1);


void fragment() {
  vec2 uv = gl_FragCoord.xy/ubo.screenSize;
  uv = (uv - 0.5)*2.0;
  Ray ray;
  ray.origin = ubo.player_position;
  ray.direction = normalize(vec3(uv, -1));
  ray.len = 0;
  bool collided = march(ray);
  float direct_light_factor = clamp(dot(normalize(ubo.light_direction), ray.normal), 0, 1);
  if (collided) {
    fragColor = vec4(vec3(direct_light_factor + ubo.ambient_color), 1.0);
  } else {
    fragColor = vec4(0);
  }
}
#endif


