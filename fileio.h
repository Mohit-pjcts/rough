/*
 * fileio.h
 * 
 * Save/load system and high score handling and writing game state to disk and reading it back.
 */

#ifndef FILEIO_H
#define FILEIO_H

#include "types.h"
#include <string>
#include <vector>

// Default file names - these get created in the same folder as the executable
constexpr const char* SAVE_FILE = "termicraft_save.dat";
constexpr const char* HIGHSCORE_FILE = "termicraft_highscores.dat";

// ----- SAVE/LOAD GAME -----

// Save current game to file. Returns true if it worked.
bool saveGame(const GameState& state, const std::string& filename = SAVE_FILE);

// Load game from file into state. Returns true if it worked.
bool loadGame(GameState& state, const std::string& filename = SAVE_FILE);

// Check if there's a save file we can load
bool saveFileExists(const std::string& filename = SAVE_FILE);

// Delete the save file (for starting fresh)
bool deleteSaveFile(const std::string& filename = SAVE_FILE);

// ----- HIGH SCORES -----

// Get all saved high scores, sorted highest first
std::vector<HighScore> loadHighScores();

// Save the high score list to disk
bool saveHighScores(const std::vector<HighScore>& scores);

// Add a new score to the list (keeps top 10 only)
// Returns true if the score made it into the top 10
bool addHighScore(const HighScore& newScore);

// Get just the #1 high score (for showing on main menu)
HighScore getTopHighScore();

// Check if a score would make it into top 10
bool isHighScore(int score);

// ----- WORLD SERIALIZATION -----
// These are used internally but exposed in case someone needs them

// Convert world to a string (for network/debug stuff)
std::string serializeWorld(const GameState& state);

// Load world from a string
bool deserializeWorld(GameState& state, const std::string& data);

#endif
