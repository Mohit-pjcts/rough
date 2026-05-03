// =============================================================================
// world_gen.cpp
// TermiCraft — World Generation Module Implementation
//
// Generates the 200×80 world with three biomes:
//   Forest (cols 0–49)   — dramatic hilly terrain, dense trees, ores below
//   Cave   (cols 50–139) — sparse surface, massive winding cave with branches
//   Light Cave (cols 140–199) — underground cavern with dragon portal
//
// All randomness uses a deterministic hash seeded by state.seed so that
// the same seed always produces the same world.
//
// Author:       Mohit
// Dependencies: world_gen.h, types.h
// =============================================================================

#include "world_gen.h"
#include <algorithm>
#include <cstring>
#include <cmath>   // for sqrtf / normal-distribution approximation
#include <vector>

// ---------------------------------------------------------------------------
// Deterministic hash — replaces rand() everywhere
// ---------------------------------------------------------------------------
static unsigned int worldHash(unsigned int seed, int col, int row) {
    unsigned int h = seed ^ (unsigned int)(col * 2971 + row * 31337);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return h;
}

static int hashPercent(unsigned int seed, int col, int row) {
    return (int)(worldHash(seed, col, row) % 100);
}

// Second independent hash channel so two checks on the same cell don't collide
static int hashPercent2(unsigned int seed, int col, int row) {
    return (int)(worldHash(seed ^ 0xDEADBEEF, col + 7777, row + 3333) % 100);
}

// ---------------------------------------------------------------------------
// Normal-distribution trunk height (mean=5, clamped to [3,8])
// Uses the sum of 6 uniform [0,1] samples ≈ normal (central limit theorem).
// Returns an int in [3, 8].
// ---------------------------------------------------------------------------
static int normalTrunkHeight(unsigned int seed, int col) {
    // Sum 6 values in [0,5] → range [0,30], mean ~15, sd ~3.5
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        sum += (int)(worldHash(seed, col * 7 + i, 9999) % 6);
    }
    // Map [0,30] → mean=5, clamp [3,8]:
    //   sum=15 → 5,  sum=0 → ~2,  sum=30 → ~8
    int h = 3 + (sum * 5) / 30;   // linear map: 0→3, 30→8
    if (h < 3) h = 3;
    if (h > 8) h = 8;
    return h;
}

