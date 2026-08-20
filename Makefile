BUILD_DIR := build
DEBUG_BUILD_DIR := build-debug

.PHONY: build build-debug run run-debug deb clean

build:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	@cmake --build $(BUILD_DIR)
	@ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

build-debug:
	@cmake -S . -B $(DEBUG_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(DEBUG_BUILD_DIR)

run: build
	@QT_LOGGING_RULES="qt.text.font.match.warning=false" ./$(BUILD_DIR)/tide

run-debug: build-debug
	@QT_LOGGING_RULES="qt.text.font.match.warning=false" ./$(DEBUG_BUILD_DIR)/tide

deb: build
	@cd $(BUILD_DIR) && cpack -G DEB

clean:
	@rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR)
