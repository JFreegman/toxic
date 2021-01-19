# Variables for game support
GAMES_CFLAGS = -DGAMES
GAMES_OBJ = game_base.o game_centipede.o game_chess.o game_util.o game_snake.o
CFLAGS += $(GAMES_CFLAGS)
OBJ += $(GAMES_OBJ)
