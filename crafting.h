// =============================================================================
// crafting.h
// TermiCraft — Crafting & Inventory System Module Header
//
// Declares the crafting and inventory system for TermiCraft. This module
// handles all equipment progression from Wood to Diamond, including the
// strict recipe validation, resource management, and minigame triggering
// mechanics required by the game's lore.
//
// The crafting system enforces a linear progression: Wood → Stone → Iron →
// Gold → Diamond, where each tier upgrade after Stone triggers a "rite of
// passage" minigame (Wordle for Iron, Minesweeper for Gold, random for Diamond).
// Crafting recipes are defined as a constant array of CraftingRecipe structs,
// ensuring compile-time validation and easy balancing.
//
// Integration with the player module is handled through the pendingUpgrade
// flag in GameState, which signals the main loop to launch the appropriate
// minigame when a player crafts beyond the Stone tier.
//
// Author:       Koki
// Dependencies: types.h, <string>
// =============================================================================

#ifndef CRAFTING_H
#define CRAFTING_H

#include <string>

#include "types.h"

// Main entry point for the crafting menu
// Handles the full UI loop and returns when player presses Q or when a
// minigame is triggered (sets state.minigameActive and state.phase)
void openCraftingMenu(GameState& state);

// Detailed inventory display (called when player presses 'I')
void showInventory(const GameState& state);

// Check if player meets requirements for a recipe
bool canCraft(const GameState& state, const CraftingRecipe& recipe);

// Get specific error message for why crafting failed
std::string getCraftingError(const GameState& state, const CraftingRecipe& recipe);

// Execute crafting. Returns true if a minigame was triggered (Iron/Gold/Diamond)
bool performCrafting(GameState& state, const CraftingRecipe& recipe);

#endif