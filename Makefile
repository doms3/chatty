CURL_CFLAGS=$(shell curl-config --cflags)
CURL_LIBS=$(shell curl-config --libs)

JSON_CFLAGS=$(shell pkg-config --cflags json-c)
JSON_LIBS=$(shell pkg-config --libs json-c)

CFLAGS=-Wall -Wextra -Werror -std=gnu11 -O2 $(CURL_CFLAGS) $(JSON_CFLAGS)
LDFLAGS=$(CURL_LIBS) $(JSON_LIBS)

RM=rm -f

chatty: aichat.o chatty.o chatty_methods.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) *.o chatty
