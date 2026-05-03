// =============================================================================
// player.cpp
// TermiCraft — Player & Crafting System Module Implementation
//
// Implements the player controller and crafting system for TermiCraft. Handles
// real-time keyboard input for movement (WASD), mining (SPACE), and crafting (C),
// with full collision detection against the procedurally generated world grid.
//
// The module enforces the strict progression system: Wood → Stone → Iron → Gold
// -> Diamond, where each tier upgrade after Stone requires completing a minigame
// (Wordle for Iron, Minesweeper for Gold) to simulate the "rite of passage"
// described in the game lore.
//
// Physics includes simple gravity simulation causing the player to fall when
// standing over air blocks. The camera system tracks player movement with
// clamping to world boundaries.
//
// Author:       Koki
// Dependencies: player.h, colors.h, menu.h, <algorithm>, <cstdlib>, <ctime>,
//               <iostream>
// =============================================================================

#include "player.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cmath>

#include "colors.h"
#include "crafting.h"
#include "menu.h"
#include "score.h"    // addScore() — centralised score increment with multiplier

// Initialize player at the dragon carcass spawn point
void initPlayer(GameState& state, const std::string& playerName) {
    // Reset player stats
    state.player.name = playerName;
    state.player.health = state.settings.playerHealth;
    state.player.maxHealth = state.settings.playerHealth;
    state.player.alive = true;
    state.player.facingX = 0;
    state.player.facingY = 1;  // Face down by default

    // Reset inventory and equipment
    state.player.inventory = Inventory();
    state.player.equipment = Equipment();

    // Reset mining state
    state.miningPending = false;
    state.minigameDamage = state.settings.minigameDamage;

    // Spawn at the forest biome surface (col 10, just above ground level).
    // Walk down from sky until we hit the first non-sky, non-air block, then
    // stand one row above it so the player is on the surface grass.
    {
        int spawnX = 10;
        int spawnY = SURFACE_LEVEL - 1; // default fallback
        for (int y = 0; y < state.worldHeight - 1; y++) {
            BlockType t = state.world[y][spawnX].type;
            if (t != BLOCK_SKY && t != BLOCK_AIR) {
                spawnY = y - 1; // stand one row above first solid block
                if (spawnY < 0) spawnY = 0;
                break;
            }
        }
        state.player.pos = Position(spawnX, spawnY);
    }

    // Initialize camera to center player
    updateCamera(state);

    state.lastMessage = "Welcome, " + playerName + "! Mine resources to survive.";
}

// Check if block type is solid (impassable)
bool isSolidBlock(BlockType type) {
    switch (type) {
        case BLOCK_AIR:
        case BLOCK_SKY:
        case BLOCK_LEAVES:
            return false;
        default:
            return true;
    }
}

// Get required tool tier to mine a block
MaterialTier getRequiredTierForBlock(BlockType block) {
    switch (block) {
        case BLOCK_DIRT:
        case BLOCK_GRASS:
        case BLOCK_WOOD:
        case BLOCK_LEAVES:
            return MATERIAL_NONE;  // Can mine with hands

        case BLOCK_STONE:
        case BLOCK_COAL:
            return MATERIAL_WOOD;

        case BLOCK_IRON:
            return MATERIAL_STONE;

        case BLOCK_GOLD:
            return MATERIAL_IRON;

        case BLOCK_DIAMOND:
            return MATERIAL_GOLD;

        case BLOCK_BEDROCK:
            return MATERIAL_DIAMOND;

        case BLOCK_DRAGON_CAVE:
            return MATERIAL_NONE;  // Anyone brave enough can try their luck

        default:
            return MATERIAL_NONE;
    }
}

// Check if player can mine a specific block
bool canPlayerMine(const GameState& state, BlockType block) {
    MaterialTier required = getRequiredTierForBlock(block);
    return state.player.equipment.pickaxe >= required;
}

