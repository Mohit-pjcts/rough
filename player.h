// =============================================================================
// player.h
// TermiCraft — Player & Crafting System Module Header
// =============================================================================

#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include "types.h"

// Initialize player at game start
void initPlayer(GameState& state, const std::string& playerName);

// Main input handler: processes a single keypress from main loop
void handleInput(GameState& state, char input);

// Free 4-directional movement (no gravity/physics)
bool movePlayer(GameState& state, int dx, int dy);

// Mine the block the player is facing
void initiateMining(GameState& state);

// Resolve mining attempt after minigame completes
void resolveMiningAttempt(GameState& state, bool minigameWon);

// Check if current tool can mine a specific block type
bool canPlayerMine(const GameState& state, BlockType block);

// Calculate required material tier to mine a block
MaterialTier getRequiredTierForBlock(BlockType block);

// Check if a block type is solid (collision)
bool isSolidBlock(BlockType type);

// Adjust camera position to center on player
void updateCamera(GameState& state);

// Damage/healing handlers
void damagePlayer(GameState& state, int amount);
void healPlayer(GameState& state, int amount);

// Crafting helpers
bool hasResourcesForTier(const GameState& state, MaterialTier targetTier);
void consumeResourcesForTier(GameState& state, MaterialTier tier);
bool checkCraftingProgression(GameState& state);
void confirmUpgrade(GameState& state);

// Enemy spawn roll (called after successful mine)
void trySpawnEnemy(GameState& state, Position minedPos);

// Get color code for player based on armor tier
const char* getPlayerArmorColor(const GameState& state);

#endif
