TARGET         := bmp.a
DEBUG_TARGET   := bmp_debug.a

SRCS := bmp.cpp

HEADERS := bmp.hpp

include ../../mk/compiler_flags.mk
include ../../mk/modules/BMP.mk

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(RM) $@
	@$(AR) $(ARFLAGS) $@ $^
	@printf '        [%s] Module archive ready (%d files)\n' "$(MODULE_NAME)" "$(TOTAL_SRCS)"

$(OBJDIR)/%.o: %.cpp $(HEADERS) | $(OBJDIR)
	@$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@
	@$(FIXDEP)

debug: CXXFLAGS += -DDEBUG=1
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	@$(RM) $@
	@$(AR) $(ARFLAGS) $@ $^

$(DEBUG_OBJDIR)/%.o: %.cpp $(HEADERS) | $(DEBUG_OBJDIR)
	@$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@
	@$(FIXDEP)

$(OBJDIR) $(DEBUG_OBJDIR):
	@$(MKDIR) $@

clean:
	@$(RM) $(CLEAN_FILES)
	@$(RMDIR) $(CLEAN_DIRS)

fclean: clean
	@$(RM) $(TARGET) $(DEBUG_TARGET) $(TEST_ARCHIVES)

re: fclean all

both: all debug

%_test.a: FORCE
	@$(MAKE) TARGET="$@" $(SUBMAKE_OVERRIDES) COMPILE_FLAGS="$(COMPILE_FLAGS) -DLIBFT_TEST_BUILD" all

-include $(DEPS) $(DEBUG_DEPS)

.PHONY: all clean fclean re debug both FORCE
