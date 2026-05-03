#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <vector>
#include <chrono>
#include <termios.h>
#include <unistd.h>

class Minesweeper {
public:
    Minesweeper(int size, int mines);
    void playGame();
    bool didWin() const { return win; }

private:
    int size;
    int mines;
    std::vector<std::vector<bool>> mineGrid;
    std::vector<std::vector<int>>  solutionGrid;
    std::vector<std::vector<char>> revealedGrid;
    bool gameOver;
    bool win;
    std::chrono::steady_clock::time_point startTime;

    void initializeGrids(int size);
    void placeMines();
    void fillSolutionGrid();
    bool isValidMove(int x, int y);
    int  countAdjacentMines(int x, int y);
    void revealSingleCell(int x, int y);
    void floodReveal(int x, int y);
    void flagCell(int x, int y);
    bool checkWin();
    void revealMines();
    void displayBoard();
    void clearScreen();
    void getPlayerInput();
    void makeMove(char action, int x, int y);
    char toUpper(char c);
    double getElapsedTime();
    void showHighScore();
    void saveHighScore(double time);
    void gameOverMessage();
};

// Entry point called by main.cpp when a mining minigame is triggered
bool runMinesweeper(int gridSize);

#endif
