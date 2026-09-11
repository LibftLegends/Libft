# VeryRealEngine — hand-written Makefile (replaces the previous CMake build).
#
# Builds two things:
#   - libVeryRealEngine.a : the engine itself (everything the subject requires
#     to be "the library"), from every src/ file except main.cpp.
#   - very_real_engine_demo : the demo executable, main.cpp linked against
#     the library above — exactly how any other game built on VeryRealEngine
#     would consume it.
#
# Linux (Xlib + the system Vulkan loader/ICD) is the actual submission
# target. macOS (Cocoa + MoltenVK, via Homebrew) is supported too, purely so
# the rest of the engine can be developed/iterated on locally without a
# Linux box — see README.md's macOS section for setup and known gaps
# (MoltenVK is not a fully conformant Vulkan implementation).
#
# Usual rules: all, clean, fclean, re.

NAME        := libVeryRealEngine.a
DEMO        := very_real_engine_demo

CXX         := c++
CXXFLAGS    := -std=c++17 -Wall -Wextra -O3 -Isrc
DEPFLAGS    = -MMD -MP

UNAME_S     := $(shell uname -s)

# --- Engine sources (everything that becomes the library) ------------------
LIB_CPP_SRCS := src/renderer/renderer.cpp \
                src/assets/obj_loader.cpp \
                src/assets/tga_loader.cpp \
                src/assets/json_parser.cpp \
                src/scene/scene.cpp \
                src/physics/physics_world.cpp \
                src/particles/particle_system.cpp
LIB_MM_SRCS  :=

ifeq ($(UNAME_S),Linux)
LIB_CPP_SRCS += src/platform/linux/window_linux.cpp
LDLIBS       := -lvulkan -lX11
else ifeq ($(UNAME_S),Darwin)
# Homebrew: `brew install vulkan-headers vulkan-loader molten-vk shaderc`.
# Linked directly against MoltenVK's own dylib (which implements the full
# Vulkan API itself) rather than going through the generic Vulkan loader —
# simpler, since Homebrew's molten-vk formula doesn't ship an ICD manifest
# for the loader to discover it by.
VULKAN_HEADERS_PREFIX := $(shell brew --prefix vulkan-headers 2>/dev/null)
MOLTENVK_PREFIX        := $(shell brew --prefix molten-vk 2>/dev/null)
ifeq ($(VULKAN_HEADERS_PREFIX),)
$(error vulkan-headers not found. Run: brew install vulkan-headers vulkan-loader molten-vk shaderc)
endif
ifeq ($(MOLTENVK_PREFIX),)
$(error molten-vk not found. Run: brew install vulkan-headers vulkan-loader molten-vk shaderc)
endif
LIB_MM_SRCS  += src/platform/macos/window_macos.mm
CXXFLAGS     += -I$(VULKAN_HEADERS_PREFIX)/include -I$(MOLTENVK_PREFIX)/include
LDLIBS       := -L$(MOLTENVK_PREFIX)/lib -lMoltenVK \
                -Wl,-rpath,$(MOLTENVK_PREFIX)/lib \
                -framework Cocoa -framework QuartzCore
# Xcode's `xcrun`-selected default SDK on this toolchain is a beta
# CommandLineTools SDK whose .tbd stub libraries (Cocoa, libSystem, libc++)
# the linker fails to parse ("unknown architecture"). Pin both compile and
# link steps to the stable Xcode SDK instead of whatever CommandLineTools
# ships as the ambient default; harmless once that's no longer the case.
MACOS_SDK    := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
ifneq ($(MACOS_SDK),)
CXXFLAGS     += -isysroot $(MACOS_SDK)
LDLIBS       += -isysroot $(MACOS_SDK)
endif
else
$(warning VeryRealEngine only ships Linux (Xlib) and macOS (Cocoa) platform \
backends; building on $(UNAME_S) will fail to link. See src/platform/.)
LDLIBS       := -lvulkan
endif

DEMO_SRCS   := src/main.cpp

OBJ_DIR     := obj
LIB_OBJS    := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(LIB_CPP_SRCS)) \
               $(patsubst src/%.mm,$(OBJ_DIR)/%.o,$(LIB_MM_SRCS))
DEMO_OBJS   := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(DEMO_SRCS))
DEPS        := $(LIB_OBJS:.o=.d) $(DEMO_OBJS:.o=.d)

# --- Shader compilation (SPIR-V) --------------------------------------------
# Prefer glslc (shaderc), fall back to glslangValidator. Compiled in place
# next to their .vert/.frag sources, since the demo loads "shaders/*.spv"
# relative to the working directory it's run from (the repo root).
GLSLC       := $(shell command -v glslc 2>/dev/null)
GLSLANG     := $(shell command -v glslangValidator 2>/dev/null)

SHADER_SRCS := $(wildcard shaders/*.vert) $(wildcard shaders/*.frag)
SHADER_SPVS := $(addsuffix .spv,$(SHADER_SRCS))

.PHONY: all clean fclean re shaders

all: $(NAME) $(DEMO) shaders

$(NAME): $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

$(DEMO): $(DEMO_OBJS) $(NAME)
	$(CXX) $(CXXFLAGS) $(DEMO_OBJS) -L. -lVeryRealEngine $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Objective-C++ platform backend (macOS only): ARC is scoped to just this
# rule via -fobjc-arc, so the rest of the (plain C++) engine isn't affected.
$(OBJ_DIR)/%.o: src/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -fobjc-arc -c $< -o $@

shaders: $(SHADER_SPVS)

shaders/%.spv: shaders/%
ifneq ($(GLSLC),)
	$(GLSLC) $< -o $@
else ifneq ($(GLSLANG),)
	$(GLSLANG) -V $< -o $@
else
	$(error Neither glslc nor glslangValidator found. Install one, e.g. \
	  'sudo apt install glslang-tools' or 'sudo apt install glslc')
endif

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(DEMO) $(SHADER_SPVS)

re: fclean all

-include $(DEPS)
