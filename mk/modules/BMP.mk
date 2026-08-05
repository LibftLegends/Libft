TARGET         := bmp.a
DEBUG_TARGET   := bmp_debug.a

SRCS := bmp.cpp

HEADERS := bmp.hpp

include $(dir $(lastword $(MAKEFILE_LIST)))common/module_defaults.mk
