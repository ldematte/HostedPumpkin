
#include "Logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const size_t TIME_BUFFER_SIZE = 80;
static const size_t LINE_BUFFER_SIZE = 320;

LogLevel::Level Logger::currentLevel = LogLevel::Debug;

wchar_t* wlevels [] = { L"Info", L"Debug", L"Error", L"Critical" };
char* levels [] = { "Info", "Debug", "Error", "Critical" };

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
   fflush(stderr);
}

void Logger::Log(int level, const char* format, va_list ap) {
   if (currentLevel > level)
      return;

   static char line_buffer[LINE_BUFFER_SIZE];
   char* buffer;

   int len = _vscprintf(format, ap) + 1; // _vscprintf doesn't count terminating '\0'
   // If our static buffer (faster) is not long enough, allocate a bigger one
   if (len > LINE_BUFFER_SIZE) {
      buffer = (char*) malloc(len * sizeof(char));
   }
   else {
      buffer = line_buffer;
   }
   vsprintf_s(buffer, len, format, ap);
   log(levels[level], line_buffer);

   // If we allocated it, free it
   if (buffer != line_buffer)
      free(buffer);
}

void Logger::Log(int level, const wchar_t* format, va_list ap) {
   if (currentLevel > level)
      return;

   static wchar_t line_buffer[LINE_BUFFER_SIZE];
   wchar_t* buffer;
   
   int len = _vscwprintf(format, ap) + 1; // _vscprintf doesn't count terminating '\0'
   // If our static buffer (faster) is not long enough, allocate a bigger one
   if (len > LINE_BUFFER_SIZE) {
      buffer = (wchar_t*) malloc(len * sizeof(wchar_t));
   }
   else {
      buffer = line_buffer;
   }
   vswprintf_s(buffer, len, format, ap);
   log(wlevels[level], line_buffer);

   // If we allocated it, free it
   if (buffer != line_buffer)
      free(buffer);
}

void Logger::Info(const char* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Info, format, ap);
   va_end(ap);
}

void Logger::Info(const wchar_t* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Info, format, ap);
   va_end(ap);
}

void Logger::Debug(const char* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Debug, format, ap);
   va_end(ap);
}

void Logger::Debug(const wchar_t* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Debug, format, ap);
   va_end(ap);
}

void Logger::Error(const char* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Error, format, ap);
   va_end(ap);
}

void Logger::Error(const wchar_t* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Error, format, ap);
   va_end(ap);
}

void Logger::Critical(const char* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Critical, format, ap);
   va_end(ap);
}

void Logger::Critical(const wchar_t* format, ...) {
   va_list ap;
   va_start(ap, format);
   Log(LogLevel::Critical, format, ap);
   va_end(ap);
}

