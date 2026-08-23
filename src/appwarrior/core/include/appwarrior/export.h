// AppWarrior: shared-library export/import macro.
//
// Marks the framework's out-of-line public symbols for Windows DLLs:
// AW_API expands to __declspec(dllexport) while building the library and
// __declspec(dllimport) while consuming it. On other platforms it is
// empty (default symbol visibility). Header-only inline facilities never
// need it.

#pragma once

#if defined(_WIN32)
  #if defined(AW_BUILDING_LIBRARY)
    #define AW_API __declspec(dllexport)
  #else
    #define AW_API __declspec(dllimport)
  #endif
#else
  #define AW_API
#endif