// Attempt to move player
bool movePlayer(GameState& state, int dx, int dy) {
    int newX = state.player.pos.x + dx;
    int newY = state.player.pos.y + dy;

    // Bounds checking
    if (newX < 0 || newX >= state.worldWidth || newY < 0 || newY >= state.worldHeight) {
        return false;
    }

    Block& targetBlock = state.world[newY][newX];

    // Collision detection
    if (isSolidBlock(targetBlock.type) && !targetBlock.mined) {
        return false;  // Can't walk through solid unmined blocks
    }

    // Update position
    state.player.pos.x = newX;
    state.player.pos.y = newY;

    // Update facing direction
    if (dx != 0 || dy != 0) {
        state.player.facingX = dx;
        state.player.facingY = dy;
    }

    // Update camera to follow
    updateCamera(state);

    return true;
}

// Update camera to center on player
void updateCamera(GameState& state) {
    // Calculate ideal camera position (top-left corner) to center player
    int idealX = state.player.pos.x - state.viewportWidth / 2;
    int idealY = state.player.pos.y - state.viewportHeight / 2;

    // Clamp to world bounds
    idealX = std::max(0, std::min(idealX, state.worldWidth - state.viewportWidth));
    idealY = std::max(0, std::min(idealY, state.worldHeight - state.viewportHeight));

    state.camera.x = idealX;
    state.camera.y = idealY;
}

// Select random minigame using MINIGAME_COUNT constant
void selectRandomMinigame(GameState& state) {
    int r = rand() % MINIGAME_COUNT;

    if (r == 0) {
        state.currentMinigame = MINIGAME_WORDLE;
    } else if (r == 1) {
        state.currentMinigame = MINIGAME_MINESWEEPER;
    } else if (r == 2) {
        state.currentMinigame = MINIGAME_TWENTYFOUR;
    } else {
        state.currentMinigame = MINIGAME_SUDOKU;
    }
}

// Initiate mining attempt - validates target then triggers minigame
void initiateMining(GameState& state) {
    // Calculate target position based on facing direction
    int targetX = state.player.pos.x + state.player.facingX;
    int targetY = state.player.pos.y + state.player.facingY;

    // Bounds check
    if (targetX < 0 || targetX >= state.worldWidth || targetY < 0 || targetY >= state.worldHeight) {
        state.lastMessage = "Cannot mine here!";
        return;
    }

    Block& block = state.world[targetY][targetX];
    BlockType targetType = block.type;

    // Check if already mined/air/sky
    if (block.mined || block.type == BLOCK_AIR || block.type == BLOCK_SKY) {
        state.lastMessage = "Nothing to mine here.";
        return;
    }

    // Check tool requirement
    if (!canPlayerMine(state, targetType)) {
        MaterialTier required = getRequiredTierForBlock(targetType);
        std::string toolName = getMaterialName(required) + " Pickaxe";
        if (required == MATERIAL_NONE) toolName = "hands";

        state.lastMessage = "Need " + toolName + " to mine this!";
        showStatusMessage(state.lastMessage, true);
        return;
    }

    // Store mining attempt details
    state.pendingMinePos  = Position(targetX, targetY);
    state.pendingMineType = targetType;
    state.miningPending   = true;

    // Minigame trigger probability: random float in [10%, 16%], strictly bounded
    // by the ratio of distinct minable ore types to number of minigames.
    // Formula: chance = rand[0.10, 0.16] * (MINIGAME_COUNT / NUM_DISTINCT_ORES)
    // — rare enough to not be annoying, still meaningful for high-value ores.
    static const float NUM_DISTINCT_ORES = 6.0f; // wood stone coal iron gold diamond
    float base = 0.10f + 0.06f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
    float triggerChance = base * (static_cast<float>(MINIGAME_COUNT) / NUM_DISTINCT_ORES);
    float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    if (roll >= triggerChance && targetType != BLOCK_DRAGON_CAVE) {
        // No minigame this time — instant mine (dragon cave always requires a challenge)
        resolveMiningAttempt(state, true);
        state.miningPending   = false;
        state.minigameActive  = false;
        state.currentMinigame = MINIGAME_NONE;
        return;
    }

    // Select and setup minigame
    selectRandomMinigame(state);
    state.minigameActive = true;
    state.phase = PHASE_MINIGAME;

    std::string gameName =
        (state.currentMinigame == MINIGAME_WORDLE) ? "Wordle" :
        (state.currentMinigame == MINIGAME_MINESWEEPER) ? "Minesweeper" :
        (state.currentMinigame == MINIGAME_TWENTYFOUR) ? "24 Game" :
        (state.currentMinigame == MINIGAME_SUDOKU) ? "Sudoku" :
        "Wordle";

    state.lastMessage = "Mining challenge: " + gameName + "!";
}

