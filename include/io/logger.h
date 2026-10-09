// Logging to stderr in one format:  [<context>] WARNING: <message>
// The context names what is being processed, e.g. "Kessenich|3446 id=3446" (dataset|index id),
// so messages from deep inside the pipeline can be traced back to their instance.

#ifndef MEDIAL_AXIS_GRAPH_LOGGER_H
#define MEDIAL_AXIS_GRAPH_LOGGER_H

#include <string>

// Context shown in front of every message; empty context: no prefix
void set_log_context(const std::string &context);
void clear_log_context();

// Info messages are only printed if verbose output is enabled (--verbose)
void set_verbose(bool verbose);

void log_info(const std::string &message);
void log_warning(const std::string &message);
void log_error(const std::string &message);

#endif // MEDIAL_AXIS_GRAPH_LOGGER_H
