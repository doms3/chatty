CURL_CFLAGS=$(shell curl-config --cflags)
CURL_LIBS=$(shell curl-config --libs)

JSON_CFLAGS=$(shell pkg-config --cflags json-c)
JSON_LIBS=$(shell pkg-config --libs json-c)

CFLAGS=-Wall -Wextra -Werror -std=gnu2x -g -O0 $(CURL_CFLAGS) $(JSON_CFLAGS) -fsanitize=leak
LDFLAGS=$(CURL_LIBS) $(JSON_LIBS) -fsanitize=leak

chatty: aichat.o chatty.o chatty_methods.o
	$(CC) -o $@ $^ $(LDFLAGS)
