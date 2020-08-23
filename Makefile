default:all

all:main

main:main.o delaytask.o
	gcc -DTIMER $^ -o $@ -lpthread

%.o:%.c
	gcc -DTIMER -c $^ -o $@ -g

.PHONY:
clean:
	rm *.o main
