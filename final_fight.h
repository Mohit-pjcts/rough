// =============================================================================
// final_fight.h
// TermiCraft — Final Boss Fight Module Header
//
// Declares all constants, structs, and functions for the Space Invaders-style
// dragon boss fight. Fully integrated with the team's shared types.h, fileio.h,
// menu.h, and colors.h. Does NOT redefine Difficulty, MaterialTier, HighScore,
// or HIGHSCORE_FILE — all are taken from the team's existing headers.
//
// Arena:     100 x 35 terminal columns/rows
// Rendering: ANSI escape codes via colors.h (no ncurses, no extra install)
// Input:     Non-blocking via fcntl(O_NONBLOCK) + read() from <fcntl.h>/<unistd.h>
// Score:     Added to state.score in real time, every increment multiplied by
//            state.settings.scoreMultiplier (from types.h DifficultySettings)
//
// Author:       Sohan
// Dependencies: types.h, fileio.h, menu.h, colors.h
//               <fcntl.h>, <unistd.h> — standard on Linux, no extra install
// =============================================================================

#ifndef FINAL_FIGHT_H
#define FINAL_FIGHT_H

#include "types.h"   // GameState, Difficulty, MaterialTier — use team's definitions

// -----------------------------------------------------------------------------
// Arena dimensions
// -----------------------------------------------------------------------------
#define FF_ARENA_WIDTH     100   // total terminal columns
#define FF_ARENA_HEIGHT     35   // total terminal rows

// -----------------------------------------------------------------------------
// Dragon art dimensions
// -----------------------------------------------------------------------------
#define FF_DRAGON_ROWS      12   // lines in the ASCII dragon block
#define FF_DRAGON_COLS      35   // width of the widest dragon line

// -----------------------------------------------------------------------------
// Dragon movement bounds
// Left edge min = 2  (clears left border at col 0)
// Left edge max = 63 (right edge = 63+35 = 98, border at col 99)
// -----------------------------------------------------------------------------
#define FF_DRAGON_MIN_X      2
#define FF_DRAGON_MAX_X     63

// -----------------------------------------------------------------------------
// Arena row layout  (rows 0-33 active; row 34 is a blank safety buffer)
// -----------------------------------------------------------------------------
#define FF_HUD_ROW           1   // score + dragon HP bar
#define FF_HUD_SEP_ROW       2   // separator under HUD
#define FF_DRAGON_TOP_ROW    3   // first row of dragon art
#define FF_PLAYER_ROW       29   // row the player sprite sits on
#define FF_HP_ROW           30   // player HP bar display
#define FF_CTRL_SEP_ROW     31   // separator above controls
#define FF_CTRL_ROW         32   // controls hint line
#define FF_BOT_ROW          33   // bottom border

// -----------------------------------------------------------------------------
// Projectile pool sizes
// -----------------------------------------------------------------------------
#define FF_MAX_FIREBALLS    12   // maximum simultaneous fireballs
#define FF_MAX_ARROWS       10   // maximum simultaneous arrows (rapid fire)

// -----------------------------------------------------------------------------
// Boss fight base HP (local to this fight, independent of state.player.health)
// Formula: calcFightHP(difficulty, armor) = baseHP + armorBonus
// Base HP per difficulty:
// -----------------------------------------------------------------------------
// Base fight HP (no armor = this, it should hurt)
#define FF_BASE_HP_EASY     20
#define FF_BASE_HP_NORMAL   15
#define FF_BASE_HP_HARD     10

// Armor HP bonuses — armor should feel necessary
#define FF_HP_BONUS_STONE   15
#define FF_HP_BONUS_IRON    30
#define FF_HP_BONUS_GOLD    50
#define FF_HP_BONUS_DIAMOND 80

// Arrow damage per hit — better armor = bigger payoff
#define FF_DMG_DEFAULT   1
#define FF_DMG_IRON      4
#define FF_DMG_GOLD      7
#define FF_DMG_DIAMOND  12

// -----------------------------------------------------------------------------
// Phase thresholds (% of dragon.maxHp remaining when phase transition triggers)
// -----------------------------------------------------------------------------
#define FF_PHASE2_PCT    66   // Phase 2 triggers at or below 66 %
#define FF_PHASE3_PCT    33   // Phase 3 triggers at or below 33 %

#define FF_PHASE1         1
#define FF_PHASE2         2
#define FF_PHASE3         3

// -----------------------------------------------------------------------------
// Score per arrow hit per phase — RAW value before scoreMultiplier
// state.settings.scoreMultiplier (1.0 / 1.5 / 2.0) is applied on every add
// -----------------------------------------------------------------------------
#define FF_SCORE_HIT_P1  10
#define FF_SCORE_HIT_P2  20
#define FF_SCORE_HIT_P3  30
#define FF_SCORE_KILL   500   // bonus when dragon HP reaches 0

// Fireball horizontal spread offsets per phase
#define FF_SPREAD_P2      3   // phase 2: two fireballs at centre ± 3 cols
#define FF_SPREAD_P3      5   // phase 3: outer fireballs at centre ± 5 cols

// Game tick duration in microseconds (50 ms = 20 ticks per second)
#define FF_TICK_US     50000

