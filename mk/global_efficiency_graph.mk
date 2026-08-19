# Canonical performance-test graph.  Its object and archive roots are kept
# separate from release, debug, and normal test builds because OPT_LEVEL and
# FT_EFFICIENCY_BUILD change the ABI/optimization configuration.

EFFICIENCY_OPT_LEVEL ?= 3
LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES := Basic Compatebility Debug Errno CMA \
	SCMA System_utils Printf PThread CPP_class Time Sink
LIBFT_GLOBAL_EFFICIENCY_CONFIG_FINGERPRINT := $(shell printf '%s\n' \
	'$(CXX)|$(COMPILE_FLAGS)|$(EFFICIENCY_OPT_LEVEL)|FT_EFFICIENCY_BUILD' | \
	cksum | awk '{print $$1}')
LIBFT_GLOBAL_EFFICIENCY_ROOT := build/libft/efficiency_opt$(EFFICIENCY_OPT_LEVEL)_cfg$(LIBFT_GLOBAL_EFFICIENCY_CONFIG_FINGERPRINT)
LIBFT_GLOBAL_EFFICIENCY_COMPILE_FLAGS := $(COMPILE_FLAGS) \
	-Wno-missing-declarations -DFT_EFFICIENCY_BUILD -pthread
LIBFT_GLOBAL_EFFICIENCY_ARCHIVES :=

define LIBFT_GLOBAL_DEFINE_EFFICIENCY_MODULE
LIBFT_GLOBAL_$(1)_EFFICIENCY_CPP_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp,$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)/%.o,$$(filter %.cpp,$$(LIBFT_GLOBAL_$(1)_SOURCES)))
LIBFT_GLOBAL_$(1)_EFFICIENCY_MM_OBJECTS := $$(patsubst $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm,$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)/%.o,$$(LIBFT_GLOBAL_$(1)_MM_SOURCES))
LIBFT_GLOBAL_$(1)_EFFICIENCY_OBJECTS := $$(LIBFT_GLOBAL_$(1)_EFFICIENCY_CPP_OBJECTS) $$(LIBFT_GLOBAL_$(1)_EFFICIENCY_MM_OBJECTS)
LIBFT_GLOBAL_$(1)_EFFICIENCY_ARCHIVE := $$(patsubst %.a,%_opt$(EFFICIENCY_OPT_LEVEL).a,$$(LIBFT_GLOBAL_$(1)_TARGET))

$$(LIBFT_GLOBAL_$(1)_EFFICIENCY_ARCHIVE): $$(LIBFT_GLOBAL_$(1)_EFFICIENCY_OBJECTS) $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk $$(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|Efficiency$(1)|$$@"; else printf '\033[1;35m[LIBFT][Efficiency$(1)] Archiving %s\033[0m\n' "$$@"; fi
	@$$(MKDIR) $$(dir $$@)
	@$$(RM) $$@.tmp
	@$$(AR) $$(ARFLAGS) $$@.tmp $$(LIBFT_GLOBAL_$(1)_EFFICIENCY_OBJECTS)
	@$$(LIBFT_GLOBAL_MV) $$@.tmp $$@
	@printf '\033[1;35m[LIBFT][Efficiency$(1)] Archive ready: %s\033[0m\n' "$$@"
endef

define LIBFT_GLOBAL_DEFINE_EFFICIENCY_CPP_RULE
$$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.cpp $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk | $$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|Efficiency$(1)|$$<"; else printf '\033[1;36m[LIBFT][Efficiency$(1)] Compiling %s\033[0m\n' "$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_EFFICIENCY_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
endef

define LIBFT_GLOBAL_DEFINE_EFFICIENCY_MM_RULE
$$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)/%.o: $(LIBFT_GLOBAL_GRAPH_PREFIX)Modules/$(1)/%.mm $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/modules/$(1).mk | $$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Modules/$(1)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|Efficiency$(1)|$$<"; else printf '\033[1;36m[LIBFT][Efficiency$(1)] Compiling %s\033[0m\n' "$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_EFFICIENCY_COMPILE_FLAGS) $$(LIBFT_GLOBAL_$(1)_CPP_FLAGS) $$(LIBFT_GLOBAL_$(1)_MM_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
endef

$(foreach module_name,$(LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES),$(eval $(call LIBFT_GLOBAL_DEFINE_EFFICIENCY_MODULE,$(module_name))))
$(foreach module_name,$(LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES),$(eval $(call LIBFT_GLOBAL_DEFINE_EFFICIENCY_CPP_RULE,$(module_name))))
$(foreach module_name,$(LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES),$(eval $(call LIBFT_GLOBAL_DEFINE_EFFICIENCY_MM_RULE,$(module_name))))
LIBFT_GLOBAL_EFFICIENCY_ARCHIVES := $(foreach module_name,$(LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_EFFICIENCY_ARCHIVE))
LIBFT_GLOBAL_EFFICIENCY_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_EFFICIENCY_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_EFFICIENCY_OBJECTS))
LIBFT_GLOBAL_EFFICIENCY_DEPENDENCIES := $(LIBFT_GLOBAL_EFFICIENCY_OBJECTS:.o=.d)

