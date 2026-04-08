BUILD_DIR ?= build/debug
CONFIG ?= Debug

.PHONY: build test first-use-smoke format dump-paths clean

build:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CONFIG)"
	cmake --build "$(BUILD_DIR)"

test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

first-use-smoke: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_first_use_smoke$$' --output-on-failure

format:
	find compiler tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i

dump-paths: build
	@test -n "$(FILE)" || { echo "usage: make dump-paths FILE=tests/cases/hello.mc"; exit 1; }
	"$(BUILD_DIR)/mc" dump-paths "$(FILE)" --build-dir "$(BUILD_DIR)"

clean:
	rm -rf build
