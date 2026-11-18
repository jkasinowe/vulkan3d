
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
