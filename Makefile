BUILD_DIR := build

all: build

build: $(BUILD_DIR)/Makefile
	@cmake --build $(BUILD_DIR)

$(BUILD_DIR)/Makefile:
	@cmake -B $(BUILD_DIR) -S .

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build

test: build
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

.PHONY: all build clean rebuild test
