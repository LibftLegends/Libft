# Canonical single-process Libft graph.
#
# This file is included by the Libft root Makefile.  Module manifests are
# loaded one at a time and their values are copied into namespaced variables so
# every module can coexist in one GNU Make database.

LIBFT_GLOBAL_GRAPH_PREFIX ?=
LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS ?= \
	$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/build_config.mk \
	$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/global_graph.mk
LIBFT_GLOBAL_ARCHIVE_SUFFIX ?= $(LIBFT_ARCHIVE_SUFFIX)
# Host and compiler target are part of the object identity. This prevents a
# Windows build and WSL/Linux build using the same `g++` command name from
# sharing objects in the same checkout.
LIBFT_GLOBAL_CONFIG_FINGERPRINT := $(shell printf '%s\n' \
	'$(LIBFT_TARGET_PLATFORM)|$(shell $(CXX) -dumpmachine 2>/dev/null)|$(CXX)|$(CC)|$(COMPILE_FLAGS)|$(CFLAGS)|$(CPPFLAGS)|$(SANITIZERS)|$(OPT_LEVEL)|$(BUILD_OUTPUT_SUFFIX)|$(LIBFT_GLOBAL_ARCHIVE_SUFFIX)' \
	| cksum | awk '{print $$1}')
LIBFT_GLOBAL_CONFIG ?= standalone$(BUILD_OUTPUT_SUFFIX)_cfg$(LIBFT_GLOBAL_CONFIG_FINGERPRINT)
LIBFT_GLOBAL_ROOT ?= $(LIBFT_GLOBAL_GRAPH_PREFIX)build/libft/$(LIBFT_GLOBAL_CONFIG)
LIBFT_GLOBAL_RELEASE_ROOT := $(LIBFT_GLOBAL_ROOT)/release
LIBFT_GLOBAL_DEBUG_ROOT := $(LIBFT_GLOBAL_ROOT)/debug
LIBFT_GLOBAL_TEST_ROOT := $(LIBFT_GLOBAL_ROOT)/test
LIBFT_GLOBAL_TEST_DEBUG_ROOT := $(LIBFT_GLOBAL_ROOT)/test_debug
LIBFT_GLOBAL_CC ?= gcc
LIBFT_GLOBAL_MV ?= mv
# Keep archive staging names stable across the recipe's separate shell
# invocations, while making them unique to this Make process.  This prevents
# concurrent parent builds sharing one checkout from deleting or renaming one
# another's temporary archive.
LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX := .tmp.$(shell printf '%s' $$$$)


LIBFT_GLOBAL_MODULE_NAMES := Basic BMP Advanced Compatebility Debug Errno CMA SCMA \
    GetNextLine DUMB Math Geometry System_utils Printf ReadLine Regex PThread \
    Threading CPP_class Template Buffer CLI Command Config CrossProcess \
    Compression CSV Encryption Crypto Encoding RNG JSon YAML File HTML Time \
    Filesystem XML Storage Networking URI API Application Observability Analytics CardGame Sink \
    Logger Parser Scripting Game Voxel GPGR

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
LIBFT_GLOBAL_$(1)_TEST_ONLY_SOURCES := $$(addprefix $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/,$$($(1)_TEST_ONLY_SOURCES))
LIBFT_GLOBAL_$(1)_MM_SOURCES := $$(addprefix $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/,$$($(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_CPP_FLAGS := $$($(1)_CPP_FLAGS)
LIBFT_GLOBAL_$(1)_MM_FLAGS := $$($(1)_MM_FLAGS)
LIBFT_GLOBAL_$(1)_C_FLAGS := $$($(1)_C_FLAGS)
LIBFT_GLOBAL_MANIFEST_LOAD :=
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_LOAD_GLOBAL_MANIFEST,$(module_name))))

LIBFT_GLOBAL_ALL_RELEASE_ARCHIVES := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_TARGET))
LIBFT_GLOBAL_RELEASE_ARCHIVES := $(LIBFT_GLOBAL_ALL_RELEASE_ARCHIVES)
ifneq ($(filter 1,$(FT_VOX_ANALYTICS)),1)
LIBFT_GLOBAL_RELEASE_ARCHIVES := $(filter-out $(LIBFT_GLOBAL_Analytics_TARGET),$(LIBFT_GLOBAL_RELEASE_ARCHIVES))
endif
LIBFT_GLOBAL_ALL_DEBUG_ARCHIVES := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_DEBUG_TARGET))
LIBFT_GLOBAL_DEBUG_ARCHIVES := $(LIBFT_GLOBAL_ALL_DEBUG_ARCHIVES)
ifneq ($(filter 1,$(FT_VOX_ANALYTICS)),1)
LIBFT_GLOBAL_DEBUG_ARCHIVES := $(filter-out $(LIBFT_GLOBAL_Analytics_DEBUG_TARGET),$(LIBFT_GLOBAL_DEBUG_ARCHIVES))
endif
LIBFT_GLOBAL_TEST_ARCHIVES := $(patsubst %.a,%_test.a,$(LIBFT_GLOBAL_ALL_RELEASE_ARCHIVES))
LIBFT_GLOBAL_TEST_DEBUG_ARCHIVES := $(patsubst %.a,%_test_debug.a,$(LIBFT_GLOBAL_ALL_RELEASE_ARCHIVES))

