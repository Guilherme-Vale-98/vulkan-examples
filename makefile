SHELL = C:/raylib/w64devkit/bin/sh.exe

CXX = g++
CC  = gcc

VULKAN_SDK = C:/VulkanSDK/1.4.341.1
SDL_DIR    = C:/SDL3
GLSLC      = $(VULKAN_SDK)/Bin/glslc.exe

INCLUDES = \
	-isystem "$(SDL_DIR)/include" \
	-isystem "$(VULKAN_SDK)/Include" \
	-iquote vk \
	-iquote tests

DEFINES = -DVK_NO_PROTOTYPES -DVK_USE_PLATFORM_WIN32_KHR

OPT      = -O2
DEPFLAGS = -MMD -MP

CXXFLAGS = -std=c++23 -Wall -Wextra -Wno-missing-field-initializers $(OPT) $(DEFINES) $(INCLUDES) $(DEPFLAGS)
CFLAGS   = -std=c17 -Wno-missing-field-initializers $(OPT) $(DEFINES) $(INCLUDES) $(DEPFLAGS)

LDFLAGS = \
	-static-libgcc \
	-static-libstdc++ \
	-L"$(SDL_DIR)/lib" \
	-lSDL3

BUILD_DIR    = build
BIN_DIR      = $(BUILD_DIR)/bin
TEST_BIN_DIR = $(BUILD_DIR)/testbin
VOLK_SRC     = $(VULKAN_SDK)/Include/Volk/volk.c

BASE_SRCS = $(wildcard vk/*.cpp)
BASE_OBJS = $(BASE_SRCS:%.cpp=$(BUILD_DIR)/%.o) $(BUILD_DIR)/volk.o

EXAMPLES  = $(notdir $(wildcard examples/*))

TEST_SRCS = $(wildcard tests/*.cpp)
TEST_BINS = $(TEST_SRCS:tests/%.cpp=$(TEST_BIN_DIR)/%.exe)
TEST_OBJS = $(TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o)
TEST_SHSRC = $(wildcard tests/shaders/*.vert tests/shaders/*.frag tests/shaders/*.comp)
TEST_SPV   = $(TEST_SHSRC:tests/shaders/%=$(TEST_BIN_DIR)/shaders/%.spv)

DEPS = $(BASE_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

.DEFAULT_GOAL := examples

# ---------- generic compile ----------
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/volk.o: $(VOLK_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c "$(VOLK_SRC)" -o $@

# ---------- runtime DLL ----------
$(BIN_DIR)/SDL3.dll: $(SDL_DIR)/bin/SDL3.dll
	@mkdir -p $(@D)
	cp "$<" "$@"

$(TEST_BIN_DIR)/SDL3.dll: $(SDL_DIR)/bin/SDL3.dll
	@mkdir -p $(@D)
	cp "$<" "$@"

# ---------- tests ----------
$(TEST_BIN_DIR)/shaders/%.spv: tests/shaders/%
	@mkdir -p $(@D)
	"$(GLSLC)" --target-env=vulkan1.4 -O $< -o $@

$(TEST_BIN_DIR)/%.exe: $(BUILD_DIR)/tests/%.o $(BASE_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(BUILD_DIR)/tests/$*.o $(BASE_OBJS) $(LDFLAGS)

test: $(TEST_BINS) $(TEST_SPV) $(TEST_BIN_DIR)/SDL3.dll
	@fail=0; \
	for t in $(TEST_BINS); do \
	  echo "--- $$t"; \
	  ( cd $(TEST_BIN_DIR) && ./$$(basename $$t) ) || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; \
	else echo "ALL TESTS PASSED"; fi

# ---------- one target per example folder ----------
define EXAMPLE_RULE
$(1)_SRCS  := $$(wildcard examples/$(1)/*.cpp)
$(1)_OBJS  := $$($(1)_SRCS:%.cpp=$$(BUILD_DIR)/%.o)
$(1)_SHSRC := $$(wildcard examples/$(1)/*.vert examples/$(1)/*.frag examples/$(1)/*.comp)
$(1)_SPV   := $$($(1)_SHSRC:examples/$(1)/%=$$(BIN_DIR)/shaders/$(1)/%.spv)

DEPS += $$($(1)_OBJS:.o=.d)

$$(BIN_DIR)/shaders/$(1)/%.spv: examples/$(1)/%
	@mkdir -p $$(@D)
	"$$(GLSLC)" --target-env=vulkan1.4 -O $$< -o $$@

$$(BIN_DIR)/$(1).exe: $$($(1)_OBJS) $$(BASE_OBJS)
	@mkdir -p $$(@D)
	$$(CXX) -o $$@ $$($(1)_OBJS) $$(BASE_OBJS) $$(LDFLAGS)

$(1): $$(BIN_DIR)/$(1).exe $$($(1)_SPV) $$(BIN_DIR)/SDL3.dll

run-$(1): $(1)
	@cd $$(BIN_DIR) && ./$(1).exe

.PHONY: $(1) run-$(1)
endef

$(foreach e,$(EXAMPLES),$(eval $(call EXAMPLE_RULE,$(e))))

examples: $(EXAMPLES)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

.NOTPARALLEL:
.PHONY: examples test clean
