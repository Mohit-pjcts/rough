/*
 * fog_of_war.h
 *
 * Fog of War visibility system for TermiCraft.
 * Handles block reveal radius by depth zone and world rendering
 * with buffered output (no flicker).
 *
 * Surface/dirt: always visible in viewport.
 * Underground (stone layer): 3-block circular radius, permanent reveal.
 * Deep layer: 2-block circular radius, permanent reveal.
 *
 * Author: Mohit
 */

#ifndef FOG_OF_WAR_H
#define FOG_OF_WAR_H

#include <string>
#include "types.h"

// Get the visibility radius for a given world row.
// Returns -1 if that row is always visible (surface/dirt).
int getVisibilityRadius(int worldY);

// Update which blocks are visible based on player position.
// Call once per tick or whenever the player moves.
void updateWorldVisibility(GameState& state);

// Render the entire world viewport + HUD into the terminal.
// Builds world, HUD and status line into one buffer, then does a single
// write() call — guarantees no tearing or flicker between frames.
// statusMsg: the last game message to show at the bottom (empty = hide).
void renderWorld(const GameState& state, const std::string& statusMsg);

#endif
