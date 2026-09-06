BUILD_DIR ?= $(abspath ../build)
.PHONY: all run unit integration uat
all:
	$(MAKE) -C .. all
run:
	ctest --test-dir $(BUILD_DIR) --output-on-failure
unit integration uat:
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $@
