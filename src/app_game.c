#ifndef APP_GAME_H
#define APP_GAME_H
#include "app_internall.h"

struct Player Player = {0};
struct Sphere Sphere = {0};
struct Light Light = {.ambientColor = {1, 1, 1}};

void APP_GameInputHandle() {
  int key_count = 0;
  const bool *keys = SDL_GetKeyboardState(&key_count);

  float matrix[16] = VM44_IDENTITY;
  VM44_Rotate(matrix, Player.rotation);
  
  float move[3] = {0, 0, 0};
  float forward[3] = {0, 0, -1};
  float right[3] = {1, 0, 0};

  VM44_V3Multiply(matrix, forward);
  VM44_V3Multiply(matrix, right);  
  
  float forward_scaled[3];
  float right_scaled[3];

  VM3_ScaleO(forward, delta_time*APP_PLAYER_SPEED, forward_scaled);
  VM3_ScaleO(right, delta_time*APP_PLAYER_SPEED, right_scaled);

  if (keys[SDL_SCANCODE_W]) {
    VM3_Add(move, forward_scaled);
  }
  if (keys[SDL_SCANCODE_S]) {
    VM3_Subtract(move, forward_scaled);    
  }
  if (keys[SDL_SCANCODE_D]) {
    VM3_Add(move, right_scaled);
  }
  if (keys[SDL_SCANCODE_A]) {
    VM3_Subtract(move, right_scaled);
  }
  if (keys[SDL_SCANCODE_SPACE]) {
    move[1] += delta_time * APP_PLAYER_SPEED;
  }
  if (keys[SDL_SCANCODE_LSHIFT]) {
    move[1] -= delta_time * APP_PLAYER_SPEED;    
  }

  const float sensitivity = 0.01;
  float dx, dy;
  SDL_GetRelativeMouseState(&dx, &dy);
  Player.rotation[1] -= dx * sensitivity;
  VM3_Add(Player.position, move);
}

#endif
