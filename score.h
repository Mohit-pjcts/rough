// =============================================================================
// score.h
// TermiCraft — Centralised Score Calculation Module Header
//
// This is the single source of truth for all score increments in the game.
// Both the mining phase (player.cpp) and the boss fight (final_fight.cpp)
// call addScore() instead of applying the multiplier inline. This guarantees
// the difficulty multiplier is applied identically everywhere and prevents
// score calculation from drifting across modules.
//
// File I/O delegation:
//   saveFinalScore() builds the HighScore struct and delegates to
//   fileio's addHighScore() for the actual disk write. score.cpp does NOT
//   open or write any files directly — fileio.cpp owns all file operations.
//
// Name entry delegation:
//   saveFinalScore() uses state.player.name directly — the player already
//   entered their name at game start via main.cpp's getPlayerName() call.
//   No second name prompt is shown.
//
// Author:       Sohan
// Dependencies: types.h, fileio.h
// =============================================================================

#ifndef SCORE_H
#define SCORE_H

#include "types.h"   // GameState, Difficulty, HighScore — team definitions

// -----------------------------------------------------------------------------
// addScore
//
// THE one function used to increment state.score anywhere in the codebase.
// Applies state.settings.scoreMultiplier to rawPoints before adding, ensuring
// the difficulty multiplier is always in effect.
//
// Called by:
//   player.cpp  — resolveMiningAttempt(), once per successful block mined
//   final_fight.cpp — on every arrow hit and on the dragon kill bonus
//
// Inputs:
//   state     - full GameState (reads scoreMultiplier, writes score)
//   rawPoints - base points before multiplier (e.g. 1 for wood, 5 for diamond,
//               10/20/30 for boss hit phases, 500 for dragon kill bonus)
// Outputs: none
//          Side effect: state.score increased by (int)(rawPoints * scoreMultiplier)
//
// Example:
//   addScore(state, 5);   // diamond mined on Hard (mult 2.0) → +10 to state.score
//   addScore(state, 10);  // Phase 1 boss hit on Normal (mult 1.5) → +15
// -----------------------------------------------------------------------------
void addScore(GameState& state, int rawPoints);

// -----------------------------------------------------------------------------
// saveFinalScore
//
// Called once at the end of a run — either victory or death in the boss fight.
// Uses state.player.name directly (set at game start by main.cpp — no second
// prompt). Builds a HighScore struct and passes it to addHighScore() (fileio.h).
// addHighScore() handles top-10 sorting and the actual file write.
//
// Partial scores from a death mid-fight are eligible — this function is
// always called regardless of win or loss, so no score is ever discarded.
//
// Inputs:
//   state          - full GameState (reads score, difficulty, player.name)
//   defeatedDragon - true if the dragon was killed (sets HighScore.defeatedDragon)
//                    false if player died or quit mid-fight
// Outputs: none
//          Side effect: may update termicraft_highscores.dat via fileio
// -----------------------------------------------------------------------------
void saveFinalScore(GameState& state, bool defeatedDragon);

#endif // SCORE_H
