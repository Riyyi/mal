/*
 * Copyright (C) 2023 Riyyi
 *
 * SPDX-License-Identifier: MIT
 */

#include <iterator> // std::next, std::prev
#include <list>
#include <memory>
#include <string>

#include "ast.h"
#include "environment.h"
#include "error.h"
#include "eval.h"
#include "forward.h"
#include "types.h"
#include "util.h"

namespace blaze {

static ValuePtr evalQuasiQuoteImpl(ValuePtr ast);

// (def! x 2)
ValuePtr Eval::evalDef(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("def!", nodes.size(), 2);

	// First argument needs to be a Symbol
	VALUE_CAST(symbol, Symbol, nodes.front());

	// Eval second argument
	m_ast = *std::next(nodes.begin());
	m_env = env;
	ValuePtr value = evalImpl();

	// Dont overwrite symbols after an error
	if (Error::the().hasAnyError()) {
		return nullptr;
	}

	// Modify existing environment
	return env->set(symbol->symbol(), value);
}

// (defmacro! x (fn* (x) x))
ValuePtr Eval::evalDefMacro(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("defmacro!", nodes.size(), 2);

	// First argument needs to be a Symbol
	VALUE_CAST(symbol, Symbol, nodes.front());

	// Eval second argument
	m_ast = *std::next(nodes.begin());
	m_env = env;
	ValuePtr value = evalImpl();
	VALUE_CAST(lambda, Lambda, value);

	// Dont overwrite symbols after an error
	if (Error::the().hasAnyError()) {
		return nullptr;
	}

	// Modify existing environment
	return env->set(symbol->symbol(), makePtr<Macro>(*lambda));
}

// (fn* (x) x)
ValuePtr Eval::evalFn(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("fn*", nodes.size(), 2);

	// First element needs to be a List or Vector
	VALUE_CAST(collection, Collection, nodes.front());
	const auto& collection_nodes = collection->nodes();

	std::vector<std::string> bindings;
	bindings.reserve(collection_nodes.size());
	for (const auto& node : collection_nodes) {
		// All nodes need to be a Symbol
		VALUE_CAST(symbol, Symbol, node);
		bindings.push_back(symbol->symbol());
	}

	// TODO: Remove limitation of 3 arguments
	// Wrap all other nodes in list and add that as lambda body
	return makePtr<Lambda>(bindings, *std::next(nodes.begin()), env);
}

ValuePtr Eval::evalMacroExpand(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("macroexpand", nodes.size(), 1);

	return macroExpand(nodes.front(), env);
}

// (quasiquoteexpand x)
ValuePtr Eval::evalQuasiQuoteExpand(const ValueVector& nodes)
{
	CHECK_ARG_COUNT_IS("quasiquoteexpand", nodes.size(), 1);

	return evalQuasiQuoteImpl(nodes.front());
}

// (quote x)
ValuePtr Eval::evalQuote(const ValueVector& nodes)
{
	CHECK_ARG_COUNT_IS("quote", nodes.size(), 1);

	return nodes.front();
}

// (try* x (catch* y z))
ValuePtr Eval::evalTry(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_AT_LEAST("try*", nodes.size(), 1);

	// Try 'x'
	m_ast = nodes.front();
	m_env = env;
	auto result = evalImpl();

	if (!Error::the().hasAnyError()) {
		return result;
	}
	if (nodes.size() == 1) {
		return nullptr;
	}

	// Catch

	// Get the error message
	auto error = (Error::the().hasOtherError())
	                 ? makePtr<String>(Error::the().otherError())
	                 : Error::the().exception();
	Error::the().clearErrors();

	VALUE_CAST(catch_list, List, nodes.back());
	const auto& catch_nodes = catch_list->nodes();
	CHECK_ARG_COUNT_IS("catch*", catch_nodes.size() - 1, 2);

	VALUE_CAST(catch_symbol, Symbol, catch_nodes.front());
	if (catch_symbol->symbol() != "catch*") {
		Error::the().add("catch block must begin with catch*");
		return nullptr;
	}

	VALUE_CAST(catch_binding, Symbol, (*std::next(catch_nodes.begin())));

	// Create new Environment that binds 'y' to the value of the exception
	auto catch_env = Environment::create(env);
	catch_env->set(catch_binding->symbol(), error);

	// Evaluate 'z' using the new Environment
	m_ast = catch_nodes.back();
	m_env = catch_env;
	return evalImpl();
}

// -----------------------------------------

// (do 1 2 3)
void Eval::evalDo(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_AT_LEAST("do", nodes.size(), 1, void());

	// Evaluate all nodes except the last
	for (auto it = nodes.begin(); it != std::prev(nodes.end(), 1); ++it) {
		m_ast = *it;
		m_env = env;
		evalImpl();
	}

	// Eval last node
	m_ast = nodes.back();
	m_env = env;
	return; // TCO
}

// (if x true false)
void Eval::evalIf(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_BETWEEN("if", nodes.size(), 2, 3, void());

	auto first_argument = *nodes.begin();
	auto second_argument = *std::next(nodes.begin());
	auto third_argument = (nodes.size() == 3) ? *std::next(std::next(nodes.begin())) : makePtr<Constant>(Constant::Nil);

	m_ast = first_argument;
	m_env = env;
	auto first_evaluated = evalImpl();
	if (!is<Constant>(first_evaluated.get())
	    || std::static_pointer_cast<Constant>(first_evaluated)->state() == Constant::True) {
		m_ast = second_argument;
		m_env = env;
		return; // TCO
	}

	m_ast = third_argument;
	m_env = env;
	return; // TCO
}

// (let* (x 1) x)
void Eval::evalLet(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("let*", nodes.size(), 2, void());

	// First argument needs to be a List or Vector
	VALUE_CAST(bindings, Collection, nodes.front(), void());
	const auto& binding_nodes = bindings->nodes();

	// List or Vector needs to have an even number of elements
	CHECK_ARG_COUNT_EVEN("bindings", binding_nodes.size(), void());

	// Create new environment
	auto let_env = Environment::create(env);

	for (auto it = binding_nodes.begin(); it != binding_nodes.end(); std::advance(it, 2)) {
		// First element needs to be a Symbol
		VALUE_CAST(elt, Symbol, (*it), void());

		std::string key = elt->symbol();
		m_ast = *std::next(it);
		m_env = let_env;
		ValuePtr value = evalImpl();
		let_env->set(key, value);
	}

	// TODO: Remove limitation of 3 arguments
	// Eval all arguments in this new env, return last sexp of the result
	m_ast = *std::next(nodes.begin());
	m_env = let_env;
	return; // TCO
}

// -----------------------------------------

static bool isSymbol(ValuePtr value, const std::string& symbol)
{
	if (!is<Symbol>(value.get())) {
		return false;
	}

	auto valueSymbol = std::static_pointer_cast<Symbol>(value)->symbol();

	if (valueSymbol != symbol) {
		return false;
	}

	return true;
}

static ValuePtr startsWith(ValuePtr ast, const std::string& symbol)
{
	if (!is<List>(ast.get())) {
		return nullptr;
	}

	const auto& nodes = std::static_pointer_cast<List>(ast)->nodes();

	if (nodes.empty() || !isSymbol(nodes.front(), symbol)) {
		return nullptr;
	}

	// Dont count the Symbol as part of the arguments
	CHECK_ARG_COUNT_IS(symbol, nodes.size() - 1, 1);

	return *std::next(nodes.begin());
}

static ValuePtr evalQuasiQuoteImpl(ValuePtr ast)
{
	if (is<HashMap>(ast.get()) || is<Symbol>(ast.get())) {
		return makePtr<List>(makePtr<Symbol>("quote"), ast);
	}

	if (!is<Collection>(ast.get())) {
		return ast;
	}

	// `~2 or `(unquote 2)
	const auto unquote = startsWith(ast, "unquote"); // x
	if (unquote) {
		return unquote;
	}

	// `~@(list 2 2 2) or `(splice-unquote (list 2 2 2))
	const auto splice_unquote = startsWith(ast, "splice-unquote"); // (list 2 2 2)
	if (splice_unquote) {
		return splice_unquote;
	}

	ValuePtr result = makePtr<List>();

	const auto& nodes = std::static_pointer_cast<Collection>(ast)->nodes();

	// `() or `(1 ~2 3) or `(1 ~@(list 2 2 2) 3)
	for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
		const auto& elt = *it;

		const auto splice_unquote = startsWith(elt, "splice-unquote"); // (list 2 2 2)
		if (splice_unquote) {
			// (cons 1 (concat (list 2 2 2) (cons 3 ())))
			result = makePtr<List>(makePtr<Symbol>("concat"), splice_unquote, result);
			continue;
		}

		// (cons 1 (cons 2 (cons 3 ())))
		result = makePtr<List>(makePtr<Symbol>("cons"), evalQuasiQuoteImpl(elt), result);
	}

	if (is<List>(ast.get())) {
		return result;
	}

	// Wrap result in (vec) for Vector types
	return makePtr<List>(makePtr<Symbol>("vec"), result);
}

// (quasiquote x)
void Eval::evalQuasiQuote(const ValueVector& nodes, EnvironmentPtr env)
{
	CHECK_ARG_COUNT_IS("quasiquote", nodes.size(), 1, void());

	auto result = evalQuasiQuoteImpl(nodes.front());

	m_ast = result;
	m_env = env;
	return; // TCO
}

// -----------------------------------------

} // namespace blaze
