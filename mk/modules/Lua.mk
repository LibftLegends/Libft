TARGET := Lua.a
DEBUG_TARGET := Lua_debug.a

LUA_VENDOR_DIR := vendor/lua-5.4.8
LUA_SOURCE_NAMES := lapi lauxlib lbaselib lcode lcorolib lctype ldblib \
                    ldebug ldo ldump lfunc lgc linit liolib llex lmathlib \
                    lmem loadlib lobject lopcodes loslib lparser lstate \
                    lstring lstrlib ltable ltablib ltm lundump lutf8lib \
                    lvm lzio
SRCS := $(addprefix $(LUA_VENDOR_DIR)/,$(addsuffix .c,$(LUA_SOURCE_NAMES)))

include $(dir $(lastword $(MAKEFILE_LIST)))common/module_defaults.mk

OBJS := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(LUA_SOURCE_NAMES)))
DEBUG_OBJS := $(addprefix $(DEBUG_OBJDIR)/,$(addsuffix .o,$(LUA_SOURCE_NAMES)))
TOTAL_SRCS := $(words $(SRCS))
MODULE_NAME := Lua
CC := gcc

LUA_CFLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter \
              -DLUA_USE_APICHECK -I$(LUA_VENDOR_DIR) \
              $(filter -O% -g -fsanitize=% -fno-omit-frame-pointer \
                  -ffunction-sections -fdata-sections,$(COMPILE_FLAGS))

CLEAN_DIRS := $(OBJDIR) $(DEBUG_OBJDIR)
CLEAN_FILES :=
FCLEAN_FILES := $(TARGET) $(DEBUG_TARGET) $(TARGET:.a=_test.a)
