// =============================================================================
// world_gen.h
// TermiCraft — World Generation Module Header
//
// Declares world allocation and procedural generation for TermiCraft.
// Three biomes: Forest (cols 0–49), Cave (cols 50–139), Light Cave (cols 140–199).
//
// Author:       Mohit
// Dependencies: types.h
// =============================================================================

#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "types.h"

// Allocate state.world as Block**  (rows × cols)
void initWorld(GameState& state);

// Fill the allocated world with terrain, ores, caves, trees, and the dragon portal
void generateWorld(GameState& state);

#endif
