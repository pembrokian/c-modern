BUILD_DIR ?= build/debug
CONFIG ?= Debug

.PHONY: build test first-use-smoke public-cut-smoke release-readiness-audit v0_2_gate freestanding-support-audit release-snapshot-prep format dump-paths clean

build:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CONFIG)"
	cmake --build "$(BUILD_DIR)"

test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

first-use-smoke: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_first_use_smoke$$' --output-on-failure

public-cut-smoke: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_public_cut_smoke$$' --output-on-failure

release-readiness-audit: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_release_readiness_audit$$' --output-on-failure

v0_2_gate: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_v0_2_gate$$' --output-on-failure

freestanding-support-audit: build
	ctest --test-dir "$(BUILD_DIR)" -R '^mc_freestanding_support_audit$$' --output-on-failure

release-snapshot-prep: build
	./tools/release/prepare_v0_2_snapshot.sh --source-root . --build-dir "$(BUILD_DIR)"

format:
	find compiler tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i

dump-paths: build
	@test -n "$(FILE)" || { echo "usage: make dump-paths FILE=tests/cases/hello.mc"; exit 1; }
	"$(BUILD_DIR)/mc" dump-paths "$(FILE)" --build-dir "$(BUILD_DIR)"

clean:
	rm -rf build
