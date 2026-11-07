GOOSE ?= .toolchain/goose/build/goose
CC ?= gcc
.NOTPARALLEL:
.PHONY: setup format check test test-debug test-native fuzz bench-smoke
setup:
	./scripts/setup-goose.sh
format:
	python3 scripts/native-manifest.py
	"$(GOOSE)" format watch.goose tests examples bench
check:
	python3 scripts/native-manifest.py --check
	"$(GOOSE)" format --check watch.goose tests examples bench
	"$(GOOSE)" check tests/api.goose
	python3 tests/ownership.py "$(abspath $(GOOSE))"
test:
	mkdir -p .build
	python3 tests/integration.py "$(abspath $(GOOSE))" run tests/api.goose --
	"$(GOOSE)" build tests/api.goose --cc "$(CC)" -O2 -o .build/api
	python3 tests/integration.py .build/api
test-debug:
	mkdir -p .build
	"$(GOOSE)" build tests/api.goose --cc "$(CC)" --debug --no-bce -o .build/api-debug
	python3 tests/integration.py .build/api-debug

test-native:
	mkdir -p .build
	$(CC) -std=c11 -Wall -Wextra -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined tests/decoder.c -o .build/decoder-$(CC)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 .build/decoder-$(CC)
	$(CC) -std=c11 -Wall -Wextra -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined tests/faults.c -o .build/faults-$(CC)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 .build/faults-$(CC)
	$(CC) -std=c11 -Wall -Wextra -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined tests/lifecycle.c -o .build/lifecycle-$(CC)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 .build/lifecycle-$(CC)

FUZZ_RUNS ?= 50000
fuzz:
	mkdir -p .build
	clang -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=fuzzer,address,undefined tests/fuzz.c -o .build/fuzz
	python3 scripts/fuzz-seeds.py
	UBSAN_OPTIONS=halt_on_error=1 .build/fuzz .build/fuzz-corpus -seed=219 -max_len=8192 -runs=$(FUZZ_RUNS) -timeout=2

.PHONY: test-sanitize
test-sanitize:
	mkdir -p .build
	"$(GOOSE)" build tests/api.goose --standalone --debug --no-bce --cc ./scripts/sanitizer-cc.sh -o .build/api-sanitize
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 python3 tests/integration.py .build/api-sanitize
