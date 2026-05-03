CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic

TARGET  = termicraft
OBJECTS = main.o menu.o fileio.o player.o final_fight.o score.o \
          crafting.o world_gen.o fog_of_war.o day_night.o \
          wordle.o minesweeper.o twentyfour.o equationevaluator.o sudoku.o

# ----- Link step -----
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lncurses

# ----- Compile rules -----

main.o: main.cpp menu.h fileio.h player.h world_gen.h fog_of_war.h final_fight.h score.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

menu.o: menu.cpp menu.h types.h colors.h
	$(CXX) $(CXXFLAGS) -c $<

fileio.o: fileio.cpp fileio.h types.h
	$(CXX) $(CXXFLAGS) -c $<

player.o: player.cpp player.h score.h colors.h crafting.h menu.h types.h
	$(CXX) $(CXXFLAGS) -c $<

final_fight.o: final_fight.cpp final_fight.h score.h fileio.h menu.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

score.o: score.cpp score.h fileio.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

crafting.o: crafting.cpp crafting.h player.h menu.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

world_gen.o: world_gen.cpp world_gen.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

fog_of_war.o: fog_of_war.cpp fog_of_war.h day_night.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

day_night.o: day_night.cpp day_night.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<

wordle.o: wordle.cpp colors.h menu.h types.h
	$(CXX) $(CXXFLAGS) -c $<

minesweeper.o: minesweeper.cpp minesweeper.h colors.h types.h
	$(CXX) $(CXXFLAGS) -c $<
	
twentyfour.o: twentyfour.cpp twentyfour.h equationevaluator.h colors.h menu.h types.h
	$(CXX) $(CXXFLAGS) -c $<

equationevaluator.o: equationevaluator.cpp equationevaluator.h
	$(CXX) $(CXXFLAGS) -c $<

sudoku.o: sudoku.cpp colors.h menu.h types.h
	$(CXX) $(CXXFLAGS) -c $<

# ----- Phony targets -----

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
