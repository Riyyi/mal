/*
 * Copyright (C) 2023 Riyyi
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef> // size_t
#include <vector>

#include "ast.h"
#include "lexer.h"

namespace blaze {

// Parsing -> creates AST
class Reader {
public:
	Reader(std::vector<Token>&& tokens) noexcept;
	virtual ~Reader();

	void read();

	void dump();

	ASTNode* node() { return m_node; }

private:
	bool isEOF() const;
	Token peek() const;
	Token consume();
	void ignore();

	ASTNode* readImpl();
	ASTNode* readList();
	ASTNode* readString();
	ASTNode* readValue();

	void dumpImpl(ASTNode* node);

	size_t m_index { 0 };
	size_t m_indentation { 0 };
	std::vector<Token> m_tokens;

	ASTNode* m_node { nullptr };
};

} // namespace blaze