// Resolve mining attempt after minigame completes
void resolveMiningAttempt(GameState& state, bool minigameWon) {
    if (!state.miningPending) {
        return;  // No pending mining operation
    }

    if (minigameWon) {
        // Success - grant resources
        BlockType minedType = state.pendingMineType;
        int points = 0;

        switch (minedType) {
            case BLOCK_WOOD:
                state.player.inventory.wood++;
                points = 1;
                break;
            case BLOCK_STONE:
                state.player.inventory.stone++;
                points = 2;
                break;
            case BLOCK_COAL:
                state.player.inventory.coal++;
                points = 2;
                break;
            case BLOCK_IRON:
                state.player.inventory.iron++;
                points = 3;
                break;
            case BLOCK_GOLD:
                state.player.inventory.gold++;
                points = 4;
                break;
            case BLOCK_DIAMOND:
                state.player.inventory.diamond++;
                points = 5;
                break;
            default:
                break;
        }

        // Mark block as mined
        int tx = state.pendingMinePos.x;
        int ty = state.pendingMinePos.y;
        state.world[ty][tx].mined = true;
        state.world[ty][tx].type = BLOCK_AIR;

        // Update score and stats.
        // addScore() (score.h) is the single place that applies scoreMultiplier —
        // do NOT increment state.score directly anywhere else in this file.
        if (points > 0) {
            addScore(state, points);
            state.oresMined++;
        }

        state.lastMessage = "Success! Mined " + std::string(getBlockChar(minedType)) +
                            " (+" + std::to_string(points) + " pts)";
    } else {
        // Failure - take damage
        int damage = state.settings.minigameDamage;
        damagePlayer(state, damage);

        if (state.player.alive) {
            state.lastMessage = "Failed! Took " + std::to_string(damage) + " damage! Try again.";
        } else {
            state.lastMessage = "Mining accident... you perished!";
        }
    }

    // Reset mining state
    state.miningPending = false;
    state.minigameActive = false;
    state.currentMinigame = MINIGAME_NONE;
}

// Handle single keypress
void handleInput(GameState& state, char input) {
    bool moved = false;

    switch (input) {
        case 'w':
        case 'W':
            state.player.facingX = 0; state.player.facingY = -1;
            moved = movePlayer(state, 0, -1);
            break;
        case 's':
        case 'S':
            state.player.facingX = 0; state.player.facingY = 1;
            moved = movePlayer(state, 0, 1);
            break;
        case 'a':
        case 'A':
            state.player.facingX = -1; state.player.facingY = 0;
            moved = movePlayer(state, -1, 0);
            break;
        case 'd':
        case 'D':
            state.player.facingX = 1; state.player.facingY = 0;
            moved = movePlayer(state, 1, 0);
            break;

        case ' ':  // Mine - now initiates minigame challenge
            initiateMining(state);
            break;

        case 'c':
        case 'C': {  // Crafting
            openCraftingMenu(state);
            break;
        }

        case 'i':
        case 'I':  // Inventory
            showInventory(state);
            break;

        case 'p':
        case 'P':  // Pause/Save
            state.phase = PHASE_MENU;
            break;

        case 'q':
        case 'Q':  // Quit to menu
            state.phase = PHASE_MENU;
            break;
    }

    if (moved) {
        // Clear message on move unless it was important
        if (state.lastMessage.find("Failed") == std::string::npos &&
            state.lastMessage.find("Success") == std::string::npos) {
            state.lastMessage = "";
        }
    }
}

