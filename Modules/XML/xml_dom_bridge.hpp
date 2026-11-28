#ifndef XML_DOM_BRIDGE_HPP
#define XML_DOM_BRIDGE_HPP

#include "xml.hpp"
#include "../Parser/dom.hpp"

// Each conversion replaces the destination tree only after the new tree is ready.
int32_t xml_document_to_dom(const xml_document &document, ft_dom_document &dom) noexcept;
int32_t xml_document_from_dom(const ft_dom_document &dom, xml_document &document) noexcept;

#endif
