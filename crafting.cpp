// =============================================================================
// crafting.cpp
// TermiCraft — Crafting & Inventory System Module Implementation
//
// Implements the crafting and inventory system for TermiCraft. This module
// provides a full-screen terminal UI for the crafting bench and detailed
// inventory display, with real-time validation of resource requirements and
// equipment prerequisites.
//
// The implementation features:
// - A scrolling menu of 10 recipes (5 tiers × 2 equipment types)
// - Color-coded availability (green=craftable, yellow=visible, red=locked)
// - Sidebar showing current inventory and equipment status
// - Detailed cost breakdown and error messages for failed crafts
// - Health bonuses when upgrading armor (+5 to +25 HP per tier)
//
// The crafting loop uses single-character input (W/S/ENTER/Q) for navigation
// and integrates with the existing termios-based getch() function from menu.h.
// When a recipe triggers a minigame (Iron, Gold, or Diamond tiers), the function
// returns immediately, allowing the main loop to transition to PHASE_MINIGAME.
//
// Compilation requires linking with the existing colors.h and menu.h modules.
// No external libraries beyond the standard C++ library are needed.
//
// Author:       Koki
// Dependencies: crafting.h, colors.h, menu.h, player.h, <iostream>, <iomanip>,
//               <algorithm>, <unistd.h> for usleep()
// =============================================================================

#include "crafting.h"

#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "colors.h"
#include "menu.h"    // For getch() and clearScreen()
#include "player.h"  // For checkCraftingProgression() and healPlayer()

// Recipe definitions matching the progression chart
const CraftingRecipe RECIPES[] = {
    // Tier 1: Wood (1 wood each, no prereqs)
    {MATERIAL_WOOD, false, 1, 0, 0, 0, 0, MATERIAL_NONE, MATERIAL_NONE,
     "Wooden Pickaxe", "Basic tool for mining stone"},
    {MATERIAL_WOOD, true, 1, 0, 0, 0, 0, MATERIAL_NONE, MATERIAL_NONE,
     "Wooden Armor", "Crude protection (+5 Max HP)"},

    // Tier 2: Stone (2 stone, requires Wood tools)
    {MATERIAL_STONE, false, 0, 2, 0, 0, 0, MATERIAL_WOOD, MATERIAL_WOOD,
     "Stone Pickaxe", "Mines iron ore efficiently"},
    {MATERIAL_STONE, true, 0, 2, 0, 0, 0, MATERIAL_WOOD, MATERIAL_WOOD,
     "Stone Armor", "Solid protection (+10 Max HP)"},

    // Tier 3: Iron (3 iron, requires Stone tools, triggers Wordle)
    {MATERIAL_IRON, false, 0, 0, 3, 0, 0, MATERIAL_STONE, MATERIAL_STONE,
     "Iron Pickaxe", "Mines gold ore. Rite of Passage required."},
    {MATERIAL_IRON, true, 0, 0, 3, 0, 0, MATERIAL_STONE, MATERIAL_STONE,
     "Iron Armor", "Heavy plating (+20 Max HP)"},

    // Tier 4: Gold (4 gold, requires Iron tools, triggers Minesweeper)
    {MATERIAL_GOLD, false, 0, 0, 0, 4, 0, MATERIAL_IRON, MATERIAL_IRON,
     "Gold Pickaxe", "Mines diamond. Another trial awaits."},
    {MATERIAL_GOLD, true, 0, 0, 0, 4, 0, MATERIAL_IRON, MATERIAL_IRON,
     "Gold Armor", "Gilded defense (+25 Max HP)"},

    // Tier 5: Diamond (5 diamond, requires Gold tools, triggers final trial)
    {MATERIAL_DIAMOND, false, 0, 0, 0, 0, 5, MATERIAL_GOLD, MATERIAL_GOLD,
     "Diamond Pickaxe", "Ultimate mining power"},
    {MATERIAL_DIAMOND, true, 0, 0, 0, 0, 5, MATERIAL_GOLD, MATERIAL_GOLD,
     "Diamond Armor", "Legendary protection (+40 Max HP)"}};

