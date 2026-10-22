#ifndef PLAINROOT_VALIDATION_EXPORT_H
#define PLAINROOT_VALIDATION_EXPORT_H

#if defined _WIN32 || defined __CYGWIN__
#  ifdef PLAINROOT_VALIDATION_BUILD
#    define PLAINROOT_VALIDATION_API __declspec(dllexport)
#  else
#    define PLAINROOT_VALIDATION_API __declspec(dllimport)
#  endif
#elif defined __GNUC__
#  define PLAINROOT_VALIDATION_API __attribute__((visibility("default")))
#else
#  define PLAINROOT_VALIDATION_API
#endif

#endif
