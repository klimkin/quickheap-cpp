#ifndef QUICKHEAP_EXPORT_HPP
#define QUICKHEAP_EXPORT_HPP

#if defined(_WIN32) && defined(QUICKHEAP_SHARED)
#if defined(QUICKHEAP_BUILDING_LIBRARY)
#define QUICKHEAP_API __declspec(dllexport)
#else
#define QUICKHEAP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define QUICKHEAP_API __attribute__((visibility("default")))
#else
#define QUICKHEAP_API
#endif

#endif