const int NUM_RECIPES = sizeof(RECIPES) / sizeof(RECIPES[0]);

// Helper: Check if recipe is visible (prereqs met) vs locked
bool isRecipeVisible(const GameState& state, const CraftingRecipe& recipe) {
    return state.player.equipment.pickaxe >= recipe.requiredPickaxe &&
           state.player.equipment.armor >= recipe.requiredArmor;
}

// Check if player can craft right now
bool canCraft(const GameState& state, const CraftingRecipe& recipe) {
    if (!isRecipeVisible(state, recipe)) return false;

    const Inventory& inv = state.player.inventory;
    return inv.wood >= recipe.woodCost &&
           inv.stone >= recipe.stoneCost &&
           inv.iron >= recipe.ironCost &&
           inv.gold >= recipe.goldCost &&
           inv.diamond >= recipe.diamondCost;
}

// Get error message for display
std::string getCraftingError(const GameState& state, const CraftingRecipe& recipe) {
    if (!isRecipeVisible(state, recipe)) {
        if (recipe.tier == MATERIAL_WOOD) return "Gather wood first";
        return "Requires " + getMaterialName(recipe.requiredPickaxe) + " tools";
    }

    const Inventory& inv = state.player.inventory;
    if (inv.wood < recipe.woodCost) return "Need " + std::to_string(recipe.woodCost - inv.wood) + " more wood";
    if (inv.stone < recipe.stoneCost) return "Need " + std::to_string(recipe.stoneCost - inv.stone) + " more stone";
    if (inv.iron < recipe.ironCost) return "Need " + std::to_string(recipe.ironCost - inv.iron) + " more iron";
    if (inv.gold < recipe.goldCost) return "Need " + std::to_string(recipe.goldCost - inv.gold) + " more gold";
    if (inv.diamond < recipe.diamondCost) return "Need " + std::to_string(recipe.diamondCost - inv.diamond) + " more diamond";

    return "Unknown error";
}

// Execute crafting
bool performCrafting(GameState& state, const CraftingRecipe& recipe) {
    if (!canCraft(state, recipe)) return false;

    // Consume resources
    state.player.inventory.wood -= recipe.woodCost;
    state.player.inventory.stone -= recipe.stoneCost;
    state.player.inventory.iron -= recipe.ironCost;
    state.player.inventory.gold -= recipe.goldCost;
    state.player.inventory.diamond -= recipe.diamondCost;

    // Apply upgrade
    if (recipe.isArmor) {
        state.player.equipment.armor = recipe.tier;
        // Heal bonus for new armor
        state.player.maxHealth += (recipe.tier * 5);  // +5, +10, +15, +20, +25 incremental
        healPlayer(state, recipe.tier * 5);           // Heal the bonus amount
    } else {
        state.player.equipment.pickaxe = recipe.tier;
    }

    state.lastMessage = std::string("Crafted ") + recipe.displayName + "!";

    // Check for minigame trigger (handled by player.cpp logic)
    if (checkCraftingProgression(state)) {
        return true;  // Minigame was triggered
    }
    return false;
}

