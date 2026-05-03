/*
 * sudoku.cpp
 *
 * Sudoku minigame for TermiCraft.
 * Unlocks a specific tier (e.g., Diamond) by solving the grid.
 * Easy/Medium: 6x6 grid (numbers 1-6)
 * Hard: 9x9 grid (numbers 1-9)
 */

#include "colors.h"
#include "menu.h"
#include "types.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

class SudokuGame {
private:
    int size;
    int boxR; // Subgrid rows
    int boxC; // Subgrid cols
    std::vector<std::vector<int>> grid;
    std::vector<std::vector<bool>> fixed;
    int cellsToRemove;
    std::string lastMessage;
    std::string boardP;

    void waitForKey() {
        const char* prompt = "Press any key to continue...";
        std::cout << "\n" << hpad(28) << COLOR_DIM << prompt << COLOR_RESET;
        std::cout.flush();

        struct termios oldt, raw;
        tcgetattr(STDIN_FILENO, &oldt);
        raw = oldt;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        tcflush(STDIN_FILENO, TCIFLUSH);
        char dummy;
        read(STDIN_FILENO, &dummy, 1);

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }

    void showEndScreen(bool won) {
        clearAndCenterV(won ? 12 : 11);

        std::string Aw = hpad(59);
        std::string Ag = hpad(80);

        if (won) {
            std::cout << COLOR_BOLD_GREEN << "\n"
                << Aw << "   ██╗   ██╗ ██████╗ ██╗   ██╗    ██╗    ██╗██╗███╗   ██╗\n"
                << Aw << "   ╚██╗ ██╔╝██╔═══██╗██║   ██║    ██║    ██║██║████╗  ██║\n"
                << Aw << "    ╚████╔╝ ██║   ██║██║   ██║    ██║ █╗ ██║██║██╔██╗ ██║\n"
                << Aw << "     ╚██╔╝  ██║   ██║██║   ██║    ██║███╗██║██║██║╚██╗██║\n"
                << Aw << "      ██║   ╚██████╔╝╚██████╔╝    ╚███╔███╔╝██║██║ ╚████║\n"
                << Aw << "      ╚═╝    ╚═════╝  ╚═════╝      ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝\n"
                << COLOR_RESET << "\n";

            std::cout << "\n" << hpad(24) << "Sudoku board solved.\n";
        } else {
            std::cout << COLOR_BOLD_RED << "\n"
                << Ag << "   ██████╗  █████╗ ███╗   ███╗███████╗     ██████╗ ██╗   ██╗███████╗██████╗\n"
                << Ag << "  ██╔════╝ ██╔══██╗████╗ ████║██╔════╝    ██╔═══██╗██║   ██║██╔════╝██╔══██╗\n"
                << Ag << "  ██║  ███╗███████║██╔████╔██║█████╗      ██║   ██║██║   ██║█████╗  ██████╔╝\n"
                << Ag << "  ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝      ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗\n"
                << Ag << "  ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗    ╚██████╔╝ ╚████╔╝ ███████╗██║  ██║\n"
                << Ag << "   ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝     ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝\n"
                << COLOR_RESET << "\n";

            std::cout << "\n" << hpad(17) << "You fled Sudoku.\n";
        }

        waitForKey();
    }

    void printDivider() {
        std::cout << boardP << COLOR_CYAN << "+";
        for (int c = 0; c < size; c++) {
            std::cout << "---";
            if ((c + 1) % boxC == 0) {
                std::cout << "+";
            }
        }
        std::cout << COLOR_RESET << "\n";
    }

    bool isSafe(int r, int c, int num) {
        for (int i = 0; i < size; i++) {
            if (grid[r][i] == num || grid[i][c] == num) return false;
        }
        int startRow = r - r % boxR;
        int startCol = c - c % boxC;
        for (int i = 0; i < boxR; i++) {
            for (int j = 0; j < boxC; j++) {
                if (grid[i + startRow][j + startCol] == num) return false;
            }
        }
        return true;
    }

    bool findUnassigned(int& r, int& c) {
        for (r = 0; r < size; r++) {
            for (c = 0; c < size; c++) {
                if (grid[r][c] == 0) return true;
            }
        }
        return false;
    }

    bool solveGrid() {
        int r, c;
        if (!findUnassigned(r, c)) return true;

        std::vector<int> nums;
        for (int i = 1; i <= size; i++) nums.push_back(i);
        
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(nums.begin(), nums.end(), g);

        for (int num : nums) {
            if (isSafe(r, c, num)) {
                grid[r][c] = num;
                if (solveGrid()) return true;
                grid[r][c] = 0;
            }
        }
        return false;
    }

    void generateBoard() {
        grid.assign(size, std::vector<int>(size, 0));
        fixed.assign(size, std::vector<bool>(size, false));
        lastMessage = "";

        solveGrid();

        int removed = 0;
        while (removed < cellsToRemove) {
            int r = rand() % size;
            int c = rand() % size;
            if (grid[r][c] != 0) {
                grid[r][c] = 0;
                removed++;
            }
        }

        for (int r = 0; r < size; r++) {
            for (int c = 0; c < size; c++) {
                if (grid[r][c] != 0) {
                    fixed[r][c] = true;
                }
            }
        }
    }

