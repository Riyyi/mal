#include <csignal> // std::signal
#include <cstdlib> // std::exit
#include <string>
#include <string_view>

#include "ruc/argparser.h"
#include "ruc/format/color.h"

#include "ast.h"
#include "environment.h"
#include "error.h"
#include "eval.h"
#include "forward.h"
#include "lexer.h"
#include "printer.h"
#include "reader.h"
#include "readline.h"
#include "settings.h"

#if 0
static blaze::EnvironmentPtr s_outer_env = blaze::Environment::create();

namespace blaze {

auto read(std::string_view input) -> ValuePtr
{
	Lexer lexer(input);
	lexer.tokenize();
	if (Settings::the().get("dump-lexer") == "1") {
		lexer.dump();
	}

	Reader reader(std::move(lexer.tokens()));
	reader.read();
	if (Settings::the().get("dump-reader") == "1") {
		reader.dump();
	}

	return reader.node();
}

auto eval(ValuePtr ast, EnvironmentPtr env) -> ValuePtr
{
	Eval eval(ast, env);
	eval.eval();

	return eval.ast();
}

} // namespace blaze

auto print(blaze::ValuePtr exp) -> std::string
{
	blaze::Printer printer;

	return printer.print(exp);
}

auto rep(std::string_view input) -> std::string
{
	blaze::Error::the().clearErrors();
	blaze::Error::the().setInput(input);

	return print(blaze::eval(blaze::read(input), s_outer_env));
}

static auto cleanup(int signal) -> void
{
	print("\033[0m\n");
	std::exit(signal);
}

auto main(int argc, char* argv[]) -> int
{
	bool dump_lexer = false;
	bool dump_reader = false;
	bool pretty_print = false;
	std::string_view history_path = "~/.mal-history";

	// CLI arguments
	ruc::ArgParser arg_parser;
	arg_parser.addOption(dump_lexer, 'l', "dump-lexer", nullptr, nullptr);
	arg_parser.addOption(dump_reader, 'r', "dump-reader", nullptr, nullptr);
	arg_parser.addOption(pretty_print, 'c', "color", nullptr, nullptr);
	arg_parser.addOption(history_path, 'h', "history", nullptr, nullptr, nullptr, ruc::ArgParser::Required::Yes);
	arg_parser.parse(argc, argv);

	// Set settings
	blaze::Settings::the().set("dump-lexer", dump_lexer ? "1" : "0");
	blaze::Settings::the().set("dump-reader", dump_reader ? "1" : "0");
	blaze::Settings::the().set("pretty-print", pretty_print ? "1" : "0");

	// Signal callbacks
	std::signal(SIGINT, cleanup);
	std::signal(SIGTERM, cleanup);

	installFunctions(s_outer_env);

	blaze::Readline readline(pretty_print, history_path);

	std::string input;
	while (readline.get(input)) {
		if (pretty_print) {
			print("\033[0m");
		}
		print("{}\n", rep(input));
	}

	if (pretty_print) {
		print("\033[0m");
	}

	return 0;
}
#endif
