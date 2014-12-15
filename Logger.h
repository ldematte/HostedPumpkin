

#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

class LogLevel {
public:
   enum Level {
      Info = 1,
      Debug = 2,
      Error = 3,
      Critical = 4
   };
};

class Logger {

private:
   static LogLevel::Level currentLevel;

public:
   static void Error(const char* format, ...);
   static void Critical(const char* format, ...);
   static void Info(const char* format, ...);
   static void Debug(const char* format, ...);
   static void Debug(const wchar_t* format, ...);

};


#endif //LOGGER_H_INCLUDED
