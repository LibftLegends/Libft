YAML_TARGET := YAML.a
YAML_DEBUG_TARGET := YAML_debug.a

YAML_SOURCES := yaml_reader.cpp \
        yaml_value.cpp \
        yaml_reader_utils.cpp \
        yaml_writer.cpp \
        yaml_dom_bridge.cpp \
        yaml_serializer.cpp

YAML_HEADERS := yaml.hpp yaml_dom_bridge.hpp
