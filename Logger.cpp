
#include "Logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

static const size_t TIME_BUFFER_SIZE = 80;
static const size_t LINE_BUFFER_SIZE = 320;

LogLevel::Level Logger::currentLevel = LogLevel::Debug;

// Ugly, but faster than returning a std::string
static void now(char* buffer) {
   time_t rawtime;
   struct tm timeinfo;
   
   time(&rawtime);
   localtime_s(&timeinfo, &rawtime);

   strftime(buffer, TIME_BUFFER_SIZE, "%d-%m-%Y %I:%M:%S", &timeinfo);
}

static void now(wchar_t* buffer) {
   time_t rawtime;
   struct tm timeinfo;

   time(&rawtime);
   localtime_s(&timeinfo, &rawtime);

   
   wcsftime(buffer, TIME_BUFFER_SIZE, L"%d-%m-%Y %I:%M:%S", &timeinfo);
}

static void log(const char* level, const char* message) {
   char time_buffer[TIME_BUFFER_SIZE];
   now(time_buffer);
   fprintf(stderr, "%s - [%s] %s\n", time_buffer, level, message);
}

static void log(const wchar_t* level, const wchar_t* message) {
   wchar_t time_buffer[TIME_BUFFER_SIZE];
   now(time_buffer);   
   fwprintf(stderr, L"%s - [%s] %s\n", time_buffer, level, message);
}

void Logger::Info(const char* format, ...) {
   if (currentLevel > LogLevel::Info)
      return;

   char line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vsnprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log("INFO", line_buffer);
}

void Logger::Info(const wchar_t* format, ...) {
   if (currentLevel > LogLevel::Info)
      return;

   wchar_t line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vswprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log(L"INFO", line_buffer);
}

void Logger::Debug(const char* format, ...) {
   if (currentLevel > LogLevel::Debug)
      return;

   char line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vsnprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log("DEBUG", line_buffer);
}

void Logger::Debug(const wchar_t* format, ...) {
   if (currentLevel > LogLevel::Debug)
      return;

   wchar_t line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vswprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log(L"DEBUG", line_buffer);
}

void Logger::Error(const char* format, ...) {
   if (currentLevel > LogLevel::Error)
      return;
   char line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vsnprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log("ERROR", line_buffer);
}

void Logger::Error(const wchar_t* format, ...) {
   if (currentLevel > LogLevel::Error)
      return;
   wchar_t line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vswprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log(L"ERROR", line_buffer);
}

void Logger::Critical(const char* format, ...) {
   char line_buffer[LINE_BUFFER_SIZE];

   va_list ap;
   va_start(ap, format);
   vsnprintf_s(line_buffer, LINE_BUFFER_SIZE, format, ap);
   va_end(ap);

   log("CRITICAL", line_buffer);
}

