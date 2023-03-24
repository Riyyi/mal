/*
 * Copyright (C) 2023 Riyyi
 *
 * SPDX-License-Identifier: MIT
 */

#include "lexer.h"
#include "ruc/format/print.h"

#include "error.h"
#include "printer.h"
#include "types.h"

namespace blaze {

Printer::Printer(ASTNode* node)
	: m_node(node)
{
}

Printer::~Printer()
{
	delete m_node;
}

// -----------------------------------------

void Printer::dump()
{
	if (Error::the().hasAnyError()) {
		dumpError();
		return;
	}

	if (m_node == nullptr) {
		return;
	}

	dumpImpl(m_node);
	print("\n");
}

void Printer::dumpImpl(ASTNode* node)
{
	auto printSpacing = [this]() -> void {
		if (!m_firstNode && !m_previousNodeIsList) {
			print(" ");
		}
	};

	if (is<List>(node)) {
		printSpacing();
		print("(");
		m_firstNode = false;
		m_previousNodeIsList = true;
		List* list = static_cast<List*>(node);
		for (size_t i = 0; i < list->nodes().size(); ++i) {
			dumpImpl(list->nodes()[i]);
			m_previousNodeIsList = false;
		}
		print(")");
	}
	else if (is<Vector>(node)) {
		printSpacing();
		print("[");
		m_firstNode = false;
		m_previousNodeIsList = true;
		Vector* vector = static_cast<Vector*>(node);
		for (size_t i = 0; i < vector->nodes().size(); ++i) {
			dumpImpl(vector->nodes()[i]);
			m_previousNodeIsList = false;
		}
		print("]");
	}
	else if (is<HashMap>(node)) {
		printSpacing();
		print("{{");
		m_firstNode = false;
		m_previousNodeIsList = true;
		HashMap* hash_map = static_cast<HashMap*>(node);
		for (auto element : hash_map->elements()) {
			print("{} ", element.first.front() == 0x7f ? ":" + element.first.substr(1) : element.first); // 127
			dumpImpl(element.second);
			m_previousNodeIsList = false;
		}
		print("}}");
	}
	else if (is<String>(node)) {
		printSpacing();
		print("{}", static_cast<String*>(node)->data());
	}
	else if (is<Keyword>(node)) {
		printSpacing();
		print(":{}", static_cast<Keyword*>(node)->keyword().substr(1));
	}
	else if (is<Number>(node)) {
		printSpacing();
		print("{}", static_cast<Number*>(node)->number());
	}
	else if (is<Symbol>(node)) {
		printSpacing();
		print("{}", static_cast<Symbol*>(node)->symbol());
	}
}

void Printer::dumpError()
{
	print("Error: ");
	if (Error::the().hasTokenError()) {
		Token error = Error::the().tokenError();
		print("{}", error.symbol);
	}
	else if (Error::the().hasOtherError()) {
		std::string error = Error::the().otherError();
		print("{}", error);
	}
	print("\n");
}

} // namespace blaze
