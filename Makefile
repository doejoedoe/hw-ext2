CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99
LDFLAGS =

TARGETS = util1 util2 util3
COMMON = ext2.c

.PHONY: all clean test valgrind cleanext2

all: $(TARGETS)

util1: util1.c $(COMMON) ext2.h
	$(CC) $(CFLAGS) -o $@ util1.c $(COMMON) $(LDFLAGS)

util2: util2.c $(COMMON) ext2.h
	$(CC) $(CFLAGS) -o $@ util2.c $(COMMON) $(LDFLAGS)

util3: util3.c $(COMMON) ext2.h
	$(CC) $(CFLAGS) -o $@ util3.c $(COMMON) $(LDFLAGS)

test: $(TARGETS)
	@chmod +x test.sh
	@./test.sh

valgrind: $(TARGETS)
	@echo "valgrind: util1"
	valgrind --leak-check=full ./util1 ext2_test.img 12 2>&1 | tail -5
	@echo ""
	@echo "valgrind: util2"
	valgrind --leak-check=full ./util2 ext2_test.img 12 2>&1 | tail -5
	@echo ""
	@echo "valgrind: util3"
	./util2 ext2_test.img 2 | valgrind --leak-check=full ./util3 2>&1 | tail -3
	@echo ""

clean:
	rm -f $(TARGETS) *.o *.extracted *_expected.txt *_util3.txt loop_*.extracted

allclean: clean
	-sudo umount mnt 2>/dev/null || true
	rm -f ext2_test.img
	rm -rf mnt