include ../mk/build_config.mk
include ../mk/compiler_flags.mk
include ../mk/demo/config.mk

all: $(TARGET) $(LEGACY_TARGET)

validate-levels:
	@perl tools/demo_validate_levels.pl demo_levels.cpp

$(TARGET): ../Full_Libft.a $(SRCS)
	@printf '        [%s] Linking %s\n' "$(MODULE_NAME)" "$(TARGET)"
	@mkdir -p $(TARGET_DIR)
	@$(CXX) $(COMPILE_FLAGS) $(SRCS) -o $@ ../Full_Libft.a $(LINK_OPT_FLAGS) $(LINK_FLAGS)

$(LEGACY_TARGET): $(TARGET)
	@printf '        [%s] Syncing %s\n' "$(MODULE_NAME)" "$(LEGACY_TARGET)"
	@cp $(TARGET) $(LEGACY_TARGET)

../Full_Libft.a:
	@$(MAKE) -C .. Full_Libft.a OPT_LEVEL=$(OPT_LEVEL)

clean:
	@status=0; \
	if ! $(RM) $(TARGET) $(LEGACY_TARGET); then \
		status=1; \
	fi; \
	if ! $(RMDIR) $(TARGET_DIR); then \
		status=1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '        [%s] clean: failed\n' "$(MODULE_NAME)"; \
		exit 1; \
	fi

fclean: clean
	@if [ -z "$(LIBFT_PARENT_CLEAN)" ]; then \
		$(MAKE) -C .. fclean; \
	fi

re:
	@$(MAKE) fclean
	@$(MAKE) all

.PHONY: all clean fclean re validate-levels
