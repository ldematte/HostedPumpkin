

#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED


class Logger {

public:
   static void Error(const char* format, ...);

   static void Critical(const char* format, ...);

   static void Info(const char* format, ...);

};


#endif //LOGGER_H_INCLUDED
