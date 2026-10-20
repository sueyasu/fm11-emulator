# Standalone ROM diagnostics; deliberately NOT registered as CMake test targets.
LWASM ?= lwasm
OUT ?= build/term-rom

.PHONY: all real-sub clean
all: $(OUT)/main.bin $(OUT)/sub.bin

$(OUT):
	mkdir -p "$@"

$(OUT)/main.bin: main.asm | $(OUT)
	$(LWASM) --format=raw --output="$@" "$<"
	@test "$$(wc -c < "$@")" -eq 4096 || { echo "main ROM size must be 4096 bytes" >&2; exit 1; }

$(OUT)/sub.bin: sub.asm | $(OUT)
	$(LWASM) --format=raw --output="$@" "$<"
	@test "$$(wc -c < "$@")" -eq 20480 || { echo "sub ROM size must be 20480 bytes" >&2; exit 1; }

# The actual /Term-path test intentionally uses the user's genuine sub ROM.
real-sub: $(OUT)/main-real-sub.bin

$(OUT)/main-real-sub.bin: main-real-sub.asm | $(OUT)
	$(LWASM) --format=raw --output="$@" "$<"
	@test "$(wc -c < "$@")" -eq 4096 || { echo "main ROM must be 4096 bytes" >&2; exit 1; }

clean:
	rm -rf "$(OUT)"
