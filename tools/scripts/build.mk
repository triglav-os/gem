# Internal build recipes, invoked inside Docker by container.sh.
ifneq ($(GEM_IN_CONTAINER),1)
$(error Use the root Makefile; internal build recipes require Docker)
endif
.DEFAULT_GOAL := all
BUILD_DIR ?= $(CURDIR)/build/container-sdl
GEM_OUTPUT_ROOT ?= $(CURDIR)/bin
JOBS ?= 4
TEST_JOBS ?= 4
BUILD_TYPE ?= RelWithDebInfo
.PHONY: all configure rasta cef mupdf tests unit integration uat sdk package install clean audit standards
configure:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DGEM_PLATFORM=rasta -DBUILD_TESTING=ON -DGEM_OUTPUT_ROOT="$(GEM_OUTPUT_ROOT)" $(CMAKE_FLAGS)
all: configure
	cmake --build "$(BUILD_DIR)" --parallel $(JOBS)
rasta: configure
	cmake --build "$(BUILD_DIR)" --target rasta_viewer --parallel $(JOBS)
mupdf: configure
	cmake --build "$(BUILD_DIR)" --target gemread_mupdf --parallel $(JOBS)
cef: configure
	cmake --build "$(BUILD_DIR)" --target gemscape_cef --parallel $(JOBS)
tests: all
	python3 -B tests/report.py --build "$(BUILD_DIR)" --jobs "$(TEST_JOBS)"
unit integration uat: all
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure --parallel "$(TEST_JOBS)" -L $@
sdk: configure
	cmake --build "$(BUILD_DIR)" --target gem_sdk --parallel $(JOBS)
clean:
	cmake --build "$(BUILD_DIR)" --target clean
audit: all
	python3 -B tools/scripts/audit.py --build "$(BUILD_DIR)"
standards:
	python3 -B tools/scripts/audit.py --build "$(BUILD_DIR)" --standards-only

package: all
	cmake --build "$(BUILD_DIR)" --target gemix_package
install: all
	DESTDIR="$(DESTDIR)" cmake --install "$(BUILD_DIR)" --prefix "$(PREFIX)" --strip
