#include "twentyfour.h"

#include "colors.h"
#include "equationevaluator.h"
#include "menu.h"
#include "types.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

struct card {
    std::string face;
    int value;
    int suit;
};

class TwentyFour {
public:
    TwentyFour();
    bool playGame(int attempts, int timeLimit);

private:
    void printCards(const std::vector<card>& cards);
    std::vector<card> picked;
    std::vector<std::vector<int>> allPuzzles;
    bool win;
    int gamestate;

    bool evaluateInput(std::string expression);
    void pickCards();
    bool validateInput(std::string expression);
    bool checkNumbersUsed(const std::string& expression, std::vector<card> numbers);
};

static std::vector<int> parseNumbers(const std::string& numbersStr) {
    std::vector<int> result;
    std::string clean;

    for (char c : numbersStr) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == ',') {
            clean += c;
        }
    }

    std::stringstream ss(clean);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            result.push_back(std::stoi(token));
        }
    }

    return result;
}

static std::vector<std::vector<int>> loadPuzzleNumbers(const std::string& filename) {
    std::vector<std::vector<int>> puzzles;
    std::ifstream file(filename);

    if (!file.is_open()) {
        return puzzles;
    }

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            continue;
        }

        if (line.empty()) continue;

        size_t bracketPos = line.find(']');
        if (bracketPos == std::string::npos) continue;

        std::vector<int> numbers = parseNumbers(line.substr(0, bracketPos + 1));

        if (numbers.size() == 4) {
            puzzles.push_back(numbers);
        }
    }

    return puzzles;
}

TwentyFour::TwentyFour() : win(false), gamestate(0) {
    allPuzzles = loadPuzzleNumbers("twentyfourpuzzles.csv");
}

void TwentyFour::printCards(const std::vector<card>& cards) {
    std::string P = hpad(47);

    std::cout << P;
    for (const auto& card : cards) std::cout << "┌─────────┐ ";
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) {
        std::cout << "│" << card.face;
        if (card.face.size() == 1) std::cout << "        │ ";
        else std::cout << "       │ ";
    }
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) std::cout << "│         │ ";
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) {
        std::string suit_symbol;
        if (card.suit == 0) suit_symbol = "♥";
        else if (card.suit == 1) suit_symbol = "♦";
        else if (card.suit == 2) suit_symbol = "♣";
        else suit_symbol = "♠";

        std::cout << "│    " << suit_symbol << "    │ ";
    }
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) std::cout << "│         │ ";
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) {
        if (card.face.size() == 1) std::cout << "│        " << card.face << "│ ";
        else std::cout << "│       " << card.face << "│ ";
    }
    std::cout << "\n";

    std::cout << P;
    for (const auto& card : cards) std::cout << "└─────────┘ ";
    std::cout << "\n";
}

void TwentyFour::pickCards() {
    if (allPuzzles.empty()) return;

    int puzzleNumber = rand() % allPuzzles.size();
    std::vector<int>& selectedNumbers = allPuzzles[puzzleNumber];
    picked.clear();

    for (int value : selectedNumbers) {
        card c;
        c.value = value;

        if (value == 1) c.face = "A";
        else if (value == 11) c.face = "J";
        else if (value == 12) c.face = "Q";
        else if (value == 13) c.face = "K";
        else c.face = std::to_string(value);

        c.suit = rand() % 4;
        picked.push_back(c);
    }
}

