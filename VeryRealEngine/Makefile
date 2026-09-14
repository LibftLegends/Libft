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
# Usual rules: all, clean, fclean, re. `make SANITIZE=1` builds an
# ASan+UBSan-instrumented copy alongside the normal one (see below).

NAME        := libVeryRealEngine.a
DEMO        := very_real_engine_demo

CXX         := c++
CXXFLAGS    := -std=c++17 -Wall -Wextra -O3 -Isrc
DEPFLAGS    = -MMD -MP

UNAME_S     := $(shell uname -s)

# --- Engine sources (everything that becomes the library) ------------------
LIB_CPP_SRCS := src/math/vec3.cpp \
                src/math/mat4inverse.cpp \
                src/math/mat4.cpp \
                src/math/aabb.cpp \
                src/math/frustum.cpp \
                src/ecs/entity.cpp \
                src/ecs/poolbase.cpp \
                src/ecs/registry.cpp \
                src/renderer/framestats.cpp \
                src/renderer/frustumculler.cpp \
                src/renderer/geometryrenderpassfactory.cpp \
                src/renderer/geometrypipelinefactory.cpp \
                src/renderer/geometryscenetargets.cpp \
                src/renderer/geometryglobalubo.cpp \
                src/renderer/geometrypass.cpp \
                src/renderer/light.cpp \
                src/renderer/materialhandle.cpp \
                src/renderer/meshhandle.cpp \
                src/renderer/meshregistry.cpp \
                src/renderer/occlusionpipelinefactory.cpp \
                src/renderer/occlusionculler.cpp \
                src/renderer/postprocesspipelinefactory.cpp \
                src/renderer/postprocesspass.cpp \
                src/renderer/renderitem.cpp \
                src/renderer/rendererframesync.cpp \
                src/renderer/shadowcasterselector.cpp \
                src/renderer/renderer.cpp \
                src/renderer/screenshotcapture.cpp \
                src/renderer/shadowlightmatrix.cpp \
                src/renderer/shadowpipelinefactory.cpp \
                src/renderer/shadowpass.cpp \
                src/renderer/swapchain.cpp \
                src/renderer/swapchainsupportdetails.cpp \
                src/renderer/texturehandle.cpp \
                src/renderer/textureuploader.cpp \
                src/renderer/textureregistry.cpp \
                src/renderer/vulkanphysicaldeviceselector.cpp \
                src/renderer/vulkancommandscope.cpp \
                src/renderer/vulkanbufferfactory.cpp \
                src/renderer/vulkanimagefactory.cpp \
                src/renderer/vulkandevice.cpp \
                src/renderer/vulkaninstance.cpp \
                src/assets/jsonvalue.cpp \
                src/assets/jsonlexer.cpp \
                src/assets/jsongrammar.cpp \
                src/assets/jsonparser.cpp \
                src/assets/meshvertex.cpp \
                src/assets/submesh.cpp \
                src/assets/materialdata.cpp \
                src/assets/meshdata.cpp \
                src/assets/imagedata.cpp \
                src/assets/mtlloader.cpp \
                src/assets/objfaceindexparser.cpp \
                src/assets/objgeometrybuilder.cpp \
                src/assets/objloader.cpp \
                src/assets/tgaloader.cpp \
                src/assets/skinnedasset.cpp \
                src/scene/namecomponent.cpp \
                src/scene/parentcomponent.cpp \
                src/scene/transformcomponent.cpp \
                src/scene/worldtransformcomponent.cpp \
                src/scene/meshcomponent.cpp \
                src/scene/visibilitycomponent.cpp \
                src/scene/physicsbodycomponent.cpp \
                src/scene/transformcomposer.cpp \
                src/scene/scenephysicsparser.cpp \
                src/scene/scenelightparser.cpp \
                src/scene/sceneobjectloader.cpp \
                src/scene/scenerendercollector.cpp \
                src/scene/scene.cpp \
                src/platform/window.cpp \
                src/physics/bodyhandle.cpp \
                src/physics/collider.cpp \
                src/physics/rigidbodydesc.cpp \
                src/physics/contact.cpp \
                src/physics/physicsbody.cpp \
                src/physics/collisiondetector.cpp \
                src/physics/contactresolver.cpp \
                src/physics/physicsworld.cpp \
                src/particles/particlesystemdesc.cpp \
                src/particles/particlesystem.cpp \
                src/audio/soundhandle.cpp \
                src/audio/voicehandle.cpp \
                src/audio/wavclip.cpp \
                src/audio/wavloader.cpp \
                src/audio/audiosystem.cpp \
                src/audio/mixer.cpp \
                src/assets/skinnedmeshbonereader.cpp \
                src/assets/skinnedmeshvertexreader.cpp \
                src/assets/skinnedmeshanimationreader.cpp \
                src/assets/skinnedmeshloader.cpp \
                src/animation/bone.cpp \
                src/animation/keyframe.cpp \
                src/animation/animationtrack.cpp \
                src/animation/animationclip.cpp \
                src/animation/animator.cpp \
                src/animation/skeleton.cpp