// Render the crafting interface
void renderCraftingUI(const GameState& state, int selectedIndex) {
    // Compute centering: box is 80 chars wide
    int termCols, termRows;
    getTermSize(termCols, termRows);
    int leftPad = (termCols - 80) / 2;
    if (leftPad < 0) leftPad = 0;
    int rightCol = leftPad + 80;  // 1-indexed column of right border ║

    std::string PAD(leftPad, ' ');
    // Jump-to-right-border helper: positions cursor at rightCol then prints ║\n
    // Using ANSI CHA (Cursor Horizontal Absolute): \033[Ncol]G
    auto RB = [&]() {
        // CHA is 1-indexed; move to rightCol then print border
        std::cout << "\033[" << rightCol << "G" << COLOR_BOLD_CYAN << "║\n" << COLOR_RESET;
    };

    // Vertical centering: 21 rows total
    int topPad = (termRows - 21) / 2;
    if (topPad < 0) topPad = 0;

    clearScreen();
    if (topPad > 0) std::cout << std::string(topPad, '\n');

    std::cout << COLOR_BOLD_CYAN;
    std::cout << PAD << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << PAD << "║  " << COLOR_BOLD_WHITE << "CRAFTING BENCH" << COLOR_BOLD_CYAN
              << "                              " << COLOR_BOLD_WHITE << "INVENTORY"
              << COLOR_BOLD_CYAN;
    RB();
    std::cout << PAD << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << COLOR_RESET;

    // Print recipe list with inventory on the right
    for (int i = 0; i < NUM_RECIPES; i++) {
        const CraftingRecipe& r = RECIPES[i];
        bool isSelected = (i == selectedIndex);
        bool visible = isRecipeVisible(state, r);
        bool craftable = canCraft(state, r);

        std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_RESET;
        if (isSelected) {
            std::cout << COLOR_BOLD_YELLOW << "\xe2\x96\xb6 " << COLOR_RESET;  // ▶
        } else {
            std::cout << "  ";
        }

        // Recipe name
        if (!visible) {
            std::cout << COLOR_DIM << "  [Locked]          " << COLOR_RESET;
        } else if (craftable) {
            std::cout << COLOR_GREEN << std::left << std::setw(20) << r.displayName << COLOR_RESET;
        } else {
            std::cout << COLOR_YELLOW << std::left << std::setw(20) << r.displayName << COLOR_RESET;
        }

        // Inventory column (gap then item)
        std::cout << "          ";
        switch (i) {
            case 0: std::cout << "Wood:    " << COLOR_WOOD    << std::setw(3) << state.player.inventory.wood    << COLOR_RESET; break;
            case 1: std::cout << "Stone:   " << COLOR_STONE   << std::setw(3) << state.player.inventory.stone   << COLOR_RESET; break;
            case 2: std::cout << "Iron:    " << COLOR_IRON    << std::setw(3) << state.player.inventory.iron    << COLOR_RESET; break;
            case 3: std::cout << "Gold:    " << COLOR_GOLD_ORE<< std::setw(3) << state.player.inventory.gold    << COLOR_RESET; break;
            case 4: std::cout << "Diamond: " << COLOR_DIAMOND << std::setw(3) << state.player.inventory.diamond << COLOR_RESET; break;
            case 6: std::cout << "Pickaxe: " << getMaterialColor(state.player.equipment.pickaxe)
                              << std::left << std::setw(7) << getMaterialName(state.player.equipment.pickaxe) << COLOR_RESET; break;
            case 7: std::cout << "Armor:   " << getMaterialColor(state.player.equipment.armor)
                              << std::left << std::setw(7) << getMaterialName(state.player.equipment.armor) << COLOR_RESET; break;
            case 8: std::cout << "HP: " << COLOR_HEALTH << state.player.health << "/" << state.player.maxHealth << COLOR_RESET; break;
            default: break;
        }

        RB();
    }

    // Details box
    std::cout << PAD << COLOR_BOLD_CYAN
              << "╠══════════════════════════════════════════════════════════════════════════════╣\n"
              << COLOR_RESET;

    const CraftingRecipe& selected = RECIPES[selectedIndex];
    bool visible = isRecipeVisible(state, selected);
    bool craftable = canCraft(state, selected);

    // Selected line
    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_RESET
              << COLOR_BOLD_WHITE << "Selected: " << COLOR_RESET;
    if (visible)
        std::cout << getTierColor(selected.tier) << selected.displayName << COLOR_RESET;
    else
        std::cout << COLOR_DIM << "??? (Complete previous tier)" << COLOR_RESET;
    RB();

    // Cost line
    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_RESET << "Cost: ";
    if (!visible) {
        std::cout << COLOR_DIM << "Unknown" << COLOR_RESET;
    } else {
        bool first = true;
        auto printCost = [&](int cost, const char* name, const char* color) {
            if (cost > 0) {
                if (!first) std::cout << ", ";
                std::cout << color << cost << " " << name << COLOR_RESET;
                first = false;
            }
        };
        printCost(selected.woodCost,    "Wood",    COLOR_WOOD);
        printCost(selected.stoneCost,   "Stone",   COLOR_STONE);
        printCost(selected.ironCost,    "Iron",    COLOR_IRON);
        printCost(selected.goldCost,    "Gold",    COLOR_GOLD_ORE);
        printCost(selected.diamondCost, "Diamond", COLOR_DIAMOND);
    }
    RB();

    // Description line
    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_RESET
              << COLOR_DIM << "Desc: " << COLOR_RESET;
    if (visible)
        std::cout << selected.description;
    else
        std::cout << "Complete previous tier to unlock";
    RB();

    // Status line
    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_RESET << "Status: ";
    if (craftable)
        std::cout << COLOR_GREEN << "\xe2\x9c\x93 Ready to craft!" << COLOR_RESET;  // ✓
    else if (!visible)
        std::cout << COLOR_DIM << "Locked — craft previous tier first" << COLOR_RESET;
    else
        std::cout << COLOR_WARNING << "\xe2\x9c\x97 " << getCraftingError(state, selected) << COLOR_RESET;  // ✗
    RB();

    // Footer
    std::cout << PAD << COLOR_BOLD_CYAN
              << "╠══════════════════════════════════════════════════════════════════════════════╣\n"
              << PAD << "║  " << COLOR_WHITE << "[W/S] Navigate  [ENTER] Craft  [Q] Back"
              << COLOR_BOLD_CYAN;
    RB();
    std::cout << PAD << COLOR_BOLD_CYAN
              << "╚══════════════════════════════════════════════════════════════════════════════╝\n"
              << COLOR_RESET;
}