// -----------------------------------------------------------------------------
// Structs  (HighScore, Difficulty, MaterialTier already in types.h)
// -----------------------------------------------------------------------------

/*
 * BossConfig
 *
 * Difficulty-specific parameters for the boss fight.
 * Built once by initBossConfig() at fight start, never modified during the fight.
 */
struct BossConfig {
    int  dragonHp;        // dragon starting HP
    int  fireballDmg;     // HP removed from player per fireball hit
    int  fireRateTicks;   // ticks between fireball volleys
    int  dragonSpeed;     // base columns dragon moves per tick
    bool hasEnrage;       // hard: speed + rate boost when HP drops to 50%
    int  spread3;         // phase-3 fireball outer spread (wider on hard)
};

/*
 * Dragon
 *
 * Live state of the dragon enemy throughout the fight.
 * The full ASCII art block moves as a single unit: only x changes per tick.
 * y is always FF_DRAGON_TOP_ROW.
 */
struct Dragon {
    int  x, y;
    int  hp, maxHp;
    int  speed, direction, phase;
    bool enraged;       // hard: triggers at 50% HP — speed + fire rate boost
    int  announceTicks; // ticks left to flash a phase-change / enrage banner
};

/*
 * Fireball
 *
 * One downward projectile fired by the dragon.
 * The fireballs[] pool recycles inactive slots without reallocating.
 */
struct Fireball {
    int  x;          // current column
    int  y;          // current row
    bool active;     // true = in flight and should be rendered + checked
    int  dx;         // horizontal drift per tick: 0 (straight), -1 or +1 (spread)
};

/*
 * Arrow
 *
 * One upward projectile fired by the player.
 * Rapid fire: multiple arrows can be active simultaneously (up to FF_MAX_ARROWS).
 */
struct Arrow {
    int  x;          // current column
    int  y;          // current row
    bool active;     // true = in flight
};

// -----------------------------------------------------------------------------
// Public function declarations
// -----------------------------------------------------------------------------

/*
 * runBossFight
 *
 * Main entry point. Called from main.cpp when the player enters the dragon cave.
 * Manages: intro screen, game loop, score breakdown, name entry, high score
 * save via fileio's addHighScore(), and sets state.phase before returning.
 *
 * Score is added to state.score in real time throughout the fight.
 * Every addition is: state.score += (int)(rawPoints * state.settings.scoreMultiplier)
 *
 * Inputs:  state — full GameState (reads: score, difficulty, settings, armor)
 *                                 (writes: score, phase, dragonDefeated)
 * Outputs: true  = dragon defeated → state.phase set to PHASE_VICTORY
 *          false = player died     → state.phase set to PHASE_GAMEOVER
 *          Partial scores (death mid-fight) are still saved to the leaderboard.
 */
bool runBossFight(GameState& state);

/*
 * initBossConfig
 *
 * Builds the difficulty-specific BossConfig from a Difficulty enum value.
 *
 * Inputs:  diff — DIFF_EASY, DIFF_NORMAL, or DIFF_HARD (from types.h)
 * Outputs: BossConfig fully populated for that difficulty
 *
 * Tick timing reference (FF_TICK_US = 50 ms per tick):
 *   Easy:   fireRateTicks=40  ≈ 2.0 s between volleys, dragonSpeed=1
 *   Normal: fireRateTicks=24  ≈ 1.2 s,                 dragonSpeed=2
 *   Hard:   fireRateTicks=14  ≈ 0.7 s,                 dragonSpeed=3
 */
BossConfig initBossConfig(Difficulty diff);

/*
 * calcFightHP
 *
 * Computes the player's starting HP for the boss fight.
 * Formula: baseHP (from difficulty) + flat armorBonus.
 * No cap — armor always adds on top regardless of difficulty.
 *
 * Inputs:
 *   diff  — DIFF_EASY / DIFF_NORMAL / DIFF_HARD (base: 40 / 30 / 20)
 *   armor — MaterialTier from types.h (MATERIAL_NONE through MATERIAL_DIAMOND)
 * Outputs: int — total starting HP
 *
 * Examples:
 *   calcFightHP(DIFF_EASY,   MATERIAL_DIAMOND) → 40 + 30 = 70
 *   calcFightHP(DIFF_NORMAL, MATERIAL_GOLD)    → 30 + 22 = 52
 *   calcFightHP(DIFF_HARD,   MATERIAL_IRON)    → 20 + 15 = 35
 *   calcFightHP(DIFF_HARD,   MATERIAL_NONE)    → 20 +  0 = 20
 */
int calcFightHP(Difficulty diff, MaterialTier armor);

/*
 * calcArmorDamage
 *
 * Returns damage dealt to the dragon per arrow hit, based on armor tier.
 *
 * Inputs:  armor — MaterialTier (MATERIAL_NONE through MATERIAL_DIAMOND)
 * Outputs: int — damage per hit
 *            NONE / WOOD / STONE → 1
 *            IRON                → 3
 *            GOLD                → 5
 *            DIAMOND             → 8
 */
int calcArmorDamage(MaterialTier armor);

#endif // FINAL_FIGHT_H
