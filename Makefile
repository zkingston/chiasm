all:
	gcc -Wall -Wextra -Iinclude -o bin/stream src/*.c -ljpeg

clean:
	rm bin/stream