// Main crafting menu loop
void openCraftingMenu(GameState& state) {
    int selected = 0;

    // Find first visible recipe to start selection there
    for (int i = 0; i < NUM_RECIPES; i++) {
        if (isRecipeVisible(state, RECIPES[i])) {
            selected = i;
            break;
        }
    }

    while (true) {
        renderCraftingUI(state, selected);

        char input = getch();

        switch (input) {
            case 'w':
            case 'W':
                // Move up to previous visible recipe, wrap around
                do {
                    selected = (selected - 1 + NUM_RECIPES) % NUM_RECIPES;
                } while (!isRecipeVisible(state, RECIPES[selected]) && selected > 0);
                break;

            case 's':
            case 'S':
                // Move down to next visible recipe, stop at last visible
                do {
                    selected = (selected + 1) % NUM_RECIPES;
                } while (!isRecipeVisible(state, RECIPES[selected]) && selected < NUM_RECIPES - 1);
                // If we wrapped to invisible ones, go back to first visible
                if (!isRecipeVisible(state, RECIPES[selected])) {
                    for (int i = 0; i < NUM_RECIPES; i++) {
                        if (isRecipeVisible(state, RECIPES[i])) {
                            selected = i;
                            break;
                        }
                    }
                }
                break;

            case '\n':
            case '\r':
            case ' ':
                // Attempt craft
                if (canCraft(state, RECIPES[selected])) {
                    bool minigameTriggered = performCrafting(state, RECIPES[selected]);
                    if (minigameTriggered) {
                        // Minigame was triggered, exit menu immediately
                        // Main loop will detect state.minigameActive and state.phase
                        return;
                    }
                    // Small delay to show success message
                    usleep(500000);  // 500ms
                } else {
                    // Error flash
                    std::cout << "\a";  // Bell
                }
                break;

            case 'q':
            case 'Q':
                return;  // Exit to game
        }
    }
}

