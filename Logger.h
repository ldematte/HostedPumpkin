

#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <stdarg.h>

class LogLevel {
public:
   enum Level {
      Info = 0,
      Debug = 1,
      Error = 2,
      Critical = 3
   };
};

class Logger {

private:
   static LogLevel::Level currentLevel;

   static void Log(int level, const char* format, va_list ap);
   static void Log(int level, const wchar_t* format, va_list ap);

public:
   static void Error(const char* format, ...);
   static void Error(const wchar_t* format, ...);
   static void Critical(const char* format, ...);
   static void Critical(const wchar_t* format, ...);
   static void Info(const char* format, ...);
   static void Info(const wchar_t* format, ...);   
   static void Debug(const char* format, ...);
   static void Debug(const wchar_t* format, ...);

};


#endif //LOGGER_H_INCLUDED
