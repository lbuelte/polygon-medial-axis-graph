#ifndef MEDIALAXISWITHHOLES_LOGGER_H
#define MEDIALAXISWITHHOLES_LOGGER_H

#include <fstream>
#include <filesystem>

// Functions to initialize and close the global logger global_log
void init_global_log(const std::string &filename);
void close_global_log();


// Set LogContext to log instance specific errors in pruning
struct LogContext {
    std::string dataset;
    int instance_id = -1;
};
void set_log_context(const std::string& dataset, int instance_id);
void clear_log_context();


void log_error(std::ostream &stream, const std::string& message);
void log_warning(std::ostream &stream, const std::string& message);

#endif // MEDIALAXISWITHHOLES_LOGGER_H