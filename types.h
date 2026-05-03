/*
 * types.h
 *
 * Shared data structures for TermiCraft — everyone includes this.
 * If you're adding new structs or enums, put them here so the whole team can use them.
 *
 * DON'T modify existing structs without telling the group chat first.
 */

#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <ctime>
#include <string>

// ----- CONSTANTS -----

// World size stuff
const int WORLD_WIDTH   = 200;
const int WORLD_HEIGHT  = 80;
const int SURFACE_LEVEL = 8;    // ground starts here, sky above
const int STONE_LEVEL   = 12;   // stone layer begins
const int GOLD_LEVEL    = 30;   // gold layer begins
const int DIAMOND_LEVEL = 50;   // diamond layer begins
const int DEEP_LEVEL    = 25;   // rare ores spawn below this
const int MINIGAME_COUNT = 4;   // Total number of available minigames

// Block types - used in the world grid
enum BlockType {
    BLOCK_AIR = 0,
    BLOCK_SKY = 1,
    BLOCK_GRASS = 2,
    BLOCK_DIRT = 3,
    BLOCK_STONE = 4,
    BLOCK_COAL = 5,
    BLOCK_IRON = 6,
    BLOCK_GOLD = 7,
    BLOCK_DIAMOND = 8,
    BLOCK_WOOD = 9,
    BLOCK_LEAVES = 10,
    BLOCK_BEDROCK = 11,
    BLOCK_DRAGON_CAVE = 12
};

// Tool and armor tiers - higher = better
enum MaterialTier {
    MATERIAL_NONE    = 0,
    MATERIAL_WOOD    = 1,
    MATERIAL_STONE   = 2,
    MATERIAL_IRON    = 3,
    MATERIAL_GOLD    = 4,
    MATERIAL_DIAMOND = 5
};

// Difficulty settings
enum Difficulty {
    DIFF_EASY   = 0,
    DIFF_NORMAL = 1,
    DIFF_HARD   = 2
};

// Minigame types
enum MinigameType {
    MINIGAME_NONE        = 0,
    MINIGAME_WORDLE      = 1,
    MINIGAME_MINESWEEPER = 2,
    MINIGAME_TWENTYFOUR  = 3,
    MINIGAME_SUDOKU      = 4
};

// Random event types
enum EventType {
    EVENT_NONE     = 0,
    EVENT_RED_ZONE = 1,   // danger zone: player takes damage per step inside
    EVENT_ORE_SURGE = 2   // bonus: ore drops doubled for a short time
};

struct RandomEvent {
    EventType type;
    bool      active;
    int       x, y;         // world-space centre
    int       radius;       // taxicab radius of the affected area
    int       ticksLeft;    // frames until the event expires
    int       damage;       // HP cost per burn tick inside the zone
    int       damageTimer;  // frames since last damage tick
    int       damagePeriod; // frames between damage ticks (slow burn)
    int       alertTicks;   // frames to show the big alert banner

    RandomEvent() : type(EVENT_NONE), active(false), x(0), y(0),
                    radius(0), ticksLeft(0), damage(0),
                    damageTimer(0), damagePeriod(40), alertTicks(0) {}
};

// What phase the game is in
enum GamePhase {
    PHASE_MENU     = 0,
    PHASE_PLAYING  = 1,
    PHASE_MINIGAME = 2,
    PHASE_BOSS     = 3,
    PHASE_GAMEOVER = 4,
    PHASE_VICTORY  = 5
};

// ----- DATA STRUCTURES -----

struct Position {
    int x;
    int y;

    Position() : x(0), y(0) {}
    Position(int _x, int _y) : x(_x), y(_y) {}
};

struct Block {
    BlockType type;
    bool mined;
    bool visible;

    Block() : type(BLOCK_AIR), mined(false), visible(false) {}
};

struct Inventory {
    int wood;
    int stone;
    int coal;
    int iron;
    int gold;
    int diamond;

    Inventory() : wood(0), stone(0), coal(0), iron(0), gold(0), diamond(0) {}

    int total() const {
        return wood + stone + coal + iron + gold + diamond;
    }
};

struct Equipment {
    MaterialTier pickaxe;
    MaterialTier armor;

    Equipment() : pickaxe(MATERIAL_NONE), armor(MATERIAL_NONE) {}
};

struct Player {
    std::string name;
    Position pos;
    int health;
    int maxHealth;
    Inventory inventory;
    Equipment equipment;
    bool alive;
    int facingX;
    int facingY;

    Player() : name("Player"), health(100), maxHealth(100), alive(true), facingX(0), facingY(1) {}
};

struct Enemy {
    std::string name;
    Position pos;
    int health;
    int maxHealth;
    int damage;
    bool alive;
    char symbol;

    Enemy() : name("Bug"), health(20), maxHealth(20), damage(5), alive(true), symbol('B') {}
};

struct CraftingRecipe {
    MaterialTier tier;
    bool isArmor;
    int woodCost;
    int stoneCost;
    int ironCost;
    int goldCost;
    int diamondCost;
    MaterialTier requiredPickaxe;
    MaterialTier requiredArmor;
    const char* displayName;
    const char* description;
};

// Recipe table defined in crafting.cpp
extern const CraftingRecipe RECIPES[];
extern const int NUM_RECIPES;

struct DifficultySettings {
    std::string name;
    int playerHealth;
    int enemyHealthMult;
    int oreSpawnRate;
    int enemySpawnChance;
    float scoreMultiplier;
    int wordleWordLength;
    int minesweeperSize;
    int twentyFourAttempts;
    int twentyFourTimeLimit;
    int minigameDamage;