LIB_MM_SRCS  :=

ifeq ($(UNAME_S),Linux)
LIB_CPP_SRCS += src/platform/linux/linuxkeymap.cpp \
                src/platform/linux/windowlinux.cpp \
                src/platform/linux/audiolinux.cpp
# -lasound: ALSA, a Linux system library (same category as the XCB the
# subject names as an allowed example) — see src/platform/linux/audiolinux.hpp.
LDLIBS       := -lvulkan -lX11 -lasound -lpthread
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
# No CoreAudio backend yet (see src/audio/nullaudiosystem.cpp) — macOS is
# the local-dev-only platform, Linux is the actual submission target.
LIB_CPP_SRCS += src/audio/nullaudiosystem.cpp \
                src/platform/macos/macoskeymap.cpp
LIB_MM_SRCS  += src/platform/macos/vremetalview.mm \
                src/platform/macos/vrewindowdelegate.mm \
                src/platform/macos/windowmacos.mm
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

# `make SANITIZE=1` (or `re`/`clean` with the same flag) builds a separate,
# instrumented copy — its own object dir/library/binary names, so it never
# clobbers a normal release build sitting alongside it. Used to verify the
# subject's "no memory leaks" / "no unexpected termination" requirements
# with real tooling (ASan catches leaks/UB memory errors, UBSan catches
# undefined behavior like signed overflow or misaligned access) rather than
# just by inspection.
ifeq ($(SANITIZE),1)
CXXFLAGS    += -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined
LDLIBS      += -fsanitize=address,undefined
LIB_BASENAME := VeryRealEngine-sanitize
NAME        := libVeryRealEngine-sanitize.a
DEMO        := very_real_engine_demo-sanitize
OBJ_DIR_SUFFIX := -sanitize
else
LIB_BASENAME := VeryRealEngine
OBJ_DIR_SUFFIX :=
endif

DEMO_SRCS   := src/main.cpp \
               src/app/application.cpp \
               src/app/debugconfig.cpp \
               src/app/demoaudio.cpp \
               src/app/doorinteraction.cpp \
               src/app/lightswitchinteraction.cpp \
               src/app/pendulumrig.cpp \
               src/app/firstpersoncamera.cpp

OBJ_DIR     := obj$(OBJ_DIR_SUFFIX)
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
	$(CXX) $(CXXFLAGS) $(DEMO_OBJS) -L. -l$(LIB_BASENAME) $(LDLIBS) -o $@

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
	rm -rf $(OBJ_DIR) obj obj-sanitize

fclean: clean
	rm -f $(NAME) $(DEMO) $(SHADER_SPVS) \
	    libVeryRealEngine.a libVeryRealEngine-sanitize.a \
	    very_real_engine_demo very_real_engine_demo-sanitize

re: fclean all

-include $(DEPS)
