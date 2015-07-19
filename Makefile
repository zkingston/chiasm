all:
	mkdir -p bin
	gcc -Wall -Wextra -Iinclude -g3 -O0 -o bin/stream src/*.c \
	-ljpeg -lavutil -lavformat -lavcodec

clean:
	rm bin/stream
