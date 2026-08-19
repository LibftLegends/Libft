Parser_TARGET := parser.a
Parser_DEBUG_TARGET := parser_debug.a

Parser_SOURCES :=         parser_document_backend.cpp \
                parser_dom_node.cpp \
                parser_dom_document.cpp \
                parser_dom_validation_report.cpp \
                parser_dom_schema.cpp \
                parser_dom_find_path.cpp

Parser_HEADERS := document_backend.hpp dom.hpp
