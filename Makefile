default:all

all:main

main:main.o delaytask.o
	gcc  $^ -o $@ -lpthread

%.o:%.c
	gcc -c $^ -o $@ -g

.PHONY:
clean:
	rm *.o main
