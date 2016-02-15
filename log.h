#ifndef LOG_H
#define LOG_H

typedef enum {
  LOG_TRACE = 0,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR
} log_type;


void log_file_set(const char *logfile);
void log_minlevel_set(log_type minlevel);
void log_env_init();
void log_write(log_type type, const char *file, int line,
               const char *fmt, ...);

#define LOG(type, ...) \
  log_write(type, __FILE__, __LINE__, __VA_ARGS__)

#endif /*LOG_H*/