    bool isSolved() {
        for (int r = 0; r < size; r++) {
            for (int c = 0; c < size; c++) {
                if (grid[r][c] == 0) return false;
            }
        }
        return true;
    }

public:
    SudokuGame(Difficulty diff) {
        srand(static_cast<unsigned int>(time(nullptr)));
        if (diff == DIFF_EASY) {
            size = 6; boxR = 2; boxC = 3; cellsToRemove = 15;
        } else if (diff == DIFF_NORMAL) {
            size = 6; boxR = 2; boxC = 3; cellsToRemove = 22;
        } else {
            size = 9; boxR = 3; boxC = 3; cellsToRemove = 45;
        }
        generateBoard();
    }

    void displayBoard() {
        clearScreen();
        std::string titleP = hpad(36);
        boardP = hpad(size * 3 + (size / boxC) + 4);

        std::cout << COLOR_BOLD_CYAN;
        std::cout << "\n" << titleP << "╔══════════════════════════════════╗\n";
        std::cout << titleP << "║      🔢 SUDOKU MINIGAME 🔢      ║\n";
        std::cout << titleP << "║  Fill the grid so every row,     ║\n";
        std::cout << titleP << "║  column, and box is unique!      ║\n";
        std::cout << titleP << "╚══════════════════════════════════╝\n\n";
        std::cout << COLOR_RESET;

        std::cout << boardP << "  ";
        for (int c = 0; c < size; c++) {
            std::cout << COLOR_DIM << (c + 1) << "  " << COLOR_RESET;
            if ((c + 1) % boxC == 0) std::cout << "  ";
        }
        std::cout << "\n";

        printDivider();
        for (int r = 0; r < size; r++) {
            std::cout << boardP << COLOR_DIM << "  " << (r + 1) << COLOR_RESET << " " << COLOR_CYAN << "|" << COLOR_RESET;
            for (int c = 0; c < size; c++) {
                if (grid[r][c] == 0) {
                    std::cout << COLOR_DIM << " . " << COLOR_RESET;
                } else if (fixed[r][c]) {
                    std::cout << COLOR_BOLD_YELLOW << " " << grid[r][c] << " " << COLOR_RESET;
                } else {
                    std::cout << COLOR_BOLD_GREEN << " " << grid[r][c] << " " << COLOR_RESET;
                }

                if ((c + 1) % boxC == 0) {
                    std::cout << COLOR_CYAN << "|" << COLOR_RESET;
                }
            }
            std::cout << "\n";
            if ((r + 1) % boxR == 0) {
                printDivider();
            }
        }
        
        if (!lastMessage.empty()) {
            std::cout << "\n" << boardP << COLOR_RED << lastMessage << COLOR_RESET;
            lastMessage = "";
        } else {
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    bool playGame() {
        while (!isSolved()) {
            displayBoard();

            std::cout << COLOR_WHITE << boardP << "Enter move (Row Col Value) e.g., '1 3 5'.\n" << COLOR_RESET;
            std::cout << COLOR_DIM << boardP << "Enter '0 0 0' to quit.\n" << COLOR_RESET;
            std::cout << COLOR_WHITE << "\n" << boardP << "Your move: " << COLOR_RESET;

            // Safely toggle terminal mode for standard input
            struct termios cooked, raw;
            tcgetattr(STDIN_FILENO, &cooked);
            raw = cooked;
            cooked.c_lflag |= (ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &cooked);

            std::string input;
            std::getline(std::cin, input);

            tcsetattr(STDIN_FILENO, TCSANOW, &raw);

            std::stringstream ss(input);
            int r, c, val;
            if (ss >> r >> c >> val) {
                if (r == 0 && c == 0 && val == 0) {
                    g_minigameForfeited = true;
                    showEndScreen(false);
                    return false;
                }
                
                r--; c--;

                if (r < 0 || r >= size || c < 0 || c >= size) {
                    lastMessage = "Invalid row or column!";
                    continue;
                }
                if (val < 0 || val > size) {
                    lastMessage = "Invalid value! Must be between 1 and " + std::to_string(size) + " (or 0 to clear).";
                    continue;
                }
                if (fixed[r][c]) {
                    lastMessage = "You can't change a fixed number!";
                    continue;
                }

                if (val == 0) {
                    grid[r][c] = 0;
                } else {
                    int temp = grid[r][c];
                    grid[r][c] = 0;
                    if (isSafe(r, c, val)) {
                        grid[r][c] = val;
                    } else {
                        grid[r][c] = temp;
                        lastMessage = "Invalid move! Conflicts with row, col, or box.";
                    }
                }
            } else {
                lastMessage = "Please enter 3 numbers separated by spaces.";
            }
        }

        lastMessage = "";
        showEndScreen(true);
        return true;
    }
};

// ----- EXTERNAL ENTRY POINT -----

bool runSudoku(Difficulty diff) {
    SudokuGame game(diff);
    return game.playGame();
}
