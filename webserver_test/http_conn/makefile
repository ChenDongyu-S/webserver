src = $(wildcard ./*.c)
object = $(patsubst %.c,%.o,$(src))
target = hello
CC = g++

$target:$(object)
	$(CC) -o $@ $<

%.o:%.c
	$(CC) -g -o $@ -c $< -pthread

.PHONY:clean
clean:
	rm -f $(object)