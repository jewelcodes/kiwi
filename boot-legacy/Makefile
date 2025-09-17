MODULE:=boot-legacy
CC=x86_64-axon-gcc
LD=x86_64-axon-ld
ASM=nasm

CFLAGS=-c -Wall -O3 -I./include -I../global
LDFLAGS=-O3
ASMFLAGS=-f bin -wnone

BIN_SRC:=$(shell find . -type f -name "*.asm")
BIN_OBJ:=$(BIN_SRC:.asm=.bin)
CSRC:=$(shell find . -type f -name "*.c")
COBJ:=$(CSRC:.c=.o)

RECURSION_DEPTH ?= 0

.PHONY: all clean build

all: build

clean:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🧹  boot-legacy\n" $$indent ""
	@rm -f $(COBJ) $(BIN_OBJ)

%.o: %.c
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s⚡️  $<\n" $$indent ""
	@$(CC) -o $@ ${CFLAGS} $<

%.bin: %.asm
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🔩  $<\n" $$indent ""
	@$(ASM) -o $@ ${ASMFLAGS} $<

build:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🧱  building \x1B[1m$(MODULE)\x1B[0m\n" $$indent ""; \
	printf "\x1B[0m %*s🔍  checking for changes\n" $$indent ""; \
	if $(MAKE) -q $(MODULE); then \
		printf "\x1B[0m %*s✅  \x1B[1m$(MODULE)\x1B[0m is up to date\n" $$indent ""; \
	else \
		start=$$(date +%s.%N); \
		next_depth=$$(( $(RECURSION_DEPTH) + 1 )); \
		if [ $$next_depth -gt 16 ]; then \
			printf "\x1B[0m %*s❌  recursion depth exceeded\n" $$indent ""; \
			printf "\x1B[0m %*s❌  \x1B[1m$(MODULE)\x1B[0m failing\n" $$indent ""; \
			exit 1; \
		fi; \
		$(MAKE) --no-print-directory RECURSION_DEPTH=$$next_depth $(MODULE); \
		ret=$$?; \
		end=$$(date +%s.%N); \
		dur=$$(echo "$$end - $$start" | bc); \
		if [ $$ret -ne 0 ]; then \
			printf "\x1B[0m %*s❌  \x1B[1m$(MODULE)(\x1B[0m failing after %.2fs\n" $$indent "" $$dur; \
			exit $$ret; \
		fi; \
		printf "\x1B[0m %*s✅  built \x1B[1m$(MODULE)\x1B[0m in %.2fs\n" $$indent "" $$dur; \
	fi

$(MODULE): $(COBJ) $(BIN_OBJ)