.PHONY: LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD
LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD:

LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX := .config

define LIBFT_DEFINE_GLOBAL_MODULE
LIBFT_GLOBAL_$(1)_RELEASE_SOURCES := $$(filter-out $$(LIBFT_GLOBAL_$(1)_TEST_ONLY_SOURCES),$$(LIBFT_GLOBAL_$(1)_SOURCES))
LIBFT_GLOBAL_$(1)_RELEASE_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
LIBFT_GLOBAL_$(1)_RELEASE_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
LIBFT_GLOBAL_$(1)_RELEASE_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o,$$(filter %.mm,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
LIBFT_GLOBAL_$(1)_DEBUG_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
LIBFT_GLOBAL_$(1)_DEBUG_C_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.c,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
LIBFT_GLOBAL_$(1)_DEBUG_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o,$$(filter %.mm,$$(LIBFT_GLOBAL_$(1)_RELEASE_SOURCES)))
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
# Make normally trusts an archive's timestamp if its object prerequisites did
# not change.  Detect member deletion at parse time and add a force prerequisite
# only for a corrupt archive, preserving valid no-op builds.
LIBFT_GLOBAL_$(1)_RELEASE_ARCHIVE_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TARGET)" ] && ! sh "$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/check_archive_members.sh" "$$(LIBFT_GLOBAL_$(1)_TARGET)" $$(notdir $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS)) >/dev/null 2>&1; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_DEBUG_ARCHIVE_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET)" ] && ! sh "$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/check_archive_members.sh" "$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET)" $$(notdir $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS)) >/dev/null 2>&1; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_TEST_ARCHIVE := $$(patsubst %.a,%_test.a,$$(LIBFT_GLOBAL_$(1)_TARGET))
LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE := $$(patsubst %.a,%_test_debug.a,$$(LIBFT_GLOBAL_$(1)_TARGET))
LIBFT_GLOBAL_$(1)_TEST_ARCHIVE_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE)" ] && ! sh "$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/check_archive_members.sh" "$$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE)" $$(notdir $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS)) >/dev/null 2>&1; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE)" ] && ! sh "$(LIBFT_GLOBAL_GRAPH_PREFIX)mk/check_archive_members.sh" "$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE)" $$(notdir $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS)) >/dev/null 2>&1; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_RELEASE_ARCHIVE_CONFIG_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TARGET)" ] && [ "$$(cat "$$(LIBFT_GLOBAL_$(1)_TARGET)$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)" 2>/dev/null)" != "$(LIBFT_GLOBAL_CONFIG)" ]; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_DEBUG_ARCHIVE_CONFIG_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET)" ] && [ "$$(cat "$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET)$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)" 2>/dev/null)" != "$(LIBFT_GLOBAL_CONFIG)" ]; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_TEST_ARCHIVE_CONFIG_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE)" ] && [ "$$(cat "$$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE)$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)" 2>/dev/null)" != "$(LIBFT_GLOBAL_CONFIG)" ]; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE_CONFIG_REPAIR := $$(shell if [ -f "$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE)" ] && [ "$$(cat "$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE)$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)" 2>/dev/null)" != "$(LIBFT_GLOBAL_CONFIG)" ]; then printf '%s' LIBFT_GLOBAL_FORCE_ARCHIVE_REBUILD; fi)
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

