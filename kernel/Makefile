MODULE:=kernel
CC=x86_64-kiwi-gcc
LD=x86_64-kiwi-ld
ASM=nasm

ARCH=x86_64
ARCH_DEF=ARCH_X86_64
ASFLAGS=-f elf64 -i./src/x86_64/$(ARCH)
CFLAGS=-Wall -c -D$(ARCH_DEF) -I../global -I./src/include -I./src/arch/$(ARCH)/include -mno-sse -ffreestanding -O3 -mcmodel=kernel -mno-red-zone
LDFLAGS=-T./src/arch/$(ARCH)/linker-$(ARCH).ld -nostdlib

CSRC:=$(shell find . -type f -name "*.c")
COBJ:=$(CSRC:.c=.o)

ASRC:=$(shell find . -type f -name "*.asm")
AOBJ:=$(ASRC:.asm=.o)

RECURSION_DEPTH ?= 0

.PHONY: all clean build

all: build

clean:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🧹  kernel\n" $$indent ""
	@rm -f $(AOBJ) $(COBJ)

%.o: %.c
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s⚡️  $<\n" $$indent ""
	@$(CC) -o $@ ${CFLAGS} $<

%.o: %.asm
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🔩  $<\n" $$indent ""
	@$(ASM) -o $@ ${ASFLAGS} $<

kiwi: $(AOBJ) $(COBJ)
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🚀  \x1B[1mkiwi\x1B[0m\n" $$indent ""; \
	$(LD) -o kiwi ${LDFLAGS} $(AOBJ) $(COBJ)

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

$(MODULE): kiwi
