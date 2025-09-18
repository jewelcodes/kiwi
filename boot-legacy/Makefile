MODULE:=boot-legacy
CC=x86_64-lux-gcc
LD=x86_64-lux-ld
ASM=nasm

CFLAGS=-Wall -c -I./include -I../global -ffreestanding -O2 -m32 -mno-sse --param=min-pagesize=0
LDFLAGS=-T./linker.ld -nostdlib -m elf_i386
STAGE1_ASMFLAGS=-f bin -wnone
STAGE2_ASMFLAGS=-f elf -wnone

STAGE1_SRC:=$(shell find ./bootsect -type f -name "*.asm")
STAGE1_OBJ:=$(STAGE1_SRC:.asm=.bin)
STAGE2_SRC:=$(shell find ./stage2 -type f -name "*.asm")
STAGE2_OBJ:=$(STAGE2_SRC:.asm=.o)
CSRC:=$(shell find . -type f -name "*.c")
COBJ:=$(CSRC:.c=.o)

RECURSION_DEPTH ?= 0

.PHONY: all clean build

all: build

clean:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🧹  boot-legacy\n" $$indent ""
	@rm -f $(COBJ) $(STAGE1_OBJ)

%.o: %.c
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s⚡️  $<\n" $$indent ""
	@$(CC) -o $@ ${CFLAGS} $<

%.o: %.asm
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🔩  $<\n" $$indent ""
	@$(ASM) -o $@ ${STAGE2_ASMFLAGS} $<

%.bin: %.asm
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🔩  $<\n" $$indent ""
	@$(ASM) -o $@ ${STAGE1_ASMFLAGS} $<

boot.bin: $(STAGE2_OBJ) $(COBJ)
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🔧  linking boot.bin\n" $$indent ""; \
	$(LD) -o boot.bin ${LDFLAGS} $(STAGE2_OBJ) $(COBJ)

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

$(MODULE): $(STAGE1_OBJ) boot.bin
