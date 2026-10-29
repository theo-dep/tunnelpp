configure:
	cmake --preset clang

build-debug: configure
	cmake --build --preset clang-debug

build-release: configure
	cmake --build --preset clang-release

configure-tests:
	cmake --preset clang-test

build-tests: configure-tests
	cmake --build --preset clang-test-debug

# usage: just test '[base64*]' (Catch2 test spec, optional)
test *args: build-tests
	ctest --preset clang-test-debug --output-on-failure {{args}}

configure-coverage:
	cmake --preset clang-test -B build/coverage -DTUNNELPP_BUILD_COVERAGE=ON

coverage: configure-coverage
	cmake --build build/coverage --config Debug
	cd build/coverage && LLVM_PROFILE_FILE="$PWD/coverage-%p.profraw" ctest -C Debug --output-on-failure
	llvm-profdata merge -sparse $(find build/coverage -name '*.profraw') -o build/coverage.profdata
	llvm-cov report build/coverage/common/tests/Debug/common_tests -object build/coverage/server/tests/Debug/server_tests -object build/coverage/client/tests/Debug/client_tests -object build/coverage/tests/Debug/e2e_tests --instr-profile=build/coverage.profdata -ignore-filename-regex='vcpkg_installed|/usr/|build/'

tidy-configure:
	cmake --preset clang-lint

tidy *files:
	clang-tidy -p build/clang-lint {{files}}

format *files:
	clang-format -i {{files}}

check: test
