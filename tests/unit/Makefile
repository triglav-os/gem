BUILD_DIR ?= $(abspath ../../build)
.PHONY: all run
all:
	$(MAKE) -C ../.. all
run:
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L unit