// Damage player (now used for minigame failure)
void damagePlayer(GameState& state, int amount) {
    state.player.health -= amount;
    if (state.player.health <= 0) {
        state.player.health = 0;
        state.player.alive = false;
        state.gameOver = true;
        state.phase = PHASE_GAMEOVER;
    }
}

// Heal player
void healPlayer(GameState& state, int amount) {
    state.player.health = std::min(state.player.health + amount, state.player.maxHealth);
}

// Check if player has resources to craft target tier
bool hasResourcesForTier(const GameState& state, MaterialTier targetTier) {
    const Inventory& inv = state.player.inventory;

    switch (targetTier) {
        case MATERIAL_WOOD:
            return inv.wood >= 1;  // 1 wood for wooden tools

        case MATERIAL_STONE:
            // Need wooden pickaxe first, then 2 stone
            return state.player.equipment.pickaxe >= MATERIAL_WOOD && inv.stone >= 2;

        case MATERIAL_IRON:
            // Need stone tools, then 3 iron
            return state.player.equipment.pickaxe >= MATERIAL_STONE && inv.iron >= 3;

        case MATERIAL_GOLD:
            // Need iron tools, then 4 gold
            return state.player.equipment.pickaxe >= MATERIAL_IRON && inv.gold >= 4;

        case MATERIAL_DIAMOND:
            // Need gold tools, then 5 diamond
            return state.player.equipment.pickaxe >= MATERIAL_GOLD && inv.diamond >= 5;

        default:
            return false;
    }
}

// Consume resources for crafting
void consumeResourcesForTier(GameState& state, MaterialTier tier) {
    Inventory& inv = state.player.inventory;

    switch (tier) {
        case MATERIAL_WOOD:
            inv.wood -= 1;
            break;
        case MATERIAL_STONE:
            inv.stone -= 2;
            break;
        case MATERIAL_IRON:
            inv.iron -= 3;
            break;
        case MATERIAL_GOLD:
            inv.gold -= 4;
            break;
        case MATERIAL_DIAMOND:
            inv.diamond -= 5;
            break;
        default:
            break;
    }
}

// Check if crafting should trigger minigame (Iron, Gold, Diamond)
bool checkCraftingProgression(GameState& /*state*/) {
    // Minigames only trigger during mining, not crafting
    return false;
}

// Called by main.cpp after minigame victory to confirm the upgrade
void confirmUpgrade(GameState& state) {
    if (state.pendingUpgrade != MATERIAL_NONE) {
        state.minigameActive = false;
        state.lastMessage = "Upgrade to " + getMaterialName(state.pendingUpgrade) + " confirmed!";

        // Check if full diamond armor achieved
        if (state.pendingUpgrade == MATERIAL_DIAMOND &&
            state.player.equipment.armor == MATERIAL_DIAMOND) {
            state.lastMessage = "Full Diamond Armor achieved! Find the portal beneath the carcass!";
        }

        state.pendingUpgrade = MATERIAL_NONE;
    }
}

// Get color based on armor tier for player rendering
const char* getPlayerArmorColor(const GameState& state) {
    return getMaterialColor(state.player.equipment.armor);
}

// Roll to spawn an enemy at the mined position
void trySpawnEnemy(GameState& state, Position minedPos) {
    if (state.settings.enemySpawnChance == 0) return;
    if (rand() % 100 >= state.settings.enemySpawnChance) return;

    Enemy e;
    e.pos = minedPos;
    int scaledHp = 20 * state.settings.enemyHealthMult / 100;
    e.health = e.maxHealth = std::max(5, scaledHp);
    e.damage = (state.difficulty == DIFF_HARD) ? 15 : 10;
    e.alive  = true;
    e.symbol = 'B';
    e.name   = "Cave Bug";
    state.enemies.push_back(e);
}
