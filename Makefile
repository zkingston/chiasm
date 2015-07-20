show:
	gcc -Wall -Wextra -Iinclude -g3 -O0 -o bin/show \
		src/display.c src/decode.c src/device.c \
		`pkg-config --cflags --libs gtk+-2.0 --libs gthread-2.0` \
		-ljpeg -lavutil -lavformat -lavcodec -lpthread

stream:
	gcc -Wall -Wextra -Iinclude -g3 -O0 -o bin/stream \
		src/stream.c src/decode.c src/device.c \
		-ljpeg -lavutil -lavformat -lavcodec -lpthread

clean:
	rm bin/stream
