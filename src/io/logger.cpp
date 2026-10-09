#include "io/logger.h"

#include <iostream>

namespace {

std::string current_context;    // set by set_log_context, shown in front of every message
bool verbose_output = false;    // set by set_verbose

void log(const char *level, const std::string &message)
{
    if (not current_context.empty()) {
        std::cerr << "[" << current_context << "] ";
    }
    std::cerr << level << ": " << message << std::endl;
}

} // end of anonymous namespace


void set_log_context(const std::string &context)
{
    current_context = context;
}

void clear_log_context()
{
    current_context.clear();
}

void set_verbose(bool verbose)
{
    verbose_output = verbose;
}

void log_info(const std::string &message)
{
    if (verbose_output) {
        log("INFO", message);
    }
}

void log_warning(const std::string &message)
{
    log("WARNING", message);
}

void log_error(const std::string &message)
{
    log("ERROR", message);
}
