#include "config.h"

#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "expressionwrapper.h"
#include "mruby++.h"

//! \brief Parses and represents command line arguments.
struct ProgramOptions
{
    //! \brief Parse command line arguments and return a new \c ProgramOptions struct.
    //! \exception std::runtime_error when command line argument parsing fails.
    ProgramOptions(int argc, char** argv);

    //! `-h`: \c true if the program should exit with its usage info.
    bool help = false;
    //! `-v`: \c true if the program should exit with its version info.
    bool version = false;

    //! A list of Ruby expressions to evaluate in the given order.
    std::vector<std::string_view> expressions;
};

ProgramOptions::ProgramOptions(int argc, char** argv)
{
    if (argc < 1)
    {
        throw std::runtime_error("Really?");
    }

    std::vector<std::string_view> args(argv + 1, argv + argc);

    // scan for options
    auto it = args.begin();
    while (it != args.end())
    {
        auto arg = *it;

        if (arg.length() == 0) // skip empty arguments
        {
            ++it; // advance to the next argument before restarting the loop
            continue;
        }

        if (arg == "--") // every other arg is an expression
        {
            it = args.erase(it); // remove the current arg from the list as we already processed it
            break;
        }

        // special handling for GNU-style help options
        if (arg == "--help")
        {
            help = true;
            it = args.erase(it); // remove the current arg from the list as we already processed it
            continue;
        }
        if (arg == "--version")
        {
            version = true;
            it = args.erase(it); // remove the current arg from the list as we already processed it
            continue;
        }

        if (arg.length() > 1 && arg[0] == '-')
        {
            auto argit = arg.begin();
            ++argit;
            for (; argit != arg.end(); ++argit)
            {
                switch (*argit)
                {
                case 'h':
                    help = true;
                    break;
                case 'v':
                    version = true;
                    break;
                default:
                    throw std::runtime_error(std::string("invalid option -") + *argit);
                }
            }

            it = args.erase(it); // remove the current arg from the list as we already processed it
            continue;
        }

        ++it; // advance to the next argument before restarting the loop
    }

    // the remaining args are our expressions
    expressions = args;
}

//! \brief Print the version to \a stream.
//! \param stream The output stream to print the version info to.
void print_version(std::ostream& stream = std::cout)
{
    stream << "rq " << PROJECT_VERSION << " (mruby " << MRUBY_VERSION << ")" << std::endl;
}

//! \brief Print the application's usage to \a stream.
//! \param stream The output stream to print the usage to.
void print_usage(std::ostream& stream = std::cout)
{
    stream << R"(Usage: rq [options] [--] [EXPRESSION...]
  -v              print the version number
  -h              show this message)" << std::endl;
}

//! \brief Main entry point.
//! \param argc Number of arguments.
//! \param argv List of arguments.
int main(int argc, char** argv)
{
    try
    {
        ProgramOptions opts(argc, argv);
        if (opts.help)
        {
            print_usage();
            return EXIT_SUCCESS;
        }

        if (opts.version)
        {
            print_version();
            return EXIT_SUCCESS;
        }

        MRuby rb;

        std::cout << "reading from stdin" << std::endl;
        if (!rb.eval("item = JSON.parse(STDIN.read)"))
        {
            std::cerr << "rq: read from stdin failed:" << std::endl;
            rb.print_error();

            return EXIT_FAILURE;
        }

        std::cout << "running " << opts.expressions.size() << " expressions" << std::endl;
        for (const auto& expr : opts.expressions)
        {
            auto wrapped_expr = ExpressionWrapper::wrap(expr);
            std::cout << "----> " << wrapped_expr << std::endl;
            if (!rb.eval(wrapped_expr))
            {
                std::cerr << "rq: expression " << expr << " failed to run:" << std::endl;
                rb.print_error();

                return EXIT_FAILURE;
            }
        }

        std::cout << "printing item" << std::endl;
        if (!rb.eval("puts JSON.generate(item, pretty_print: true, indent_width: 2)"))
        {
            std::cerr << "rq: printing item failed" << std::endl;
            rb.print_error();

            return EXIT_FAILURE;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "rq: " << ex.what() << std::endl << std::endl;
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
