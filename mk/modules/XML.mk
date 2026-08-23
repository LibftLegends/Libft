XML_TARGET         := XMLParser.a
XML_DEBUG_TARGET   := XMLParser_debug.a

XML_SOURCES := xml_document.cpp \
        xml_node_thread_safety.cpp \
        xml_dom_bridge.cpp \
        xml_serializer.cpp

XML_HEADERS := xml.hpp xml_document.hpp xml_dom_bridge.hpp
