CC       = gcc
CPPFLAGS += -I slow5lib/include/
CFLAGS   += -g -Wall -O2
LDFLAGS  += $(LIBS) -lz -lpthread -lm
BUILD_DIR = build

zstd=1

ifeq ($(zstd),1)
LDFLAGS		+= -lzstd
endif

BINARY = slowION

OBJ = $(BUILD_DIR)/main.o \
	  $(BUILD_DIR)/misc.o \
	  $(BUILD_DIR)/error.o \
	  $(BUILD_DIR)/slowion.o \

ifdef asan
	CFLAGS += -fsanitize=address -fno-omit-frame-pointer
	LDFLAGS += -fsanitize=address -fno-omit-frame-pointer
endif

.PHONY: clean test

$(BINARY): $(OBJ) slow5lib/lib/libslow5.a
	$(CC) $(CFLAGS) $(OBJ) slow5lib/lib/libslow5.a $(LDFLAGS) -o $@

$(BUILD_DIR)/main.o: src/main.c src/misc.h src/error.h src/slowion.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LANGFLAG) $< -c -o $@

$(BUILD_DIR)/error.o: src/error.c src/error.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/misc.o: src/misc.c src/misc.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/slowion.o: src/slowion.c src/slowion.h src/misc.h src/error.h src/rand.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

slow5lib/lib/libslow5.a:
	$(MAKE) -C slow5lib zstd=$(zstd) no_simd=$(no_simd) zstd_local=$(zstd_local) lib/libslow5.a

clean:
	rm -rf $(BINARY) $(BUILD_DIR)/*.o
	make -C slow5lib clean

test: $(BINARY)
	./test/test.sh

