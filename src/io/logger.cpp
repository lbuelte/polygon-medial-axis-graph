#include "io/logger.h"

#include <iostream>

/* ------ Global logger ------ */

std::ofstream global_log; // Global declaraion of logger

void init_global_log(const std::string &filename)
{
    namespace fs = std::filesystem;

    try {
        fs::path path(filename);
        fs::path dir = path.parent_path();

        // Create directory if it doesn't exist and is not empty
        if (!dir.empty() && !fs::exists(dir)) {
            std::cout << "creating dir " << filename << std::endl;
            fs::create_directories(dir);
        }

        global_log.open(filename, std::ios::out);
        if (!global_log) {
            std::cerr << "ERROR: Could not open log file: " << filename << "\n";
            return;
        }

        global_log << std::unitbuf;  // auto-flush
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }
}

void close_global_log()
{
    if (global_log.is_open())
        global_log.close();
}


/* ------ LogContext ------ */

static LogContext current_context;  // only visible in this file

void set_log_context(const std::string& dataset, int instance_id)
{
    current_context.dataset = dataset;
    current_context.instance_id = instance_id;
}

void clear_log_context()
{
    current_context = LogContext{};
}

void log_error(std::ostream &stream, const std::string& message)
{
    if (!stream)
        return;

    stream << "[dataset=" << current_context.dataset
           << ", instance=" << current_context.instance_id
           << "] ERROR: "
           << message << std::endl;
}

void log_warning(std::ostream &stream, const std::string& message)
{
    if (!stream)
        return;

    stream << "[dataset=" << current_context.dataset
           << ", instance=" << current_context.instance_id
           << "] WARNING: "
           << message << std::endl;
}