LIBFT_GLOBAL_EFFICIENCY_SOURCE_FILES := $(wildcard Test/Efficiency/*.cpp)
LIBFT_GLOBAL_EFFICIENCY_TEST_ROOT := $(LIBFT_GLOBAL_EFFICIENCY_ROOT)/Test/Efficiency
LIBFT_GLOBAL_EFFICIENCY_TEST_OBJECTS := $(patsubst %.cpp,$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/%.o,$(LIBFT_GLOBAL_EFFICIENCY_SOURCE_FILES))
LIBFT_GLOBAL_EFFICIENCY_TEST_DEPENDENCIES := $(LIBFT_GLOBAL_EFFICIENCY_TEST_OBJECTS:.o=.d)
LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE := Test/libft_efficiency_tests$(if $(filter Windows_NT,$(OS)),.exe,)
LIBFT_GLOBAL_EFFICIENCY_LINK_FLAGS := -pthread

ifeq ($(OS),Windows_NT)
LIBFT_GLOBAL_EFFICIENCY_LINK_FLAGS += -lz -lws2_32 -lgdi32 -lwinmm -ldbghelp -lopengl32
else ifeq ($(UNAME_S),Darwin)
LIBFT_GLOBAL_EFFICIENCY_LINK_FLAGS += -lz -framework Cocoa -framework CoreGraphics \
	-framework QuartzCore -framework AudioToolbox -lobjc -lpthread
else
LIBFT_GLOBAL_EFFICIENCY_LINK_FLAGS += -Wl,--allow-multiple-definition -rdynamic -lz -ldl
endif

ifeq ($(UNAME_S),Darwin)
LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_START :=
LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_END :=
else
LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_START := -Wl,--start-group
LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_END := -Wl,--end-group
endif

define LIBFT_GLOBAL_DEFINE_EFFICIENCY_TEST_OBJECT
$(1): $(2) | $(patsubst %/,%,$(dir $(1)))
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|compile|libft|EfficiencyTest|$$<"; else printf '\033[1;36m[LIBFT][EfficiencyTest] Compiling %s\033[0m\n' "$$<"; fi
	@$$(CXX) $$(LIBFT_GLOBAL_EFFICIENCY_COMPILE_FLAGS) -MMD -MP -MF $$(@:.o=.d) -MT $$@ -c $$< -o $$@
endef
$(foreach source_file,$(LIBFT_GLOBAL_EFFICIENCY_SOURCE_FILES),$(eval $(call LIBFT_GLOBAL_DEFINE_EFFICIENCY_TEST_OBJECT,$(patsubst %.cpp,$(LIBFT_GLOBAL_EFFICIENCY_ROOT)/%.o,$(source_file)),$(source_file))))

$(sort $(patsubst %/,%,$(dir $(LIBFT_GLOBAL_EFFICIENCY_OBJECTS) $(LIBFT_GLOBAL_EFFICIENCY_TEST_OBJECTS)))):
	@$(MKDIR) "$@"

$(LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE): $(LIBFT_GLOBAL_EFFICIENCY_TEST_OBJECTS) $(LIBFT_GLOBAL_EFFICIENCY_ARCHIVES) mk/global_efficiency_graph.mk
	@if [ "$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|link|libft|EfficiencyTest|$@"; else printf '\033[1;35m[LIBFT][EfficiencyTest] Linking %s\033[0m\n' "$@"; fi
	@$(MKDIR) "$(dir $@)"
	@sh mk/write_object_response_file.sh "$@.rsp" "$(LIBFT_GLOBAL_EFFICIENCY_TEST_ROOT)" "Test/Efficiency"
	@printf '%s\n' "$(LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_START) $(LIBFT_GLOBAL_EFFICIENCY_ARCHIVES) $(LIBFT_GLOBAL_EFFICIENCY_ARCHIVE_GROUP_END)" >> "$@.rsp"
	@$(CXX) $(LIBFT_GLOBAL_EFFICIENCY_COMPILE_FLAGS) -o $@ @$@.rsp $(LIBFT_GLOBAL_EFFICIENCY_LINK_FLAGS)
	@$(RM) $@.rsp
	@printf '\033[1;35m[LIBFT][EfficiencyTest] Link ready: %s\033[0m\n' "$@"

ifneq ($(filter performance_benchmarks Efficiency run_performance_benchmarks run_Efficiency,$(MAKECMDGOALS)),)
-include $(wildcard $(LIBFT_GLOBAL_EFFICIENCY_DEPENDENCIES) $(LIBFT_GLOBAL_EFFICIENCY_TEST_DEPENDENCIES))
endif
