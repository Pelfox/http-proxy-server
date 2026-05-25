BUILD_DIR := build

all: build

build: $(BUILD_DIR)/Makefile
	@cmake --build $(BUILD_DIR)

build-tests: $(BUILD_DIR)/Makefile
	@cmake --build $(BUILD_DIR) --target tests

$(BUILD_DIR)/Makefile:
	@cmake -B $(BUILD_DIR) -S .

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build

test: build-tests
	@$(BUILD_DIR)/tests

.PHONY: all build build-tests clean rebuild test
