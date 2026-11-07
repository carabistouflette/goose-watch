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