    DifficultySettings() : name("Normal"), playerHealth(100),
        enemyHealthMult(100), oreSpawnRate(100), enemySpawnChance(15),
        scoreMultiplier(1.5f), wordleWordLength(5), minesweeperSize(8),
        twentyFourAttempts(3), twentyFourTimeLimit(90), minigameDamage(20) {}
};

struct HighScore {
    std::string playerName;
    int score;
    Difficulty difficulty;
    std::time_t timestamp;
    bool defeatedDragon;

    HighScore() : playerName("---"), score(0), difficulty(DIFF_EASY),
        timestamp(0), defeatedDragon(false) {}
};

struct GameState {
    GamePhase phase;
    Difficulty difficulty;
    DifficultySettings settings;

    Block** world;
    int worldWidth;
    int worldHeight;

    Player player;
    std::vector<Enemy> enemies;

    bool dragonCaveFound;
    Position dragonCavePos;
    bool dragonDefeated;

    int score;
    int oresMined;
    int enemiesKilled;

    MinigameType currentMinigame;
    bool minigameActive;
    MaterialTier pendingUpgrade;

    Position camera;
    int viewportWidth;
    int viewportHeight;

    bool gameOver;
    bool victory;
    std::string lastMessage;
    unsigned int seed;

    Position pendingMinePos;
    BlockType pendingMineType;
    bool miningPending;
    int minigameDamage;

    RandomEvent activeEvent;
    int         eventCooldown;   // frames until a new event can trigger
    bool        oreSurgeActive;  // shortcut flag for mining code

    GameState() : phase(PHASE_MENU), difficulty(DIFF_NORMAL),
        world(nullptr), worldWidth(WORLD_WIDTH), worldHeight(WORLD_HEIGHT),
        dragonCaveFound(false), dragonDefeated(false),
        score(0), oresMined(0), enemiesKilled(0),
        currentMinigame(MINIGAME_NONE), minigameActive(false),
        pendingUpgrade(MATERIAL_NONE),
        viewportWidth(80), viewportHeight(24),
        gameOver(false), victory(false), seed(0),
        pendingMineType(BLOCK_AIR), miningPending(false), minigameDamage(20),
        eventCooldown(0), oreSurgeActive(false) {}

    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;
};

// ----- HELPER FUNCTIONS -----

inline const char* getBlockChar(BlockType type) {
    switch (type) {
        case BLOCK_AIR:         return " ";
        case BLOCK_SKY:         return " ";
        case BLOCK_GRASS:       return "\xe2\x89\xa1"; // ≡
        case BLOCK_DIRT:        return "\xe2\x96\x92"; // ▒
        case BLOCK_STONE:       return "\xe2\x96\x93"; // ▓
        case BLOCK_COAL:        return "\xe2\x97\x8f"; // ●
        case BLOCK_IRON:        return "\xe2\x97\x86"; // ◆
        case BLOCK_GOLD:        return "\xe2\x98\x85"; // ★
        case BLOCK_DIAMOND:     return "\xe2\x99\xa6"; // ♦
        case BLOCK_WOOD:        return "\xe2\x94\x82"; // │
        case BLOCK_LEAVES:      return "\xe2\x99\xa3"; // ♣
        case BLOCK_BEDROCK:     return "\xe2\x96\xac"; // ▬
        case BLOCK_DRAGON_CAVE: return "\xe2\x8a\x97"; // ⊗
        default:                return "?";
    }
}

inline std::string getMaterialName(MaterialTier tier) {
    switch (tier) {
        case MATERIAL_NONE:    return "None";
        case MATERIAL_WOOD:    return "Wood";
        case MATERIAL_STONE:   return "Stone";
        case MATERIAL_IRON:    return "Iron";
        case MATERIAL_GOLD:    return "Gold";
        case MATERIAL_DIAMOND: return "Diamond";
        default:               return "Unknown";
    }
}

inline int getBlockScore(BlockType type) {
    switch (type) {
        case BLOCK_WOOD:    return 1;
        case BLOCK_STONE:   return 2;
        case BLOCK_COAL:    return 2;
        case BLOCK_IRON:    return 3;
        case BLOCK_GOLD:    return 4;
        case BLOCK_DIAMOND: return 5;
        default:            return 0;
    }
}

inline DifficultySettings getDifficultySettings(Difficulty diff) {
    DifficultySettings s;
    switch (diff) {
        case DIFF_EASY:
            s.name             = "Easy";
            s.playerHealth     = 150;
            s.enemyHealthMult  = 75;
            s.oreSpawnRate     = 130;
            s.enemySpawnChance = 10;
            s.scoreMultiplier  = 1.0f;
            s.wordleWordLength   = 4;
            s.minesweeperSize    = 6;
            s.twentyFourAttempts = 5;
            s.twentyFourTimeLimit = 180;
            s.minigameDamage     = 10;
            break;
        case DIFF_NORMAL:
            s.name             = "Normal";
            s.playerHealth     = 100;
            s.enemyHealthMult  = 100;
            s.oreSpawnRate     = 100;
            s.enemySpawnChance = 15;
            s.scoreMultiplier  = 1.5f;
            s.wordleWordLength   = 5;
            s.minesweeperSize    = 8;
            s.twentyFourAttempts = 3;
            s.twentyFourTimeLimit = 90;
            s.minigameDamage     = 20;
            break;
        case DIFF_HARD:
            s.name             = "Hard";
            s.playerHealth     = 75;
            s.enemyHealthMult  = 150;
            s.oreSpawnRate     = 70;
            s.enemySpawnChance = 25;
            s.scoreMultiplier  = 2.0f;
            s.wordleWordLength   = 6;
            s.minesweeperSize    = 10;
            s.twentyFourAttempts = 1;
            s.twentyFourTimeLimit = 30;
            s.minigameDamage     = 30;
            break;
    }
    return s;
}

#endif
