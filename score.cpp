// =============================================================================
// score.cpp
// TermiCraft — Centralised Score Calculation Module Implementation
//
// Implements addScore() and saveFinalScore().
//
// addScore():
//   Single multiplier application point — rawPoints * scoreMultiplier,
//   cast to int (fractional points are truncated, not rounded, consistent
//   with the rest of the codebase's existing cast pattern).
//   Guards against negative rawPoints so a buggy caller cannot reduce score.
//
// saveFinalScore():
//   Prompts for player name via menu.h's getPlayerName().
//   Builds a HighScore struct from current GameState.
//   Delegates to fileio's addHighScore() for top-10 sorting + disk write.
//   Returns after printing a one-line confirmation to the player.
//
// File I/O:
//   This module does NOT open or write any files directly.
//   All persistence is handled by fileio.cpp (addHighScore, loadHighScores).
//
// Author:       Sohan
// Dependencies: score.h, fileio.h, menu.h, colors.h, <ctime>, <iostream>
// =============================================================================

#include "score.h"
#include "fileio.h"    // addHighScore(), getTopHighScore()
#include "colors.h"    // COLOR_SUCCESS, COLOR_RESET, COLOR_SCORE

#include <iostream>
#include <ctime>       // time() for HighScore.timestamp

using std::cout;
using std::string;

// =============================================================================
// addScore
// =============================================================================

/*
 * addScore
 * Increments state.score by rawPoints multiplied by the current difficulty
 * scoreMultiplier. This is the only place in the codebase that modifies
 * state.score during gameplay — both player.cpp and final_fight.cpp call here.
 *
 * Inputs:
 *   state     - GameState (reads settings.scoreMultiplier, writes score)
 *   rawPoints - base points before multiplier, must be >= 0
 *               Negative values are silently ignored (defensive guard)
 * Outputs: none
 *
 * scoreMultiplier values by difficulty (from types.h getDifficultySettings):
 *   Easy:   1.0f  → rawPoints x 1.0
 *   Normal: 1.5f  → rawPoints x 1.5  (fractional remainder truncated)
 *   Hard:   2.0f  → rawPoints x 2.0
 *
 * Examples:
 *   addScore(state, 1);   // wood, Easy   → +1
 *   addScore(state, 5);   // diamond, Hard → +10
 *   addScore(state, 10);  // P1 boss hit, Normal → +15
 *   addScore(state, 500); // kill bonus, Hard → +1000
 */
void addScore(GameState& state, int rawPoints) {
    // Guard: never subtract from score due to a negative rawPoints call
    if (rawPoints < 0) return;

    // Apply difficulty multiplier — cast truncates fractional remainder
    state.score += static_cast<int>(rawPoints * state.settings.scoreMultiplier);
}

// =============================================================================
// saveFinalScore
// =============================================================================

/*
 * saveFinalScore
 * End-of-run save flow: name prompt → HighScore build → fileio save.
 * Called exactly once per run, from final_fight.cpp's runBossFight()
 * after the score breakdown screen.
 *
 * Inputs:
 *   state          - GameState (reads score, difficulty)
 *   defeatedDragon - true = dragon was killed, false = player died or quit
 * Outputs: none
 *
 * Delegation chain:
 *   addHighScore()   [fileio.h] — sorts top 10 and writes to disk
 *   getTopHighScore()[fileio.h] — reads previous best to check for new record
 *                                 (called BEFORE addHighScore to avoid race)
 */
void saveFinalScore(GameState& state, bool defeatedDragon) {
    // Player name already captured at game start via main.cpp's getPlayerName().
    // state.player.name is used directly — no second prompt needed.
    string name = state.player.name;
    if (name.empty()) name = "Anonymous";
    if ((int)name.size() > 16) name = name.substr(0, 16);

    // --- Build HighScore struct (defined in types.h) ---
    HighScore hs;
    hs.playerName     = name;
    hs.score          = state.score;        // full run total (mining + boss)
    hs.difficulty     = state.difficulty;   // Difficulty enum from types.h
    hs.timestamp      = std::time(nullptr); // Unix timestamp for display/sorting
    hs.defeatedDragon = defeatedDragon;     // false on death = partial score

    // --- Snapshot top score BEFORE saving ---
    // Must be called before addHighScore() so we compare against the previous
    // best, not the score we are about to write. Calling it after would mean
    // our new score is already in the file, making isNewBest almost always
    // true for any top-10 entry — a false positive.
    HighScore prevTop = getTopHighScore();

    // --- Delegate to fileio for top-10 sort + disk write ---
    // addHighScore() in fileio.cpp:
    //   1. Loads current top-10 from termicraft_highscores.dat
    //   2. Appends hs, re-sorts descending by score
    //   3. Trims to top 10, writes back to file
    //   4. Returns true if the new score made the cut
    bool madeList = addHighScore(hs);

    // --- Confirmation message ---
    // isNewBest uses prevTop (pre-save snapshot) — strictly greater than,
    // not >= , so tied scores do not trigger the "new record" banner.
    bool isNewBest = (state.score > prevTop.score);

    if (isNewBest) {
        cout << COLOR_SUCCESS
             << "\n  ** NEW ALL-TIME HIGH SCORE! **\n"
             << COLOR_RESET;
    } else if (madeList) {
        cout << COLOR_SUCCESS
             << "\n  Score saved — you made the top 10!\n"
             << COLOR_RESET;
    } else {
        cout << "\n  Score recorded.\n";
    }
}
