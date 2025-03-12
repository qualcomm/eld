//===- Defines.h-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_DEFINES_H
#define ELD_PLUGINAPI_DEFINES_H

// This particular piece is duplicated in all the header files. This is
// make sure the customer includes one header file. Heaer files that are
// included in specific CPP files will also export the definition.
#ifdef _WIN32
// MSVC produces a warning from exporting STL types. This is to prevent
// customers from mixing STL implementations. We dont have that use case
// as we ask the customers to use the same runtime as per what is built with the
// tools. Disable this warning.
#pragma warning(disable : 4251)
#ifndef DLL_A_EXPORT
#ifndef IN_DLL
#define DLL_A_EXPORT __declspec(dllexport)
#else
#define DLL_A_EXPORT __declspec(dllimport)
#endif // IN_DLL
#endif // DLL_A_EXPORT
#endif // WIN32

#ifndef _WIN32
#define DLL_A_EXPORT
#endif // WIN32

#if defined(__GNUC__) || defined(__clang__)
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#define DEPRECATED
#endif

#endif
