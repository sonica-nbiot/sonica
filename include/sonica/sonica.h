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

#ifndef SONICA_SONICA_H
#define SONICA_SONICA_H

#ifdef __cplusplus
#include <complex>
extern "C" {
#else
#include <complex.h>
#endif

#include <math.h>

#include "sonica/config.h"
#include "sonica/version.h"

#include "sonica/nbiot_enb/enb_dl_nbiot.h"
#include "sonica/nbiot_enb/enb_ul_nbiot.h"

#include "sonica/nbiot_phch/nprach.h"
#include "sonica/nbiot_phch/npusch.h"

#ifdef __cplusplus
}
#undef I // Fix complex.h #define I nastiness when using C++
#endif

#endif // SONICA_SONICA_H
