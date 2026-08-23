Geometry_TARGET := geometry.a
Geometry_DEBUG_TARGET := geometry_debug.a

Geometry_SOURCES := geometry_aabb.cpp \
        geometry_3d.cpp \
        geometry_collision.cpp \
        geometry_circle.cpp \
        geometry_lock_tracker.cpp \
        geometry_sphere.cpp

Geometry_HEADERS := geometry.hpp aabb.hpp circle.hpp geometry_3d.hpp geometry_lock_tracker.hpp sphere.hpp
