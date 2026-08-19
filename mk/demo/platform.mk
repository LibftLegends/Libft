UNAME_S := $(shell uname -s)
LINK_FLAGS :=

ifeq ($(OS),Windows_NT)
    LINK_FLAGS += -lgdi32 -luser32 -lws2_32
else ifeq ($(UNAME_S),Darwin)
    LINK_FLAGS += -framework Cocoa -framework CoreGraphics -framework QuartzCore -framework AudioToolbox -lobjc -lpthread
else
    LINK_FLAGS += -lX11 -lXext -lpthread
endif
