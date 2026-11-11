#include "app_internall.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

struct Player Player = {0};
struct Sphere Sphere = {0};
struct Light Light = {0};

void APP_GameInputHandle() {
  int key_count = 0;
  const bool *keys = SDL_GetKeyboardState(&key_count);
  const float SPEED = 1.0;
  float forward[3] = {0};
  forward[2] = 1 * SDL_cos(Player.rotation[1]);
  forward[0] = -1 * SDL_sin(Player.rotation[1]);
  float right[3] = {0};
  right[2] = 1 * SDL_sin(Player.rotation[1]);
  right[0] = 1 * SDL_cos(Player.rotation[1]);
  
  
  if (keys[SDL_SCANCODE_W]) {
    Player.position[2] -= forward[2] * delta_time * SPEED;
    Player.position[0] -= forward[0]*delta_time * SPEED;    
  }
  if (keys[SDL_SCANCODE_S]) {
    Player.position[2] += forward[2] * delta_time * SPEED;
    Player.position[0] += forward[0] * delta_time * SPEED;
  }
  if (keys[SDL_SCANCODE_A]) {
    Player.position[2] -= right[2] * delta_time * SPEED;
    Player.position[0] -= right[0] * delta_time * SPEED;
  }
  if (keys[SDL_SCANCODE_D]) {
    Player.position[2] += right[2] * delta_time * SPEED;
    Player.position[0] += right[0] * delta_time * SPEED;
  }
  if (keys[SDL_SCANCODE_SPACE]) {
    Player.position[1] += delta_time * SPEED;
  }
  if (keys[SDL_SCANCODE_LSHIFT]) {
    Player.position[1] -= delta_time * SPEED;    
  }

  const float sensitivity = 0.01;
  float dx, dy;
  SDL_GetRelativeMouseState(&dx, &dy);
  Player.rotation[1] += dx*sensitivity;
}

void APP_GameRotateYMatix(float angle, float mat[static 9]) {
  float c = SDL_cos(angle);
  float s = SDL_sin(angle);
  mat[0] = c;
  mat[1] = 0.0;
  mat[2] = s;
  mat[3] = 0.0;
  mat[4] = 1.0;
  mat[5] = 0.0;
  mat[6] = -s;
  mat[7] = 0.0;
  mat[8] = c;
}

