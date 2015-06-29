PROG=fenix_tester
CLASSES=Config FileInfo FileTree FileChunk
OTHER=fenix_tester.o sha256.o
OBJS=$(addprefix obj/,${OTHER} $(addsuffix .o,${CLASSES}))

INC=-Isrc -Iinclude

CFLAGS=-Wall -std=c++11 -c
LDFLAGS=-Wall -lvcdcom -lvcdenc -lvcddec -lconfig++ -lboost_system -lboost_filesystem
CC=g++

all: ${PROG}

obj/%.o: src/%.cpp
	${CC} ${CFLAGS} ${INC} -o $@ $<

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} ${INC} -o $@ $^

clean:
	rm -f ${PROG} ${OBJS}

.PHONY: clean all

.SECONDARY:
