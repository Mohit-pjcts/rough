/*
 * fileio.cpp
 * 
 * Save/load implementation. Writes everything to text files.
 * Formatss line by line.
 */

#include "fileio.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iostream>
#include <cstdio>

// ----- SAVE/LOAD GAME -----

bool saveGame(const GameState& state, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Header so we know it's our file
    file << "TERMICRAFT_SAVE_V1\n";
    
    // Basic game state
    file << static_cast<int>(state.phase) << "\n";
    file << static_cast<int>(state.difficulty) << "\n";
    file << state.seed << "\n";
    file << state.score << "\n";
    file << state.oresMined << "\n";
    file << state.enemiesKilled << "\n";
    
    // Player stuff
    file << state.player.name << "\n";
    file << state.player.pos.x << " " << state.player.pos.y << "\n";
    file << state.player.health << " " << state.player.maxHealth << "\n";
    file << state.player.alive << "\n";
    
    // Inventory
    file << state.player.inventory.wood << " ";
    file << state.player.inventory.stone << " ";
    file << state.player.inventory.coal << " ";
    file << state.player.inventory.iron << " ";
    file << state.player.inventory.gold << " ";
    file << state.player.inventory.diamond << "\n";
    
    // Equipment
    file << static_cast<int>(state.player.equipment.pickaxe) << " ";
    file << static_cast<int>(state.player.equipment.armor) << "\n";
    
    // Dragon stuff
    file << state.dragonCaveFound << " ";
    file << state.dragonCavePos.x << " " << state.dragonCavePos.y << " ";
    file << state.dragonDefeated << "\n";
    
    // World dimensions
    file << state.worldWidth << " " << state.worldHeight << "\n";
    
    // World data — type and mined flag per cell
    for (int y = 0; y < state.worldHeight; y++) {
        for (int x = 0; x < state.worldWidth; x++) {
            file << static_cast<int>(state.world[y][x].type)
                 << "," << static_cast<int>(state.world[y][x].mined);
            if (x < state.worldWidth - 1) file << " ";
        }
        file << "\n";
    }
    
    // Enemies
    file << state.enemies.size() << "\n";
    for (const Enemy& enemy : state.enemies) {
        file << enemy.name << "\n";
        file << enemy.pos.x << " " << enemy.pos.y << "\n";
        file << enemy.health << " " << enemy.maxHealth << "\n";
        file << enemy.damage << " " << enemy.alive << " " << enemy.symbol << "\n";
    }
    
    file.close();
    return true;
}

bool loadGame(GameState& state, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Check header
    std::string header;
    std::getline(file, header);
    if (header != "TERMICRAFT_SAVE_V1") {
        file.close();
        return false;
    }
    
    // Read basic state
    int phase, difficulty;
    file >> phase >> difficulty;
    state.phase = static_cast<GamePhase>(phase);
    state.difficulty = static_cast<Difficulty>(difficulty);
    state.settings = getDifficultySettings(state.difficulty);
    
    file >> state.seed;
    file >> state.score;
    file >> state.oresMined;
    file >> state.enemiesKilled;
    file.ignore(); // skip the newline after these numbers
    
    // Player
    std::getline(file, state.player.name);
    file >> state.player.pos.x >> state.player.pos.y;
    file >> state.player.health >> state.player.maxHealth;
    file >> state.player.alive;
    
    // Inventory
    file >> state.player.inventory.wood;
    file >> state.player.inventory.stone;
    file >> state.player.inventory.coal;
    file >> state.player.inventory.iron;
    file >> state.player.inventory.gold;
    file >> state.player.inventory.diamond;
    
    // Equipment
    int pickaxe, armor;
    file >> pickaxe >> armor;
    state.player.equipment.pickaxe = static_cast<MaterialTier>(pickaxe);
    state.player.equipment.armor = static_cast<MaterialTier>(armor);
    
    // Dragon state
    file >> state.dragonCaveFound;
    file >> state.dragonCavePos.x >> state.dragonCavePos.y;
    file >> state.dragonDefeated;
    
    // IMPORTANT: save old height BEFORE reading new dimensions
    // otherwise we'll try to delete rows that don't exist and crash
    int oldHeight = state.worldHeight;
    
    // Read new world size
    file >> state.worldWidth >> state.worldHeight;
    
    // Free old world using the OLD height
    if (state.world != nullptr) {
        for (int y = 0; y < oldHeight; y++) {
            delete[] state.world[y];
        }
        delete[] state.world;
    }
    
    // Allocate new world
    state.world = new Block*[state.worldHeight];
    for (int y = 0; y < state.worldHeight; y++) {
        state.world[y] = new Block[state.worldWidth];
    }
    
    // Read world data — type,mined per cell
    for (int y = 0; y < state.worldHeight; y++) {
        for (int x = 0; x < state.worldWidth; x++) {
            int type = 0, mined = 0;
            char comma;
            file >> type >> comma >> mined;
            state.world[y][x].type    = static_cast<BlockType>(type);
            state.world[y][x].mined   = (mined != 0);
            state.world[y][x].visible = false; // recalculated on load
        }
    }
    
    // Enemies
    std::size_t enemyCount;
    file >> enemyCount;
    file.ignore();
    
    state.enemies.clear();
    for (std::size_t i = 0; i < enemyCount; i++) {
        Enemy enemy;
        std::getline(file, enemy.name);
        file >> enemy.pos.x >> enemy.pos.y;
        file >> enemy.health >> enemy.maxHealth;
        file >> enemy.damage >> enemy.alive >> enemy.symbol;
        file.ignore();
        state.enemies.push_back(enemy);
    }
    
    file.close();
    return true;
}

