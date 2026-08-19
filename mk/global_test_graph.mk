# The test executable is part of the root GNU Make graph.  This fragment is
# included only by the standalone Libft root Makefile; it deliberately does
# not define public clean or test targets.

LIBFT_GLOBAL_TEST_DISCOVERED_SOURCE_FILES := \
	Test/main.cpp $(wildcard Test/Test/*.cpp) $(wildcard Test/API/*.cpp)
LIBFT_GLOBAL_TEST_OBJECT_DIRECTORIES := $(sort $(patsubst %/,%,$(patsubst Test/%,%,\
	$(dir $(filter-out Test/main.cpp,$(LIBFT_GLOBAL_TEST_DISCOVERED_SOURCE_FILES))))))
LIBFT_GLOBAL_TEST_SOURCE_FILES := $(filter-out Test/Test/test_template.cpp,\
	$(LIBFT_GLOBAL_TEST_DISCOVERED_SOURCE_FILES))
LIBFT_GLOBAL_TEST_CONFIG_INPUTS := mk/test/dependencies.mk
LIBFT_GLOBAL_TEST_OBJECT_ROOT := $(LIBFT_GLOBAL_TEST_ROOT)
LIBFT_GLOBAL_TEST_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_OBJECT_ROOT)/%.o,$(LIBFT_GLOBAL_TEST_SOURCE_FILES))
LIBFT_GLOBAL_TEST_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_OBJECTS:.o=.d)
LIBFT_GLOBAL_TEST_DEBUG_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/%.o,$(LIBFT_GLOBAL_TEST_SOURCE_FILES))
LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_DEBUG_OBJECTS:.o=.d)

ifeq ($(OS),Windows_NT)
LIBFT_GLOBAL_TEST_EXECUTABLE := Test/libft_tests.exe
LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE := Test/libft_tests_debug.exe
else
LIBFT_GLOBAL_TEST_EXECUTABLE := Test/libft_tests
LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE := Test/libft_tests_debug
endif

LIBFT_GLOBAL_TEST_CXX_FLAGS := $(COMPILE_FLAGS) -Wno-missing-declarations \
	-DLIBFT_TEST_BUILD \
	-DTEST_MODULE=\"Libft\" -DGAME_USE_VOXEL_REGION_BACKEND=1 -pthread
LIBFT_GLOBAL_TEST_DEBUG_CXX_FLAGS := $(LIBFT_GLOBAL_TEST_CXX_FLAGS) -DDEBUG=1

include mk/test/dependencies.mk

# The legacy dependency fragment was written for Test/Makefile, where source
# names are relative to Test/.  The global graph uses repository-relative
# paths, so apply the optional OpenSSL filter with the correct paths here.
ifneq ($(and $(HAS_OPENSSL_HEADERS),$(HAS_OPENSSL_LIBS)),1)
LIBFT_GLOBAL_TEST_SOURCE_FILES := $(filter-out \
	Test/Test/test_api_tls_diagnostics.cpp \
	Test/Test/test_encryption_aead.cpp \
	Test/Test/test_encryption_aead_copy_move.cpp \
	Test/Test/test_encryption_hash_algorithms.cpp, \
	$(LIBFT_GLOBAL_TEST_SOURCE_FILES))
endif

LIBFT_GLOBAL_TEST_EXCLUDED_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_OBJECT_ROOT)/%.o,\
	$(filter-out $(LIBFT_GLOBAL_TEST_SOURCE_FILES),$(LIBFT_GLOBAL_TEST_DISCOVERED_SOURCE_FILES)))
LIBFT_GLOBAL_TEST_DEBUG_EXCLUDED_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/%.o,\
	$(filter-out $(LIBFT_GLOBAL_TEST_SOURCE_FILES),$(LIBFT_GLOBAL_TEST_DISCOVERED_SOURCE_FILES)))

# Recompute the object families after optional-source filtering.  Keeping the
# old expansion here would leave the executable depending on objects for
# sources for which no compile rule was generated.
LIBFT_GLOBAL_TEST_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_OBJECT_ROOT)/%.o,$(LIBFT_GLOBAL_TEST_SOURCE_FILES))
LIBFT_GLOBAL_TEST_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_OBJECTS:.o=.d)
LIBFT_GLOBAL_TEST_DEBUG_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/%.o,$(LIBFT_GLOBAL_TEST_SOURCE_FILES))
LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_DEBUG_OBJECTS:.o=.d)

ifeq ($(OS),Windows_NT)
LIBFT_GLOBAL_TEST_LINK_FLAGS := -Wl,--allow-multiple-definition -lz -lws2_32 \
	-lgdi32 -lwinmm -ldbghelp -lopengl32 $(OPENSSL_LIBS) $(SQLITE_LIBS)
else ifeq ($(UNAME_S),Darwin)
LIBFT_GLOBAL_TEST_LINK_FLAGS := -lz -framework Cocoa -framework CoreGraphics \
	-framework QuartzCore -framework AudioToolbox -lobjc -lpthread \
	$(OPENSSL_LIBS) $(SQLITE_LIBS)
else
LIBFT_GLOBAL_TEST_LINK_FLAGS := -Wl,--allow-multiple-definition -rdynamic \
	-rdynamic -lz -ldl $(OPENSSL_LIBS) $(SQLITE_LIBS) $(X11_LIBS) \
	$(XEXT_LIBS) $(XI_LIBS) $(GL_LIBS) $(ASOUND_LIBS) -pthread
endif

ifeq ($(UNAME_S),Darwin)
LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_START :=
LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_END :=
else
LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_START := -Wl,--start-group
LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_END := -Wl,--end-group
endif

LIBFT_GLOBAL_TEST_DIRECTORIES := $(sort $(patsubst %/,%,$(dir \
	$(LIBFT_GLOBAL_TEST_OBJECTS) $(LIBFT_GLOBAL_TEST_DEBUG_OBJECTS))))

$(LIBFT_GLOBAL_TEST_DIRECTORIES):
	@$(MKDIR) "$@"

define LIBFT_GLOBAL_DEFINE_TEST_OBJECT
$(1): $(2) $(LIBFT_GLOBAL_TEST_CONFIG_INPUTS) | $(patsubst %/,%,$(dir $(1)))
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|Test|$$<"; else printf '\033[1;36m[LIBFT][Test] Compiling %s\033[0m\n' "$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_CXX_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
endef

define LIBFT_GLOBAL_DEFINE_TEST_DEBUG_OBJECT
$(1): $(2) $(LIBFT_GLOBAL_TEST_CONFIG_INPUTS) | $(patsubst %/,%,$(dir $(1)))
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|TestDebug|$$<"; else printf '\033[1;36m[LIBFT][TestDebug] Compiling %s\033[0m\n' "$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_DEBUG_CXX_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
endef

$(foreach source_file,$(LIBFT_GLOBAL_TEST_SOURCE_FILES),$(eval $(call LIBFT_GLOBAL_DEFINE_TEST_OBJECT,$(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_OBJECT_ROOT)/%.o,$(source_file)),$(source_file))))
$(foreach source_file,$(LIBFT_GLOBAL_TEST_SOURCE_FILES),$(eval $(call LIBFT_GLOBAL_DEFINE_TEST_DEBUG_OBJECT,$(patsubst %.cpp,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/%.o,$(source_file)),$(source_file))))

$(LIBFT_GLOBAL_TEST_EXECUTABLE): $(LIBFT_GLOBAL_TEST_OBJECTS) \
		$(LIBFT_GLOBAL_TEST_TARGET) mk/global_test_graph.mk
	@if [ "$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|link|libft|Test|$@"; else printf '\033[1;35m[LIBFT][Test] Linking %s\033[0m\n' "$@"; fi
	@$(MKDIR) "$(dir $@)"
	@sh mk/write_object_response_file.sh "$@.rsp" "$(LIBFT_GLOBAL_TEST_OBJECT_ROOT)/Test" "Test" $(LIBFT_GLOBAL_TEST_OBJECT_DIRECTORIES) -- $(LIBFT_GLOBAL_TEST_EXCLUDED_OBJECTS)
	@printf '%s\n' "$(LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_START) $(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_END)" >> "$@.rsp"
	@$(CXX) $(LIBFT_GLOBAL_TEST_CXX_FLAGS) -o $@ @$@.rsp \
		$(LIBFT_GLOBAL_TEST_LINK_FLAGS)
	@$(RM) $@.rsp
	@printf '\033[1;35m[LIBFT][Test] Link ready: %s\033[0m\n' "$@"

$(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE): $(LIBFT_GLOBAL_TEST_DEBUG_OBJECTS) \
		$(LIBFT_GLOBAL_TEST_DEBUG_TARGET) mk/global_test_graph.mk
	@if [ "$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|link|libft|TestDebug|$@"; else printf '\033[1;35m[LIBFT][TestDebug] Linking %s\033[0m\n' "$@"; fi
	@$(MKDIR) "$(dir $@)"
	@sh mk/write_object_response_file.sh "$@.rsp" "$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Test" "Test" $(LIBFT_GLOBAL_TEST_OBJECT_DIRECTORIES) -- $(LIBFT_GLOBAL_TEST_DEBUG_EXCLUDED_OBJECTS)
	@printf '%s\n' "$(LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_START) $(LIBFT_GLOBAL_TEST_DEBUG_TARGET) $(LIBFT_GLOBAL_TEST_ARCHIVE_GROUP_END)" >> "$@.rsp"
	@$(CXX) $(LIBFT_GLOBAL_TEST_DEBUG_CXX_FLAGS) -o $@ @$@.rsp \
		$(LIBFT_GLOBAL_TEST_LINK_FLAGS)
	@$(RM) $@.rsp
	@printf '\033[1;35m[LIBFT][TestDebug] Link ready: %s\033[0m\n' "$@"

LIBFT_GLOBAL_TEST_ACTIVE_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_DEPENDENCIES)
ifneq ($(filter debug-tests run-debug-tests,$(MAKECMDGOALS)),)
    LIBFT_GLOBAL_TEST_ACTIVE_DEPENDENCIES := $(LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCIES)
endif
-include $(wildcard $(LIBFT_GLOBAL_TEST_ACTIVE_DEPENDENCIES))
