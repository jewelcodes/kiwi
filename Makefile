MODULE:=kiwi
RECURSION_DEPTH ?= 0

.PHONY: all clean build

all: build

clean:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	new_depth=$$(( $(RECURSION_DEPTH) + 1 )); \
	printf "\x1B[0m %*s🧹  $(MODULE)\n" $$indent ""; \
	$(MAKE) --no-print-directory RECURSION_DEPTH=$$new_depth -C boot-legacy clean; \
	$(MAKE) --no-print-directory RECURSION_DEPTH=$$new_depth -C pulse clean;

build:
	@indent=$$(( $(RECURSION_DEPTH) )); \
	printf "\x1B[0m %*s🧱  building \x1B[1m$(MODULE)\x1B[0m\n" $$indent ""; \
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
		printf "\x1B[0m %*s❌  \x1B[1m$(MODULE)\x1B[0m failing after %.2fs\n" $$indent "" $$dur; \
		exit $$ret; \
	fi; \
	printf "\x1B[0m %*s✅  built \x1B[1m$(MODULE)\x1B[0m in %.2fs\n" $$indent "" $$dur; \

$(MODULE):
	@next_depth=$$(( $(RECURSION_DEPTH) + 1 )); \
	if [ $$next_depth -gt 16 ]; then \
		printf "\x1B[0m %*s❌  recursion depth exceeded\n" $$indent ""; \
		printf "\x1B[0m %*s❌  \x1B[1m$(MODULE)\x1B[0m failing\n" $$indent ""; \
		exit 1; \
	fi; \
	$(MAKE) --no-print-directory RECURSION_DEPTH=$$next_depth -C boot-legacy; \
	$(MAKE) --no-print-directory RECURSION_DEPTH=$$next_depth -C pulse; \
	printf "\x1B[0m %*s🧩  creating \x1B[1mkiwi.hdd\x1B[0m\n" $$(( $(RECURSION_DEPTH) )) ""; \
	mkdir -p guest; \
	cp boot-legacy/boot.bin guest/ ; \
	dd if=/dev/zero of=kiwi.hdd bs=1M count=128 status=none; \
	mke2fs -q -F -t ext2 -d ./guest -m 0 -E root_owner=0:0 -L kiwi kiwi.hdd; \
	dd if=./boot-legacy/bootsect/ext2.bin of=kiwi.hdd conv=notrunc bs=1024 count=1 status=none; \

run: qemu

qemu:
	@qemu-system-x86_64 \
		-drive file=kiwi.hdd,format=raw,if=ide \
		-m 512M \
		-monitor stdio