bool saveFileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

bool deleteSaveFile(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
}

// ----- HIGH SCORES -----

std::vector<HighScore> loadHighScores() {
    std::vector<HighScore> scores;
    
    std::ifstream file(HIGHSCORE_FILE);
    if (!file.is_open()) {
        return scores;
    }
    
    std::string header;
    std::getline(file, header);
    if (header != "TERMICRAFT_HIGHSCORES_V1") {
        file.close();
        return scores;
    }
    
    std::size_t count;
    file >> count;
    file.ignore();
    
    for (std::size_t i = 0; i < count && i < 10; i++) {
        HighScore hs;
        std::getline(file, hs.playerName);
        
        int difficulty;
        file >> hs.score >> difficulty >> hs.timestamp >> hs.defeatedDragon;
        hs.difficulty = static_cast<Difficulty>(difficulty);
        file.ignore();
        
        scores.push_back(hs);
    }
    
    file.close();
    
    // Sort highest first
    std::sort(scores.begin(), scores.end(), [](const HighScore& a, const HighScore& b) {
        return a.score > b.score;
    });
    
    return scores;
}

bool saveHighScores(const std::vector<HighScore>& scores) {
    std::ofstream file(HIGHSCORE_FILE);
    if (!file.is_open()) {
        return false;
    }
    
    file << "TERMICRAFT_HIGHSCORES_V1\n";
    
    std::size_t count = std::min(scores.size(), (std::size_t)10);
    file << count << "\n";
    
    for (std::size_t i = 0; i < count; i++) {
        const HighScore& hs = scores[i];
        file << hs.playerName << "\n";
        file << hs.score << " ";
        file << static_cast<int>(hs.difficulty) << " ";
        file << hs.timestamp << " ";
        file << hs.defeatedDragon << "\n";
    }
    
    file.close();
    return true;
}

bool addHighScore(const HighScore& newScore) {
    std::vector<HighScore> scores = loadHighScores();
    
    scores.push_back(newScore);
    
    // Sort and keep top 10
    std::sort(scores.begin(), scores.end(), [](const HighScore& a, const HighScore& b) {
        return a.score > b.score;
    });
    
    if (scores.size() > 10) {
        scores.resize(10);
    }
    
    // Check if this score made the cut
    bool madeList = false;
    for (const HighScore& hs : scores) {
        if (hs.score == newScore.score && hs.playerName == newScore.playerName) {
            madeList = true;
            break;
        }
    }
    
    saveHighScores(scores);
    return madeList;
}

HighScore getTopHighScore() {
    std::vector<HighScore> scores = loadHighScores();
    if (scores.empty()) {
        return HighScore();
    }
    return scores[0];
}

bool isHighScore(int score) {
    std::vector<HighScore> scores = loadHighScores();
    
    if (scores.size() < 10) {
        return score > 0;
    }
    
    return score > scores.back().score;
}

// ----- WORLD SERIALIZATION -----

std::string serializeWorld(const GameState& state) {
    std::stringstream ss;
    
    ss << state.worldWidth << "," << state.worldHeight << ";";
    
    for (int y = 0; y < state.worldHeight; y++) {
        for (int x = 0; x < state.worldWidth; x++) {
            ss << static_cast<int>(state.world[y][x].type);
            if (x < state.worldWidth - 1) ss << ",";
        }
        if (y < state.worldHeight - 1) ss << ";";
    }
    
    return ss.str();
}

bool deserializeWorld(GameState& state, const std::string& data) {
    try {
        // Parse dimensions from start of string
        std::size_t dimEnd = data.find(';');
        if (dimEnd == std::string::npos) return false;
        
        std::string dimStr = data.substr(0, dimEnd);
        std::size_t comma = dimStr.find(',');
        if (comma == std::string::npos) return false;
        
        // Save old height before we overwrite it
        int oldHeight = state.worldHeight;
        
        state.worldWidth = std::stoi(dimStr.substr(0, comma));
        state.worldHeight = std::stoi(dimStr.substr(comma + 1));
        
        // Free old world with OLD dimensions
        if (state.world != nullptr) {
            for (int y = 0; y < oldHeight; y++) {
                delete[] state.world[y];
            }
            delete[] state.world;
        }
        
        // Allocate new world
        state.world = new Block*[state.worldHeight];
        for (int y = 0; y < state.worldHeight; y++) {
            state.world[y] = new Block[state.worldWidth];
        }
        
        // Parse the actual world data
        std::string worldData = data.substr(dimEnd + 1);
        std::stringstream ss(worldData);
        
        for (int y = 0; y < state.worldHeight; y++) {
            std::string row;
            std::getline(ss, row, ';');
            
            std::stringstream rowss(row);
            for (int x = 0; x < state.worldWidth; x++) {
                std::string cell;
                std::getline(rowss, cell, ',');
                
                int type = std::stoi(cell);
                state.world[y][x].type = static_cast<BlockType>(type);
                state.world[y][x].mined = (type == BLOCK_AIR);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        // If anything goes wrong parsing, just fail gracefully
        return false;
    }
}
