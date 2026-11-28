# Compatibility wrapper for the canonical root GNU Make graph.

MODULE_NAME := $(notdir $(CURDIR))
include ../../mk/modules/$(MODULE_NAME).mk

MODULE_ARCHIVE := Modules/$(MODULE_NAME)/$($(MODULE_NAME)_TARGET)
MODULE_DEBUG_ARCHIVE := Modules/$(MODULE_NAME)/$($(MODULE_NAME)_DEBUG_TARGET)

.PHONY: all debug clean fclean re

all:
	+$(MAKE) -C ../.. $(MODULE_ARCHIVE)

debug:
	+$(MAKE) -C ../.. $(MODULE_DEBUG_ARCHIVE)

clean:
	+$(MAKE) -C ../.. clean

fclean: clean

re:
	+$(MAKE) -C ../.. clean
	+$(MAKE) -C ../.. $(MODULE_ARCHIVE)
