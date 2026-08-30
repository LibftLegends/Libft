# Canonical single-process Libft graph.
#
# This file is included by the Libft root Makefile.  Module manifests are
# loaded one at a time and their values are copied into namespaced variables so
# every module can coexist in one GNU Make database.

LIBFT_GLOBAL_GRAPH_PREFIX ?=
LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS ?= \
	$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/build_config.mk
LIBFT_GLOBAL_ARCHIVE_SUFFIX ?= $(LIBFT_ARCHIVE_SUFFIX)
LIBFT_GLOBAL_CONFIG_FINGERPRINT := $(shell printf '%s\n' \
	'$(CXX)|$(CC)|$(COMPILE_FLAGS)|$(CFLAGS)|$(CPPFLAGS)|$(SANITIZERS)|$(OPT_LEVEL)|$(BUILD_OUTPUT_SUFFIX)|$(LIBFT_GLOBAL_ARCHIVE_SUFFIX)' \
	| cksum | awk '{print $$1}')
LIBFT_GLOBAL_CONFIG ?= standalone$(BUILD_OUTPUT_SUFFIX)_cfg$(LIBFT_GLOBAL_CONFIG_FINGERPRINT)
LIBFT_GLOBAL_ROOT ?= $(LIBFT_GLOBAL_GRAPH_PREFIX)build/libft/$(LIBFT_GLOBAL_CONFIG)
LIBFT_GLOBAL_RELEASE_ROOT := $(LIBFT_GLOBAL_ROOT)/release
LIBFT_GLOBAL_DEBUG_ROOT := $(LIBFT_GLOBAL_ROOT)/debug
LIBFT_GLOBAL_TEST_ROOT := $(LIBFT_GLOBAL_ROOT)/test
LIBFT_GLOBAL_TEST_DEBUG_ROOT := $(LIBFT_GLOBAL_ROOT)/test_debug
LIBFT_GLOBAL_CC ?= gcc
LIBFT_GLOBAL_MV ?= mv


LIBFT_GLOBAL_MODULE_NAMES := Basic Advanced Compatebility Debug Errno CMA SCMA \
    GetNextLine DUMB Math Geometry System_utils Printf ReadLine Regex PThread \
    Threading CPP_class Template Buffer CLI Command Config CrossProcess \
    Compression CSV Encryption Crypto Encoding RNG JSon YAML File HTML Time \
    Filesystem XML Storage Networking URI API Application Observability Sink \
    Logger Parser Lua Game Voxel GPGR

LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES := $(filter-out Template,$(LIBFT_GLOBAL_MODULE_NAMES))

LIBFT_GLOBAL_MANIFESTS := $(addprefix $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/,$(addsuffix .mk,$(LIBFT_GLOBAL_MODULE_NAMES)))

define LIBFT_LOAD_GLOBAL_MANIFEST
LIBFT_GLOBAL_MANIFEST_LOAD := 1
include $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk
LIBFT_GLOBAL_$(1)_MANIFEST := $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk
LIBFT_GLOBAL_$(1)_DIRECTORY := $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)
LIBFT_GLOBAL_$(1)_TARGET := $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/$$(patsubst %.a,%$(LIBFT_GLOBAL_ARCHIVE_SUFFIX).a,$$($(1)_TARGET))
LIBFT_GLOBAL_$(1)_DEBUG_TARGET := $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/$$(patsubst %.a,%$(LIBFT_GLOBAL_ARCHIVE_SUFFIX).a,$$($(1)_DEBUG_TARGET))
LIBFT_GLOBAL_$(1)_SOURCES := $$(addprefix $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/,$$($(1)_SOURCES))
LIBFT_GLOBAL_$(1)_MM_SOURCES := $$(addprefix $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/,$$($(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_CPP_FLAGS := $$($(1)_CPP_FLAGS)
LIBFT_GLOBAL_$(1)_MM_FLAGS := $$($(1)_MM_FLAGS)
LIBFT_GLOBAL_$(1)_C_FLAGS := $$(subst -I$$($(1)_LUA_VENDOR_DIR),-I$(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/$$($(1)_LUA_VENDOR_DIR),$$($(1)_C_FLAGS))
LIBFT_GLOBAL_MANIFEST_LOAD :=
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_LOAD_GLOBAL_MANIFEST,$(module_name))))

LIBFT_GLOBAL_RELEASE_ARCHIVES := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TARGET))
LIBFT_GLOBAL_DEBUG_ARCHIVES := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_DEBUG_TARGET))
LIBFT_GLOBAL_TEST_ARCHIVES := $(patsubst %.a,%_test.a,$(LIBFT_GLOBAL_RELEASE_ARCHIVES))
LIBFT_GLOBAL_TEST_DEBUG_ARCHIVES := $(patsubst %.a,%_test_debug.a,$(LIBFT_GLOBAL_RELEASE_ARCHIVES))

