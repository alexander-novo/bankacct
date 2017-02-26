.cpp:
	g++ -Wall -g -o $* -I/usr/include/ncursesw $*.cpp -std=c++11 -lncursesw -ltinfo 
