EFF_SRCS := $(wildcard Efficiency/*.cpp)
EFF_TOTAL_SRCS := $(words $(EFF_SRCS))
LUA_TEST_FILES := $(wildcard Lua/*.lua)

ifneq ($(strip $(FILES)),)
SRCS := main.cpp $(FILES)
else
SRCS := main.cpp $(wildcard Test/*.cpp) $(wildcard API/*.cpp)
SRCS := $(filter-out Test/test_template.cpp,$(SRCS))
endif
