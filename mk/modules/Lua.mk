Lua_TARGET := Lua.a
Lua_DEBUG_TARGET := Lua_debug.a

Lua_LUA_VENDOR_DIR := vendor/lua-5.4.8
Lua_LUA_SOURCE_NAMES := lapi lauxlib lbaselib lcode lcorolib lctype ldblib \
                    ldebug ldo ldump lfunc lgc linit liolib llex lmathlib \
                    lmem loadlib lobject lopcodes loslib lparser lstate \
                    lstring lstrlib ltable ltablib ltm lundump lutf8lib \
                    lvm lzio
Lua_SOURCES := $(addprefix $(Lua_LUA_VENDOR_DIR)/,$(addsuffix .c,$(Lua_LUA_SOURCE_NAMES)))


OBJS := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(Lua_LUA_SOURCE_NAMES)))
DEBUG_OBJS := $(addprefix $(DEBUG_OBJDIR)/,$(addsuffix .o,$(Lua_LUA_SOURCE_NAMES)))
TOTAL_SRCS := $(words $(Lua_SOURCES))
MODULE_NAME := Lua
CC := gcc

Lua_C_FLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter \
              -DLUA_USE_APICHECK -I$(Lua_LUA_VENDOR_DIR) \
              $(filter -O% -g -fsanitize=% -fno-omit-frame-pointer \
                  -ffunction-sections -fdata-sections,$(COMPILE_FLAGS))

CLEAN_DIRS := $(OBJDIR) $(DEBUG_OBJDIR)
CLEAN_FILES :=
FCLEAN_FILES := $(Lua_TARGET) $(Lua_DEBUG_TARGET) $(Lua_TARGET:.a=_test.a)