bool TwentyFour::validateInput(std::string expression) {
    if (expression.empty()) {
        std::cout << hpad(35) << "Error: Please enter an expression\n";
        return false;
    }

    std::string validChars = "0123456789+-*/() .";
    for (char c : expression) {
        if (validChars.find(c) == std::string::npos) {
            std::cout << hpad(38) << "Error: Invalid character in expression\n";
            return false;
        }
    }

    evaluator eval;
    try {
        eval.evaluate(expression);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool TwentyFour::checkNumbersUsed(const std::string& expression, std::vector<card> cards) {
    evaluator eval;
    std::vector<int> cardValues;

    for (const auto& card : cards) {
        cardValues.push_back(card.value);
    }

    return eval.checkNumbersUsed(expression, cardValues);
}

bool TwentyFour::evaluateInput(std::string expression) {
    evaluator eval;

    try {
        double result = eval.evaluate(expression);
        return std::fabs(result - 24.0) < 1e-9;
    } catch (const std::exception&) {
        return false;
    }
}

bool TwentyFour::playGame(int attempts, int timeLimit) {
    pickCards();

    if (picked.empty()) {
        gamestate = -1;
    } else {
        auto startTime = std::chrono::steady_clock::now();

        while (true) {
            clearAndCenterV(18);

            std::cout << COLOR_BOLD_CYAN << "\n";
            std::cout << hpad(56) << "╔══════════════════════════════════════════════════════╗\n";
            std::cout << hpad(56) << "║                                                      ║\n";
            std::cout << hpad(56) << "║              🃏  24 GAME MINIGAME  🃏               ║\n";
            std::cout << hpad(56) << "║                                                      ║\n";
            std::cout << hpad(56) << "║    Use +, -, *, / and parentheses to make 24         ║\n";
            std::cout << hpad(56) << "║    Use each card value exactly once                  ║\n";
            std::cout << hpad(56) << "║    Enter q to flee [-2x penalty]                     ║\n";
            std::cout << hpad(56) << "╚══════════════════════════════════════════════════════╝\n\n";
            std::cout << COLOR_RESET;

            printCards(picked);

            std::cout << "\n" << hpad(20) << "Card values: ";
            for (size_t i = 0; i < picked.size(); i++) {
                std::cout << picked[i].value;
                if (i < picked.size() - 1) std::cout << ", ";
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();

            if (elapsed >= timeLimit) {
                gamestate = -1;
                win = false;
                break;
            }

            if (attempts <= 0) {
                gamestate = 0;
                win = false;
                break;
            }

            std::string input;
            std::cout << "\n\n" << hpad(58)
                      << "Time: " << (timeLimit - elapsed) << "s"
                      << "   Attempts: " << attempts << "\n";
            std::cout << hpad(58) << "Enter expression: ";

            struct termios cooked, raw;
            tcgetattr(STDIN_FILENO, &cooked);
            raw = cooked;
            cooked.c_lflag |= (ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &cooked);

            std::getline(std::cin, input);

            tcsetattr(STDIN_FILENO, TCSANOW, &raw);

            if (input == "q" || input == "Q" || input == "quit" || input == "QUIT") {
                g_minigameForfeited = true;
                win = false;
                break;
            }

            if (!validateInput(input)) {
                attempts--;
                sleep(1);
            } else if (!checkNumbersUsed(input, picked)) {
                std::cout << hpad(42) << "Please use each card value exactly once.\n";
                attempts--;
                sleep(1);
            } else if (evaluateInput(input)) {
                win = true;
                break;
            } else {
                std::cout << hpad(22) << "Incorrect. Try again.\n";
                attempts--;
                sleep(1);
            }
        }
    }

    clearAndCenterV(win ? 12 : 11);

    std::string Aw = hpad(59);
    std::string Ag = hpad(80);

    if (win) {
        std::cout << COLOR_BOLD_GREEN << "\n"
            << Aw << "   ██╗   ██╗ ██████╗ ██╗   ██╗    ██╗    ██╗██╗███╗   ██╗\n"
            << Aw << "   ╚██╗ ██╔╝██╔═══██╗██║   ██║    ██║    ██║██║████╗  ██║\n"
            << Aw << "    ╚████╔╝ ██║   ██║██║   ██║    ██║ █╗ ██║██║██╔██╗ ██║\n"
            << Aw << "     ╚██╔╝  ██║   ██║██║   ██║    ██║███╗██║██║██║╚██╗██║\n"
            << Aw << "      ██║   ╚██████╔╝╚██████╔╝    ╚███╔███╔╝██║██║ ╚████║\n"
            << Aw << "      ╚═╝    ╚═════╝  ╚═════╝      ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝\n"
            << COLOR_RESET << "\n";

        std::cout << "\n" << hpad(23) << "Correct! You made 24.\n";
    } else {
        std::cout << COLOR_BOLD_RED << "\n"
            << Ag << "   ██████╗  █████╗ ███╗   ███╗███████╗     ██████╗ ██╗   ██╗███████╗██████╗\n"
            << Ag << "  ██╔════╝ ██╔══██╗████╗ ████║██╔════╝    ██╔═══██╗██║   ██║██╔════╝██╔══██╗\n"
            << Ag << "  ██║  ███╗███████║██╔████╔██║█████╗      ██║   ██║██║   ██║█████╗  ██████╔╝\n"
            << Ag << "  ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝      ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗\n"
            << Ag << "  ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗    ╚██████╔╝ ╚████╔╝ ███████╗██║  ██║\n"
            << Ag << "   ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝     ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝\n"
            << COLOR_RESET << "\n";

        if (g_minigameForfeited)
            std::cout << "\n" << hpad(22) << "You fled the 24 Game.\n";
        else if (gamestate == -1)
            std::cout << "\n" << hpad(20) << "You ran out of time.\n";
        else
            std::cout << "\n" << hpad(24) << "You ran out of attempts.\n";
    }

    const char* prompt = "Press any key to continue...";
    std::cout << "\n" << hpad(28) << COLOR_DIM << prompt << COLOR_RESET;
    std::cout.flush();

    char dummy;
    read(STDIN_FILENO, &dummy, 1);

    return win;
}

bool runTwentyFour(int attempts, int timeLimit) {
    TwentyFour game;
    return game.playGame(attempts, timeLimit);
}