default:all

all:main

main:main.o delaytask.o
	g++  $^ -o $@ -std=c++11 -lpthread

%.o:%.cpp
	g++ -c $^ -o $@ -g -std=c++11

.PHONY:
clean:
	rm *.o main