define LIBFT_DEFINE_GLOBAL_MODULE
LIBFT_GLOBAL_$(1)_RELEASE_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_RELEASE_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_RELEASE_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(LIBFT_GLOBAL_$(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_DEBUG_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_DEBUG_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_DEBUG_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(LIBFT_GLOBAL_$(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_TEST_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_TEST_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_TEST_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o,$$(LIBFT_GLOBAL_$(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_TEST_DEBUG_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_TEST_DEBUG_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_TEST_DEBUG_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o,$$(LIBFT_GLOBAL_$(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS := $$(LIBFT_GLOBAL_$(1)_RELEASE_CPP_OBJECTS) $$(LIBFT_GLOBAL_$(1)_RELEASE_C_OBJECTS) $$(LIBFT_GLOBAL_$(1)_RELEASE_MM_OBJECTS)
LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS := $$(LIBFT_GLOBAL_$(1)_DEBUG_CPP_OBJECTS) $$(LIBFT_GLOBAL_$(1)_DEBUG_C_OBJECTS) $$(LIBFT_GLOBAL_$(1)_DEBUG_MM_OBJECTS)
LIBFT_GLOBAL_$(1)_TEST_OBJECTS := $$(LIBFT_GLOBAL_$(1)_TEST_CPP_OBJECTS) $$(LIBFT_GLOBAL_$(1)_TEST_C_OBJECTS) $$(LIBFT_GLOBAL_$(1)_TEST_MM_OBJECTS)
LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS := $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_CPP_OBJECTS) $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_C_OBJECTS) $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_MM_OBJECTS)
LIBFT_GLOBAL_$(1)_RELEASE_DIRECTORIES := $$(sort $$(patsubst %/,%,$$(dir $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS))))
LIBFT_GLOBAL_$(1)_DEBUG_DIRECTORIES := $$(sort $$(patsubst %/,%,$$(dir $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS))))
LIBFT_GLOBAL_$(1)_TEST_DIRECTORIES := $$(sort $$(patsubst %/,%,$$(dir $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS))))
LIBFT_GLOBAL_$(1)_TEST_DEBUG_DIRECTORIES := $$(sort $$(patsubst %/,%,$$(dir $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS))))
LIBFT_GLOBAL_$(1)_RELEASE_DEPS := $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS:.o=.d)
LIBFT_GLOBAL_$(1)_DEBUG_DEPS := $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS:.o=.d)
LIBFT_GLOBAL_$(1)_TEST_DEPS := $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS:.o=.d)
LIBFT_GLOBAL_$(1)_TEST_DEBUG_DEPS := $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS:.o=.d)
LIBFT_GLOBAL_$(1)_MANIFEST := $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk
LIBFT_GLOBAL_$(1)_MANIFEST_FINGERPRINT := $(shell cksum $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk | awk '{print $$1 "_" $$2}')
LIBFT_GLOBAL_$(1)_MANIFEST_STAMP := $$(LIBFT_GLOBAL_ROOT)/manifests/$(1)-$$(LIBFT_GLOBAL_$(1)_MANIFEST_FINGERPRINT).stamp

# Keep manifest invalidation content-based rather than relying on filesystem
# timestamp granularity.  A changed manifest selects a new stamp path, while
# an unchanged manifest keeps the same prerequisite and remains incremental.
$$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP): $$(LIBFT_GLOBAL_$(1)_MANIFEST)
	@$$(MKDIR) $$(dir $$@)
	@cp $$< $$@

$$(LIBFT_GLOBAL_$(1)_TARGET): $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@.tmp
	@$$(AR) $$(ARFLAGS) $$@.tmp $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@.tmp $$@
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET): $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@.tmp
	@$$(AR) $$(ARFLAGS) $$@.tmp $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@.tmp $$@
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(patsubst %.a,%_test.a,$$(LIBFT_GLOBAL_$(1)_TARGET)): $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@.tmp
	@$$(AR) $$(ARFLAGS) $$@.tmp $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@.tmp $$@
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(patsubst %.a,%_test_debug.a,$$(LIBFT_GLOBAL_$(1)_TARGET)): $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@.tmp
	@$$(AR) $$(ARFLAGS) $$@.tmp $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@.tmp $$@
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS): | $$(LIBFT_GLOBAL_$(1)_RELEASE_DIRECTORIES)
$$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS): | $$(LIBFT_GLOBAL_$(1)_DEBUG_DIRECTORIES)
$$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS): | $$(LIBFT_GLOBAL_$(1)_TEST_DIRECTORIES)
$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS): | $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_DIRECTORIES)
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_GLOBAL_MODULE,$(module_name))))

