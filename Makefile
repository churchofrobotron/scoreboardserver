all:
	g++ -o scoreboard_server src/main.cpp lib/mongoose/mongoose.c -std=c++0x -Ilib/mongoose -lpthread -fpermissive -ldl -g