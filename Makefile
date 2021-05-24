%: %.c
	gcc -o $@ $@.c -fsanitize=address,leak,undefined -lm -Wall