LIBFT_GLOBAL_RELEASE_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_RELEASE_OBJECTS))
LIBFT_GLOBAL_DEBUG_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_DEBUG_OBJECTS))
LIBFT_GLOBAL_TEST_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TEST_OBJECTS))
LIBFT_GLOBAL_TEST_DEBUG_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TEST_DEBUG_OBJECTS))
LIBFT_GLOBAL_RELEASE_DEPENDENCY_FILES := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_RELEASE_DEPS))
LIBFT_GLOBAL_DEBUG_DEPENDENCY_FILES := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_DEBUG_DEPS))
LIBFT_GLOBAL_TEST_DEPENDENCY_FILES := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TEST_DEPS))
LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCY_FILES := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TEST_DEBUG_DEPS))

# Parsing every configuration's dependency database on every invocation is
# especially expensive through Git Bash on Windows.  Standalone test/debug
# goals only need their active object family.  An embedded graph always keeps
# release dependencies available because the parent application links the
# selected Libft release/debug archive through the same graph.
LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_RELEASE_DEPENDENCY_FILES)
ifeq ($(strip $(LIBFT_GLOBAL_GRAPH_PREFIX)),)
    ifneq ($(filter debug global-debug,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_DEBUG_DEPENDENCY_FILES)
    endif
    ifneq ($(filter both re_both,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_RELEASE_DEPENDENCY_FILES) \
            $(LIBFT_GLOBAL_DEBUG_DEPENDENCY_FILES)
    endif
    ifneq ($(filter tests global-tests test-executable run-tests asan-tests run-asan-tests ubsan-tests run-ubsan-tests asan-ubsan-tests run-asan-ubsan-tests re-tests,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_TEST_DEPENDENCY_FILES)
    endif
    ifneq ($(filter debug-tests run-debug-tests,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCY_FILES)
    endif
endif

LIBFT_GLOBAL_CPP_COMPILE_FLAGS := $(COMPILE_FLAGS)
LIBFT_GLOBAL_TEST_COMPILE_FLAGS := $(COMPILE_FLAGS) -DLIBFT_TEST_BUILD
LIBFT_GLOBAL_DEBUG_COMPILE_FLAGS := $(COMPILE_FLAGS) -DDEBUG=1
LIBFT_GLOBAL_TEST_DEBUG_COMPILE_FLAGS := $(COMPILE_FLAGS) -DLIBFT_TEST_BUILD -DDEBUG=1

define LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" compile libft $(1) "$$<" || true; \
	else \
		printf '\033[1;36m[LIBFT][$(1)] Compiling %s\033[0m\n' "$$<"; \
	fi
endef

define LIBFT_DEFINE_CPP_RULES
$$(LIBFT_GLOBAL_$(1)_RELEASE_CPP_OBJECTS): $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_CPP_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_CPP_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_CPP_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_CPP_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_CPP_RULES,$(module_name))))

define LIBFT_DEFINE_C_RULES
$$(LIBFT_GLOBAL_$(1)_RELEASE_C_OBJECTS): $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_C_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DDEBUG=1 -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_C_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DLIBFT_TEST_BUILD -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_C_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DLIBFT_TEST_BUILD -DDEBUG=1 -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_C_RULES,$(module_name))))

define LIBFT_DEFINE_MM_RULES
$$(LIBFT_GLOBAL_$(1)_RELEASE_MM_OBJECTS): $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_CPP_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_MM_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_MM_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_MM_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_MM_RULES,$(module_name))))

LIBFT_GLOBAL_MODULE_DIRECTORIES := $(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),\
	$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(module_name) \
	$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(module_name) \
	$(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(module_name) \
	$(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(module_name))
LIBFT_GLOBAL_DIRECTORIES := $(sort $(LIBFT_GLOBAL_MODULE_DIRECTORIES) \
	$(patsubst %/,%,$(dir $(LIBFT_GLOBAL_RELEASE_OBJECTS) \
	$(LIBFT_GLOBAL_DEBUG_OBJECTS) $(LIBFT_GLOBAL_TEST_OBJECTS) \
	$(LIBFT_GLOBAL_TEST_DEBUG_OBJECTS))))

$(LIBFT_GLOBAL_DIRECTORIES):
	@$(MKDIR) $@

# Dependency files are part of the graph for every goal, including direct
# module-wrapper targets.  Restricting them to aggregate goals makes a command
# such as `make Modules/Basic/Basic.a` miss a private-header change.
-include $(wildcard $(LIBFT_GLOBAL_DEPENDENCY_FILES))
