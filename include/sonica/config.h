/*
 * Copyright 2013-2020 Software Radio Systems Limited
 * Copyright 2021      Metro Group @ UCLA
 *
 * This file is part of Sonica.
 *
 * Sonica is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Sonica is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SONICA_CONFIG_H
#define SONICA_CONFIG_H

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define SONICA_IMPORT __declspec(dllimport)
#define SONICA_EXPORT __declspec(dllexport)
#define SONICA_LOCAL
#else
#if __GNUC__ >= 4
#define SONICA_IMPORT __attribute__((visibility("default")))
#define SONICA_EXPORT __attribute__((visibility("default")))
#else
#define SONICA_IMPORT
#define SONICA_EXPORT
#define SONICA_LOCAL
#endif
#endif

// Define SONICA_API
// SONICA_API is used for the public API symbols.
#ifdef SONICA_DLL_EXPORTS // defined if we are building the SONICA DLL (instead of using it)
#define SONICA_API SONICA_EXPORT
#else
#define SONICA_API SONICA_IMPORT
#endif

// Useful macros for templates
#define CONCAT(a, b) a##b
#define CONCAT2(a, b) CONCAT(a, b)

#define STRING2(x) #x
#define STRING(x) STRING2(x)

// Common error codes
#define SONICA_SUCCESS 0
#define SONICA_ERROR -1
#define SONICA_ERROR_INVALID_INPUTS -2
#define SONICA_ERROR_TIMEOUT -3
#define SONICA_ERROR_INVALID_COMMAND -4
#define SONICA_ERROR_OUT_OF_BOUNDS -5
#define SONICA_ERROR_CANT_START -6
#define SONICA_ERROR_ALREADY_STARTED -7

// cf_t definition
typedef _Complex float cf_t;

#ifdef ENABLE_C16
typedef _Complex short int c16_t;
#endif /* ENABLE_C16 */

#endif // SONICA_CONFIG_H
