#pragma once

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// enum
enum LEVEL
{
    INFO,
    DEBUG,
    WARNING,
    ERROR,
    FATAL
};

#define DEFAULT_LEVEL DEBUG

#define LOGBUFFERSIZE 1024
#define TMBUFSIZE 32

// template<typename... Args>
// inline void log(LEVEL lv, Args... arg)
// {
//     if(DEFAULT_LEVEL > lv) return;
// }


// inline void log(LEVEL lv, const char* format, ...)
// {
//     if(DEFAULT_LEVEL > lv) return;
//     time_t t = time(nullptr);
//     struct tm* lt = localtime(&t);
//     char tmbuf[TMBUFSIZE] = {0};
//     strftime(tmbuf, 31, "%H:%M:%S", lt);

//     std::cout << format << std::endl;

//     va_list arg;
//     va_start(arg, format);
//     char buffer[LOGBUFFERSIZE];
//     vsnprintf(buffer, LOGBUFFERSIZE, format, arg);

//     // fprintf(stdout, "[%s %s:%d] " format "\n", buf, __FILE__, __LINE__, __VA_ARGS__);
//     // fprintf(stdout, "[%s %s:%d] %s\n", tmbuf, __FILE__, __LINE__, buffer);
//     fprintf(stdout, "[%s %s\n", tmbuf, buffer);
// }

// #define LOG(level, format, ...) log(level, "%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG(level, format, ...) do{\
    if (level < DEFAULT_LEVEL) break;\
    time_t t = time(NULL);\
    struct tm *lt = localtime(&t);\
    char buf[32] = {0};\
    strftime(buf, 31, "%H:%M:%S", lt);\
    fprintf(stdout, "[%s %s:%d] " format "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(0)



