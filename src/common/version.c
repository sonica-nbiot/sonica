/*
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

#include "sonica/version.h"

char* sonica_get_version()
{
  return SONICA_VERSION_STRING;
}

int sonica_get_version_major()
{
  return SONICA_VERSION_MAJOR;
}
int sonica_get_version_minor()
{
  return SONICA_VERSION_MINOR;
}
int sonica_get_version_patch()
{
  return SONICA_VERSION_PATCH;
}

int sonica_check_version(int major, int minor, int patch)
{
  return (SONICA_VERSION >= SONICA_VERSION_ENCODE(major, minor, patch));
}
