/*
 * menu.h
 * 
 * All the UI screens - main menu, difficulty select, game over, etc.
 * Also handles the in-game HUD (health bar, inventory display).
 */

#ifndef MENU_H
#define MENU_H

#include "types.h"
#include <string>
#include <vector>

// ----- MAIN MENU STUFF -----

// Shows the main menu, returns what the user picked
// 1=New Game, 2=Load, 3=High Scores, 4=How to Play, 5=Quit
int showMainMenu(const HighScore& highScore);

// Let player pick Easy/Normal/Hard
Difficulty selectDifficulty();

// Show the top 10 scores
void showHighScores(const std::vector<HighScore>& scores);

// Show controls and how to play
void showHowToPlay();

// Game over screen - shows stats and final score
void showGameOver(const GameState& state, bool isVictory);

// Yes/No prompt, returns true if they said yes
bool showConfirmation(const std::string& message);

// "Press any key to continue"
void waitForKeypress();

// Ask player to type their name
std::string getPlayerName(const std::string& prompt);

// Read a single keypress without waiting for Enter
char getch();

// Set to true by wordle/minesweeper when player quits mid-game (forfeit penalty)
extern bool g_minigameForfeited;

// ----- ASCII ART -----

// Big fancy title banner
void showTitleArt();

// You won! art
void showVictoryArt();

// You died art
void showDefeatArt();

// Draw a box around some text lines
void showBox(const std::vector<std::string>& lines, int width = 50);

// ----- IN-GAME HUD -----

// Draw the HUD at top of screen (health, inventory, etc)
void renderHUD(const GameState& state);

// Open the crafting menu
// Returns true if they crafted something
bool showCraftingMenu(GameState& state);

// Show detailed inventory screen
void showInventory(const GameState& state);

// Show a status message at bottom of screen
void showStatusMessage(const std::string& message, bool isError = false);

#endif