// Detailed inventory display (full screen)
void showInventory(const GameState& state) {
    int termCols, termRows;
    getTermSize(termCols, termRows);
    const int BW = 60;
    int leftPad = (termCols - BW - 2) / 2;
    if (leftPad < 0) leftPad = 0;
    int rightCol = leftPad + BW + 2;
    std::string PAD(leftPad, ' ');
    auto RB = [&]() {
        std::cout << "\033[" << rightCol << "G" << COLOR_BOLD_CYAN << "║\n" << COLOR_RESET;
    };
    auto border = [&]() {
        std::cout << COLOR_BOLD_CYAN;
        for (int i = 0; i < BW; i++) std::cout << "═";
    };

    bool fullDiamond = (state.player.equipment.armor == MATERIAL_DIAMOND &&
                        state.player.equipment.pickaxe == MATERIAL_DIAMOND);
    int totalLines = fullDiamond ? 18 : 15;
    int topPad = (termRows - totalLines) / 2;
    if (topPad < 0) topPad = 0;

    clearScreen();
    if (topPad > 0) std::cout << std::string(topPad, '\n');

    std::cout << PAD << COLOR_BOLD_CYAN << "╔"; border(); std::cout << "╗\n" << COLOR_RESET;
    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_BOLD_YELLOW << "INVENTORY" << COLOR_RESET;
    RB();
    std::cout << PAD << COLOR_BOLD_CYAN << "╠"; border(); std::cout << "╣\n" << COLOR_RESET;

    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_BOLD_WHITE << "RESOURCES" << COLOR_RESET;
    RB();

    auto printResource = [&](const char* color, const char* name, int count) {
        std::cout << PAD << COLOR_BOLD_CYAN << "║   " << COLOR_RESET;
        std::cout << color << std::left << std::setw(12) << name << COLOR_RESET
                  << ": " << std::setw(4) << count;
        RB();
    };
    printResource(COLOR_WOOD, "Wood", state.player.inventory.wood);
    printResource(COLOR_STONE, "Stone", state.player.inventory.stone);
    printResource(COLOR_IRON, "Iron", state.player.inventory.iron);
    printResource(COLOR_GOLD_ORE, "Gold", state.player.inventory.gold);
    printResource(COLOR_DIAMOND, "Diamond", state.player.inventory.diamond);

    std::cout << PAD << COLOR_BOLD_CYAN << "╠"; border(); std::cout << "╣\n" << COLOR_RESET;

    std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_BOLD_WHITE << "EQUIPMENT" << COLOR_RESET;
    RB();
    std::cout << PAD << COLOR_BOLD_CYAN << "║   " << COLOR_RESET
              << "Pickaxe: " << getMaterialColor(state.player.equipment.pickaxe)
              << getMaterialName(state.player.equipment.pickaxe) << COLOR_RESET;
    RB();
    std::cout << PAD << COLOR_BOLD_CYAN << "║   " << COLOR_RESET
              << "Armor:   " << getMaterialColor(state.player.equipment.armor)
              << getMaterialName(state.player.equipment.armor) << COLOR_RESET;
    RB();
    std::cout << PAD << COLOR_BOLD_CYAN << "║   " << COLOR_RESET
              << "Health:  " << COLOR_HEALTH << state.player.health << "/"
              << state.player.maxHealth << COLOR_RESET;
    RB();

    if (fullDiamond) {
        std::cout << PAD << COLOR_BOLD_CYAN << "╠"; border(); std::cout << "╣\n" << COLOR_RESET;
        std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_BOLD_YELLOW
                  << "** FULL DIAMOND ACHIEVED! **" << COLOR_RESET;
        RB();
        std::cout << PAD << COLOR_BOLD_CYAN << "║ " << COLOR_DIM
                  << "The elder awaits at the dragon carcass..." << COLOR_RESET;
        RB();
    }

    std::cout << PAD << COLOR_BOLD_CYAN << "╚"; border(); std::cout << "╝\n" << COLOR_RESET;

    std::string PP = hpad(30);
    std::cout << COLOR_DIM << "\n" << PP << "Press any key to continue..." << COLOR_RESET;
    getch();
}