$$(LIBFT_GLOBAL_$(1)_TARGET): $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS) $$(LIBFT_GLOBAL_$(1)_RELEASE_ARCHIVE_REPAIR) $$(LIBFT_GLOBAL_$(1)_RELEASE_ARCHIVE_CONFIG_REPAIR)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX)
	@$$(AR) $$(ARFLAGS) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$(LIBFT_GLOBAL_$(1)_RELEASE_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$@
	@printf '%s\n' '$(LIBFT_GLOBAL_CONFIG)' > "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$"
	@mv "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$" "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)"
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(LIBFT_GLOBAL_$(1)_DEBUG_TARGET): $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS) $$(LIBFT_GLOBAL_$(1)_DEBUG_ARCHIVE_REPAIR) $$(LIBFT_GLOBAL_$(1)_DEBUG_ARCHIVE_CONFIG_REPAIR)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX)
	@$$(AR) $$(ARFLAGS) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$(LIBFT_GLOBAL_$(1)_DEBUG_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$@
	@printf '%s\n' '$(LIBFT_GLOBAL_CONFIG)' > "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$"
	@mv "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$" "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)"
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE): $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS) $$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE_REPAIR) $$(LIBFT_GLOBAL_$(1)_TEST_ARCHIVE_CONFIG_REPAIR)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX)
	@$$(AR) $$(ARFLAGS) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$(LIBFT_GLOBAL_$(1)_TEST_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$@
	@printf '%s\n' '$(LIBFT_GLOBAL_CONFIG)' > "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$"
	@mv "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$" "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)"
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft $(1) "$$@" || true; \
	else \
		printf '\033[1;35m[LIBFT][$(1)] Archive ready: %s\033[0m\n' "$$@"; \
	fi

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE): $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS) $$(LIBFT_GLOBAL_$(1)_MANIFEST_STAMP) $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS) $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE_REPAIR) $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_ARCHIVE_CONFIG_REPAIR)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|$(1)|$$@"; else printf '\033[1;35m[LIBFT][$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX)
	@$$(AR) $$(ARFLAGS) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_OBJECTS) >/dev/null
	@$$(LIBFT_GLOBAL_MV) $$@$$(LIBFT_GLOBAL_ARCHIVE_TEMP_SUFFIX) $$@
	@printf '%s\n' '$(LIBFT_GLOBAL_CONFIG)' > "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$"
	@mv "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX).tmp.$$$$" "$$@$(LIBFT_GLOBAL_ARCHIVE_CONFIG_MARKER_SUFFIX)"
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
LIBFT_GLOBAL_DEPENDENCY_ROOTS := $(LIBFT_GLOBAL_RELEASE_ROOT)
ifeq ($(strip $(LIBFT_GLOBAL_GRAPH_PREFIX)),)
    ifneq ($(filter debug global-debug,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_DEBUG_DEPENDENCY_FILES)
        LIBFT_GLOBAL_DEPENDENCY_ROOTS := $(LIBFT_GLOBAL_DEBUG_ROOT)
    endif
    ifneq ($(filter both re_both,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_RELEASE_DEPENDENCY_FILES) \
            $(LIBFT_GLOBAL_DEBUG_DEPENDENCY_FILES)
        LIBFT_GLOBAL_DEPENDENCY_ROOTS := $(LIBFT_GLOBAL_RELEASE_ROOT) \
            $(LIBFT_GLOBAL_DEBUG_ROOT)
    endif
    ifneq ($(filter tests global-tests test-executable run-tests asan-tests run-asan-tests ubsan-tests run-ubsan-tests asan-ubsan-tests run-asan-ubsan-tests re-tests Test/libft_tests%,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_TEST_DEPENDENCY_FILES)
        LIBFT_GLOBAL_DEPENDENCY_ROOTS := $(LIBFT_GLOBAL_TEST_ROOT)
    endif
    ifneq ($(filter debug-tests run-debug-tests Test/libft_tests_debug%,$(MAKECMDGOALS)),)
        LIBFT_GLOBAL_DEPENDENCY_FILES := $(LIBFT_GLOBAL_TEST_DEBUG_DEPENDENCY_FILES)
        LIBFT_GLOBAL_DEPENDENCY_ROOTS := $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)
    endif
endif

# GCC and Clang emit absolute Windows dependency paths with an unescaped drive
# colon. Migrate the active depfile family before GNU Make parses it, then
# normalize each newly generated file in its compile recipe below.
LIBFT_GLOBAL_DEPENDENCY_NORMALIZATION := $(foreach dependency_root,$(LIBFT_GLOBAL_DEPENDENCY_ROOTS),$(shell sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh --tree "$(dependency_root)"))

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
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_CPP_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_CPP_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_CPP_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_CPP_RULES,$(module_name))))

define LIBFT_DEFINE_C_RULES
$$(LIBFT_GLOBAL_$(1)_RELEASE_C_OBJECTS): $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_C_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DDEBUG=1 -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_C_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DLIBFT_TEST_BUILD -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_C_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.c | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(LIBFT_GLOBAL_CC) $$(LIBFT_GLOBAL_$(1)_C_FLAGS) -DLIBFT_TEST_BUILD -DDEBUG=1 -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))
endef

$(foreach module_name,$(LIBFT_GLOBAL_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_C_RULES,$(module_name))))

define LIBFT_DEFINE_MM_RULES
$$(LIBFT_GLOBAL_$(1)_RELEASE_MM_OBJECTS): $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_RELEASE_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_CPP_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_DEBUG_MM_OBJECTS): $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_MM_OBJECTS): $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_TEST_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
$(call LIBFT_GLOBAL_COMPILE_PROGRESS_RECIPE,$(1))

$$(LIBFT_GLOBAL_$(1)_TEST_DEBUG_MM_OBJECTS): $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm | $(LIBFT_GLOBAL_TEST_DEBUG_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|$(1)|$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_TEST_DEBUG_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
	@sh $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/normalize_make_depfile.sh "$$(@:.o=.d)"
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