// ---------------------------------------------------------------------------
// initWorld — allocate the 2-D Block array
// ---------------------------------------------------------------------------
void initWorld(GameState& state) {
    state.worldWidth  = (state.viewportWidth > 0) ? state.viewportWidth : WORLD_WIDTH;
    state.worldHeight = WORLD_HEIGHT;

    state.world = new Block*[state.worldHeight];
    for (int y = 0; y < state.worldHeight; y++) {
        state.world[y] = new Block[state.worldWidth];
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ---------------------------------------------------------------------------
// Smooth noise helpers
// ---------------------------------------------------------------------------

static float smoothNoiseWG(unsigned int seed, float x) {
    int ix = (int)x;
    if (x < 0.0f) ix--;
    float frac = x - (float)ix;
    float t = frac * frac * (3.0f - 2.0f * frac); // smoothstep
    float a = (float)(worldHash(seed, ix,     0) % 1000) / 1000.0f;
    float b = (float)(worldHash(seed, ix + 1, 0) % 1000) / 1000.0f;
    return a + t * (b - a);
}

// Three-octave noise — same as demo.cpp's terrainHeight()
static float terrainHeightWG(unsigned int seed, int x) {
    float h = 0.0f;
    h += smoothNoiseWG(seed,       x * 0.02f) * 6.0f;
    h += smoothNoiseWG(seed + 100, x * 0.05f) * 3.0f;
    h += smoothNoiseWG(seed + 200, x * 0.10f) * 1.5f;
    return h; // range roughly 0–10.5
}

// ---------------------------------------------------------------------------
// buildSurfaceHeightmap
// Forest:    dramatic hills — multiplier 1.2, clamp [-6, +4] from SURFACE_LEVEL
// Cave:      moderate hills — multiplier 0.5, clamp [-2, +2]
// LightCave: gentle         — multiplier 0.3, clamp [-1, +2]
// ---------------------------------------------------------------------------
static void buildSurfaceHeightmap(unsigned int seed, int* heightmap, int worldW,
                                  int forestEnd, int caveEnd) {
    for (int x = 0; x < worldW; x++) {
        float baseH = terrainHeightWG(seed, x);

        if (x <= forestEnd) {
            // Forest — dramatic rolling hills matching the demo screenshot
            // baseH is [0..10.5]; multiply by 1.2 gives [0..12.6], offset by -5
            int offset = (int)(baseH * 1.2f) - 5;
            heightmap[x] = clamp(SURFACE_LEVEL + offset, SURFACE_LEVEL - 6, SURFACE_LEVEL + 4);
        } else if (x <= caveEnd) {
            // Cave biome — moderate hills
            int offset = (int)(baseH * 0.5f) - 1;
            heightmap[x] = clamp(SURFACE_LEVEL + offset, SURFACE_LEVEL - 2, SURFACE_LEVEL + 2);
        } else {
            // Light cave surface — gentle
            int offset = (int)(baseH * 0.3f);
            heightmap[x] = clamp(SURFACE_LEVEL + offset, SURFACE_LEVEL - 1, SURFACE_LEVEL + 2);
        }
    }
}

// ---------------------------------------------------------------------------
// placeTree
//
// Trunk: cells from (grassRow-1) up to (grassRow-trunkHeight) are BLOCK_WOOD.
// Canopy: diamond pattern abs(dx)+abs(dy)<=3, dx in [-2,2], dy in [-2,0],
//         anchored at topY = grassRow - trunkHeight (the TOP trunk cell).
//         This puts leaves AROUND the top trunk cell — so the trunk pokes
//         through the canopy center, giving the look:
//
//           ***        (dy=-2, abs(dx)<=1)
//          *****       (dy=-1, abs(dx)<=2)
//          **|**       (dy= 0, |=trunk, *=leaves)
//
// Spacing: at least 3 cols from lastTreeCol.
// ---------------------------------------------------------------------------
static bool placeTree(Block** world, int worldW, int worldH,
                      int col, int grassRow, int trunkHeight, int& lastTreeCol) {
    // Enforce spacing
    if (col - lastTreeCol < 3) return false;

    int trunkTop  = grassRow - trunkHeight;  // topmost trunk row (= top of canopy centre)
    int trunkBase = grassRow - 1;            // bottommost trunk row (one above grass)

    // Bounds: canopy reaches 2 above trunkTop and ±2 cols
    if (trunkTop - 2 < 0) return false;
    if (col - 2 < 0 || col + 2 >= worldW) return false;

    // Verify trunk column is clear (SKY or AIR only)
    for (int y = trunkTop; y <= trunkBase; y++) {
        if (y < 0 || y >= worldH) return false;
        BlockType t = world[y][col].type;
        if (t != BLOCK_AIR && t != BLOCK_SKY) return false;
    }

    // Place trunk
    for (int y = trunkTop; y <= trunkBase; y++) {
        world[y][col].type = BLOCK_WOOD;
    }

    // Canopy: diamond centred on trunkTop (the top trunk cell)
    //   dy ranges -2..0  (canopy sits above and level with trunk top)
    //   abs(dx)+abs(dy) <= 3
    for (int dy = -2; dy <= 0; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (abs(dx) + abs(dy) > 3) continue;
            int lx = col + dx;
            int ly = trunkTop + dy;
            if (lx < 0 || lx >= worldW || ly < 0 || ly >= worldH) continue;
            // Only overwrite sky/air — don't clobber other trunks
            BlockType t = world[ly][lx].type;
            if (t == BLOCK_AIR || t == BLOCK_SKY) {
                world[ly][lx].type = BLOCK_LEAVES;
            }
        }
    }

    lastTreeCol = col;
    return true;
}

// ---------------------------------------------------------------------------
// pickOre — determine ore type for a stone cell
// ---------------------------------------------------------------------------
static BlockType pickOre(unsigned int seed, int col, int row) {
    if (row >= 50) {
        int r = hashPercent(seed, col, row);
        if (r < 2) return BLOCK_DIAMOND;
    }
    if (row >= 30) {
        int r = hashPercent2(seed, col, row);
        if (r < 3) return BLOCK_GOLD;
    }
    if (row >= 12) {
        int r = hashPercent(seed, col + 5000, row + 5000);
        if (r < 15) return BLOCK_IRON;
    }
    return BLOCK_STONE;
}

// ---------------------------------------------------------------------------
// generateWorld — main generation routine
// ---------------------------------------------------------------------------
void generateWorld(GameState& state) {
    unsigned int seed = state.seed;
    int W = state.worldWidth;
    int H = state.worldHeight;
    Block** world = state.world;

    if (W <= 0 || H <= 0 || world == nullptr) return;

    int forestEnd = clamp((W * 25) / 100 - 1, 0, W - 1);
    int caveEnd = clamp(forestEnd + (W * 45) / 100, forestEnd + 1, W - 1);
    int caveStart = forestEnd + 1;
    int lightStart = caveEnd + 1;
    int lightEnd = W - 1;

    // --------------------------------------------------
    // STEP 0: Default everything to BLOCK_STONE, hidden
    // --------------------------------------------------
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            world[y][x].type    = BLOCK_STONE;
            world[y][x].mined   = false;
            world[y][x].visible = false;
        }
    }

    // --------------------------------------------------
    // STEP 1: Build surface heightmap
    // --------------------------------------------------
    std::vector<int> heightmap(W, SURFACE_LEVEL);
    buildSurfaceHeightmap(seed, heightmap.data(), W, forestEnd, caveEnd);

    // --------------------------------------------------
    // STEP 2: Fill sky, grass, dirt, stone+ores for all columns
    // --------------------------------------------------
    for (int x = 0; x < W; x++) {
        int grassRow = heightmap[x];

        // Sky above grass
        for (int y = 0; y < grassRow; y++) {
            world[y][x].type = BLOCK_SKY;
        }

        // Grass
        if (grassRow >= 0 && grassRow < H)
            world[grassRow][x].type = BLOCK_GRASS;

        // Dirt below grass (variable depth 3-5, only above stone level)
        int dirtDepth = 3 + (int)(worldHash(seed, x, 8910) % 3);
        for (int y = grassRow + 1; y < grassRow + 1 + dirtDepth && y < H; y++) {
            if (y < STONE_LEVEL)
                world[y][x].type = BLOCK_DIRT;
        }

        // Stone + ores from STONE_LEVEL down (bedrock at H-1)
        for (int y = STONE_LEVEL; y < H - 1; y++) {
            if (world[y][x].type == BLOCK_DIRT) continue; // already set
            world[y][x].type = pickOre(seed, x, y);
        }

        // Bedrock
        world[H - 1][x].type = BLOCK_BEDROCK;
    }

    // --------------------------------------------------
    // STEP 3: Surface visibility — reveal sky + terrain rows
    //
    // Mark the sky and every surface row visible so the player sees the
    // full hillscape without fog immediately on spawn.
    // We scan each column and mark everything from y=0 down to
    // (grassRow + 2) visible, giving a thin slice of exposed dirt.
    // --------------------------------------------------
    for (int x = 0; x < W; x++) {
        int grassRow = heightmap[x];
        int revealTo = clamp(grassRow + 2, 0, H - 1);
        for (int y = 0; y <= revealTo; y++) {
            world[y][x].visible = true;
        }
    }

    // --------------------------------------------------
    // STEP 4: Trees
    //
    // Forest (0-49):    ~20% chance, trunk height normal(mean=5) [3,8]
    // Cave   (50-139):  ~10% chance, trunk height normal(mean=4) [3,6]
    // LightCave(140-199):~5% chance, trunk height normal(mean=3) [2,4]
    // --------------------------------------------------
    {
        int lastTree = -10;

        // Forest trees
        // Guarantee at least one tree near the left edge (cols 2-6) if possible.
        bool leftTreePlaced = false;
        int leftStart = 2;
        int leftEnd = std::min(forestEnd, 6);
        for (int x = leftStart; x <= leftEnd && !leftTreePlaced; x++) {
            int grassRow = heightmap[x];
            int trunkH = normalTrunkHeight(seed, x);
            if (placeTree(world, W, H, x, grassRow, trunkH, lastTree)) {
                leftTreePlaced = true;
            }
        }

        for (int x = leftStart; x <= forestEnd; x++) {
            int grassRow = heightmap[x];
            int chance = hashPercent(seed, x, 8888);
            if (chance < 20) {
                int trunkH = normalTrunkHeight(seed, x);
                placeTree(world, W, H, x, grassRow, trunkH, lastTree);
            }
        }

        // Cave biome surface trees
        lastTree = forestEnd - 2;
        for (int x = caveStart; x <= caveEnd; x++) {
            int grassRow = heightmap[x];
            int chance = hashPercent(seed, x, 8888);
            if (chance < 10) {
                // Shorter mean (4) for cave biome
                int raw = 0;
                for (int i = 0; i < 6; i++)
                    raw += (int)(worldHash(seed, x * 7 + i, 9999) % 5);
                int trunkH = clamp(2 + (raw * 4) / 30, 2, 6);
                placeTree(world, W, H, x, grassRow, trunkH, lastTree);
            }
        }

        // Light Cave surface trees
        lastTree = caveEnd - 2;
        for (int x = lightStart; x < W; x++) {
            int grassRow = heightmap[x];
            int chance = hashPercent(seed, x, 7000);
            if (chance < 5) {
                int raw = 0;
                for (int i = 0; i < 4; i++)
                    raw += (int)(worldHash(seed, x * 5 + i, 7001) % 3);
                int trunkH = clamp(2 + (raw * 2) / 12, 2, 4);
                placeTree(world, W, H, x, grassRow, trunkH, lastTree);
            }
        }
    }

    // --------------------------------------------------
    // STEP 5: Main cave tunnel (Cave biome, cols 50–139)
    // --------------------------------------------------
    std::vector<int> cavePath(W, 0);

    {
        int centerY = 20;
        int targetY = 45;

        for (int x = caveStart; x <= caveEnd; x++) {
            cavePath[x] = centerY;

            if ((x - caveStart) % 4 == 0 && x < caveEnd) {
                int r = hashPercent(seed, x, 7777) % 5;
                int delta = r - 2;

                float progress = (float)(x - caveStart) / (float)(caveEnd - caveStart + 1);
                int ideal = 20 + (int)(progress * (targetY - 20));
                if (centerY < ideal - 2) delta = clamp(delta, 0, 2);
                if (centerY > ideal + 2) delta = clamp(delta, -2, 0);

                centerY = clamp(centerY + delta, 15, 60);
            }
        }

        // Carve main tunnel: 6 wide, 5 tall
        for (int x = caveStart; x <= caveEnd; x++) {
            int cy = cavePath[x];
            int halfW = 3;
            int halfH = 2;

            if (x <= caveStart + 3) { halfH = 4; halfW = 4; } // wide entrance

            for (int dy = -halfH; dy <= halfH; dy++) {
                for (int dx = -halfW; dx <= halfW; dx++) {
                    int wx = x + dx;
                    int wy = cy + dy;
                    if (wx < 0 || wx >= W || wy < 0 || wy >= H - 1) continue;
                    if (world[wy][wx].type == BLOCK_BEDROCK) continue;
                    world[wy][wx].type = BLOCK_AIR;
                }
            }
        }

        // Cave floor grass
        for (int x = caveStart; x <= caveEnd; x++) {
            int cy = cavePath[x];
            int floorY = cy + 3;
            if (floorY >= 0 && floorY < H - 1) {
                if (world[floorY][x].type == BLOCK_STONE ||
                    world[floorY][x].type == BLOCK_IRON  ||
                    world[floorY][x].type == BLOCK_GOLD) {
                    if (floorY - 1 >= 0 && world[floorY - 1][x].type == BLOCK_AIR) {
                        world[floorY][x].type = BLOCK_GRASS;
                    }
                }
            }
        }
    }

    // --------------------------------------------------
    // STEP 6: Branch tunnels (4–6 branches off main cave)
    // --------------------------------------------------
    {
        int numBranches = 4 + (int)(worldHash(seed, 12345, 67890) % 3);

        for (int b = 0; b < numBranches; b++) {
            int caveSpan = std::max(1, caveEnd - caveStart - 4);
            int startX = caveStart + 3 + (int)(worldHash(seed, b * 137, 4444) % caveSpan);
            if (startX > caveEnd - 3) startX = caveEnd - 3;
            int startY = cavePath[startX];

            int dirSeed = (int)(worldHash(seed, b * 271, 5555) % 4);
            int bdx, bdy;
            switch (dirSeed) {
                case 0: bdx =  1; bdy = -1; break;
                case 1: bdx =  1; bdy =  1; break;
                case 2: bdx = -1; bdy = -1; break;
                default: bdx = -1; bdy =  1; break;
            }

            int branchLen = 20 + (int)(worldHash(seed, b * 311, 6666) % 11);
            int bx = startX, by = startY;

            for (int step = 0; step < branchLen; step++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int wx = bx + dx, wy = by + dy;
                        if (wx < caveStart || wx > caveEnd) continue;
                        if (wy < 1 || wy >= H - 1) continue;
                        if (world[wy][wx].type == BLOCK_BEDROCK) continue;
                        world[wy][wx].type = BLOCK_AIR;
                    }
                }
                bx += bdx;
                by += (step % 2 == 0) ? bdy : 0;
                if (bx < caveStart || bx > caveEnd || by < 13 || by >= H - 2) break;
            }
        }
    }

    // --------------------------------------------------
    // STEP 7: Light Cave (cols 140–199)
    // --------------------------------------------------

    // Hollow out rows 20-65
    std::vector<int> lightCaveFloor(W, 0);
    if (lightStart < W) {
        // Hollow out rows 20-65
        for (int y = 20; y <= 65; y++) {
            for (int x = lightStart; x < W; x++) {
                if (world[y][x].type == BLOCK_BEDROCK) continue;
                world[y][x].type = BLOCK_AIR;
            }
        }

        // Hilly floor
        {
            int floorY = 50;
            for (int x = lightStart; x < W; x++) {
                lightCaveFloor[x] = floorY;
                if ((x - lightStart) % 4 == 0 && x < W - 1) {
                    int r = hashPercent(seed, x, 8000) % 3;
                    int delta = r - 1;
                    floorY = clamp(floorY + delta, 46, 57);
                }
            }

            for (int x = lightStart; x < W; x++) {
                int gy = lightCaveFloor[x];
                world[gy][x].type = BLOCK_GRASS;
                for (int d = 1; d <= 3; d++) {
                    if (gy + d < 65) world[gy + d][x].type = BLOCK_DIRT;
                }
                for (int y = gy + 4; y <= 65; y++) {
                    world[y][x].type = BLOCK_STONE;
                }
            }
        }

        // Light Cave floor trees (5%)
        {
            int lastTree = caveEnd - 2;
            for (int x = lightStart; x < W; x++) {
                int gy = lightCaveFloor[x];
                int chance = hashPercent(seed, x, 7000);
                if (chance < 5) {
                    int raw = 0;
                    for (int i = 0; i < 4; i++)
                        raw += (int)(worldHash(seed, x * 5 + i, 7001) % 3);
                    int trunkH = clamp(2 + (raw * 2) / 12, 2, 4);
                    placeTree(world, W, H, x, gy, trunkH, lastTree);
                }
            }
        }

        // Dragon portal centered in the light cave
        {
            int portalCol = clamp(lightStart + (lightEnd - lightStart) / 2, lightStart, lightEnd);
            int floorRow  = lightCaveFloor[portalCol];
            int portalTop = floorRow - 4;

            for (int dy = 0; dy < 4; dy++) {
                for (int dx = 0; dx < 3; dx++) {
                    int px2 = portalCol + dx;
                    int py2 = portalTop + dy + 1;
                    if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                        world[py2][px2].type = BLOCK_DRAGON_CAVE;
                }
            }

            if (portalCol - 1 >= 0) {
                world[floorRow][portalCol - 1].type = BLOCK_AIR;
                if (floorRow - 1 >= 0) world[floorRow - 1][portalCol - 1].type = BLOCK_AIR;
                if (floorRow - 2 >= 0) world[floorRow - 2][portalCol - 1].type = BLOCK_AIR;
            }
        }
    }

    // --------------------------------------------------
    // STEP 8: Connect main cave → Light Cave (cols 134-148)
    // --------------------------------------------------
    if (lightStart < W) {
        int exitY  = cavePath[caveEnd];
        int entryY = 40;

        int bridgeStart = clamp(caveEnd - 2, 0, W - 1);
        int bridgeEnd = clamp(lightStart + 3, 0, W - 1);
        int span = std::max(1, bridgeEnd - bridgeStart);

        for (int x = bridgeStart; x <= bridgeEnd; x++) {
            float t = (float)(x - bridgeStart) / (float)span;
            int cy = exitY + (int)(t * (entryY - exitY));
            for (int dy = -4; dy <= 4; dy++) {
                int wy = cy + dy;
                if (wy < 1 || wy >= H - 1) continue;
                if (x < 0 || x >= W) continue;
                if (world[wy][x].type == BLOCK_BEDROCK) continue;
                world[wy][x].type = BLOCK_AIR;
            }
        }
    }

    // --------------------------------------------------
    // STEP 9: Forest cave entrance slope (cols 45-53)
    // --------------------------------------------------
    {
        int surfaceY   = heightmap[forestEnd];
        int caveEntryY = cavePath[caveStart];

        int slopeStart = clamp(caveStart - 5, 0, W - 1);
        int slopeEnd = clamp(caveStart + 3, 0, W - 1);
        int span = std::max(1, slopeEnd - slopeStart);

        for (int x = slopeStart; x <= slopeEnd; x++) {
            float t = (float)(x - slopeStart) / (float)span;
            int cy = surfaceY + (int)(t * (caveEntryY - surfaceY));
            int halfH = 2 + (int)(t * 2);
            for (int dy = -halfH; dy <= halfH; dy++) {
                int wy = cy + dy;
                if (wy < 1 || wy >= H - 1) continue;
                if (x < 0 || x >= W) continue;
                if (world[wy][x].type == BLOCK_BEDROCK) continue;
                if (wy < surfaceY - 1 && x < caveStart - 2) continue;
                world[wy][x].type = BLOCK_AIR;
            }
        }
    }
}
