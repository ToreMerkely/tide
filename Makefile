BUILD_DIR := build

.PHONY: build run clean

build:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR)

run: build
	@./$(BUILD_DIR)/sild

clean:
	@rm -rf $(BUILD_DIR)
