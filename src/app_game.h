#include <SDL3/SDL.h>

void APP_GameRotateYMatix(float angle, float mat[static 9]);

extern double delta_time;

struct Sphere {
  float position[3];
  float radius;
};

struct Light {
  float direction[3];
  float ambientColor[3];
};

struct Player {
  float position[3];
  float rotation[3];
};


void APP_GameInputHandle();
