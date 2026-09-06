BUILD_DIR ?= $(abspath ../build)
.PHONY: all run unit integration uat
all:
	$(MAKE) -C .. all
run:
	python3 -B report.py --build $(BUILD_DIR)
unit integration uat:
	$(MAKE) -C $@ run BUILD_DIR=$(BUILD_DIR)
