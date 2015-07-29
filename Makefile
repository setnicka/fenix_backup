PROG=fenix_tester
CLASSES=Config FileInfo FileTree FileChunk Functions BackupCleaner
ADAPTERS=Adapter LocalFilesystemAdapter
OTHER=fenix_tester.o sha256.o
DIRECTORIES=obj/adapters

OBJS=$(addprefix obj/,${OTHER} $(addsuffix .o,${CLASSES} $(addprefix adapters/,${ADAPTERS}) ))

INC=-Isrc -Iinclude

CFLAGS=-Wall -std=c++11 -c
LDFLAGS=-Wall -lvcdcom -lvcdenc -lvcddec -lconfig++ -lboost_system -lboost_filesystem
CC=g++

all: directories ${PROG}
	
obj/%.o: src/%.cpp
	${CC} ${CFLAGS} ${INC} -o $@ $<

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} ${INC} -o $@ $^

clean:
	rm -f ${PROG} ${OBJS}

directories:
	mkdir -p ${DIRECTORIES}

.PHONY: clean all directories

.SECONDARY:
