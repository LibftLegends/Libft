MODULE_NAME := $(notdir $(CURDIR))
include ../../mk/modules/$(MODULE_NAME).mk

MODULE_ARCHIVE := Modules/$(MODULE_NAME)/$($(MODULE_NAME)_TARGET)
MODULE_DEBUG_ARCHIVE := Modules/$(MODULE_NAME)/$($(MODULE_NAME)_DEBUG_TARGET)
MODULE_TEST_ARCHIVE := Modules/$(MODULE_NAME)/$(patsubst %.a,%_test.a,$($(MODULE_NAME)_TARGET))

.PHONY: all debug test clean fclean re
all:
	+$(MAKE) -C ../.. $(MODULE_ARCHIVE)
debug:
	+$(MAKE) -C ../.. $(MODULE_DEBUG_ARCHIVE)
test:
	+$(MAKE) -C ../.. $(MODULE_TEST_ARCHIVE)
clean:
	+$(MAKE) -C ../.. clean
fclean: clean
re:
	+$(MAKE) -C ../.. clean
	+$(MAKE) -C ../.. $(MODULE_ARCHIVE)
