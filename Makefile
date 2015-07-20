show:
	gcc -Wall -Wextra -Iinclude -g3 -O0 -o bin/show \
		src/display.c \
		`pkg-config --cflags --libs gtk+-2.0 --libs gthread-2.0`

stream:
	gcc -Wall -Wextra -Iinclude -g3 -O0 -o bin/stream \
		src/stream.c src/decode.c src/device.c \
		-ljpeg -lavutil -lavformat -lavcodec

clean:
	rm bin/stream
