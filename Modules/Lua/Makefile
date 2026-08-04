include ../../mk/compiler_flags.mk
include ../../mk/modules/Lua.mk

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(RM) $@
	@$(AR) $(ARFLAGS) $@ $^
	@printf '        [%s] Module archive ready (%d files)\n' "$(MODULE_NAME)" $(TOTAL_SRCS)

$(OBJDIR)/%.o: $(LUA_VENDOR_DIR)/%.c | $(OBJDIR)
	@$(CC) $(LUA_CFLAGS) -MMD -MP -c $< -o $@
	@$(FIXDEP)
	@if [ "$(LIBFT_BATCH_OUTPUT)" != "1" ]; then \
		printf '        [%s] Building file %s\n' "$(MODULE_NAME)" "$<"; \
	fi

debug: LUA_CFLAGS += -DDEBUG=1
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	@$(RM) $@
	@$(AR) $(ARFLAGS) $@ $^
	@printf '        [%s Debug] Module archive ready (%d files)\n' "$(MODULE_NAME)" $(TOTAL_SRCS)

$(DEBUG_OBJDIR)/%.o: $(LUA_VENDOR_DIR)/%.c | $(DEBUG_OBJDIR)
	@$(CC) $(LUA_CFLAGS) -DDEBUG=1 -MMD -MP -c $< -o $@
	@$(FIXDEP)
	@if [ "$(LIBFT_BATCH_OUTPUT)" != "1" ]; then \
		printf '        [%s Debug] Building file %s\n' "$(MODULE_NAME)" "$<"; \
	fi

$(OBJDIR) $(DEBUG_OBJDIR):
	@$(MKDIR) $@

clean:
	@$(RMDIR) $(CLEAN_DIRS)

fclean: clean
	@$(RM) $(FCLEAN_FILES)

re:
	@$(MAKE) fclean
	@$(MAKE) all

both: all debug

%_test.a: FORCE
	@$(MAKE) TARGET="$@" $(SUBMAKE_OVERRIDES) all

-include $(OBJS:.o=.d) $(DEBUG_OBJS:.o=.d)

.PHONY: all clean fclean re debug both FORCE

FORCE:
