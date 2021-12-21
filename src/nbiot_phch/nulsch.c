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

#include "sonica/nbiot_phch/nulsch.h"

#include <stdlib.h>
#include <string.h>

#include "oai/phy_coding/defs.h"
#include "srslte/phy/utils/vector.h"

#define SCH_MAX_G_BITS (SRSLTE_MAX_PRB * 12 * 12 * 12)

typedef struct {
  unsigned int C;
  unsigned int Cplus;
  unsigned int Cminus;
  unsigned int Kplus;
  unsigned int Kminus;
  unsigned int F;
} sonica_segm;

extern void ulsch_deinterleave(int16_t*          q_bits,
                               uint32_t          Qm,
                               uint32_t          H_prime_total,
                               uint32_t          N_pusch_symbs,
                               int16_t*          g_bits,
                               srslte_uci_bit_t* ri_bits,
                               uint32_t          nof_ri_bits,
                               uint8_t*          ri_present,
                               uint32_t*         inteleaver_lut);

int sonica_nulsch_init(sonica_nulsch_t* q)
{
  int ret = SRSLTE_ERROR;

  bzero(q, sizeof(sonica_nulsch_t));

  q->ul_interleaver = srslte_vec_u32_malloc(SCH_MAX_G_BITS);

  q->temp_g_bits = srslte_vec_u8_malloc(SCH_MAX_G_BITS);
  if (!q->temp_g_bits) {
    goto clean;
  }
  bzero(q->temp_g_bits, SCH_MAX_G_BITS);

  openair_crcTableInit();
  openair_init_td16();

  ret = SRSLTE_SUCCESS;

clean:
  if (ret == SRSLTE_ERROR) {
    sonica_nulsch_free(q);
  }
  return ret;
}

int sonica_nulsch_free(sonica_nulsch_t *q)
{
  if (q->ul_interleaver) {
    free(q->ul_interleaver);
  }

  if (q->temp_g_bits) {
    free(q->temp_g_bits);
  }

  return SRSLTE_SUCCESS;
}

int sonica_nulsch_decode(sonica_nulsch_t*            q,
                         srslte_ra_nbiot_ul_grant_t* grant,
                         int16_t*                    q_bits,
                         int16_t*                    g_bits,
                         uint8_t*                    data)
{
  sonica_segm segm;
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (openair_lte_segmentation(NULL, NULL, grant->mcs.tbs + 3 * 8,
        &segm.C, &segm.Cplus, &segm.Cminus, &segm.Kplus, &segm.Kminus, &segm.F) != 0) {
    return ret;
  }

  uint32_t nb_q = grant->mcs.nof_bits;
  uint32_t nb_q_ru = nb_q / grant->nof_ru;
  uint32_t Qm   = srslte_mod_bits_x_symbol(grant->mcs.mod);
  uint32_t offset = 0;
  uint8_t crc_type;
  uint32_t E;
  uint32_t r_offset = 0;

  // Interleaving is done per RU
  for (uint32_t i = 0; i < grant->nof_ru; i++) {
    ulsch_deinterleave(&q_bits[i * nb_q_ru],
                       Qm,
                       nb_q_ru / Qm,
                       12 /* Symbols per RU */,
                       &g_bits[i * nb_q_ru],
                       q->ack_ri_bits,
                       0,
                       q->temp_g_bits,
                       q->ul_interleaver);
  }

  for (uint32_t r = 0; r < segm.C; r++) {
    uint32_t Kr = (r < segm.Cminus) ? segm.Kminus : segm.Kplus;
    uint32_t Kr_bytes = Kr / 8;

    // interleaver index
    int iind = 0;
    if (Kr_bytes<=64) {
      iind = (Kr_bytes-5);
    } else if (Kr_bytes <=128) {
      iind = 59 + ((Kr_bytes-64)>>1);
    } else if (Kr_bytes <= 256) {
      iind = 91 + ((Kr_bytes-128)>>2);
    } else if (Kr_bytes <= 768) {
      iind = 123 + ((Kr_bytes-256)>>3);
    } else {
      fprintf(stderr, "Invalid CB length\n");
    }

    memset(&q->dummy_w[r][0], 0, 3*(6144+64)*sizeof(short));
    uint32_t RTC = openair_generate_dummy_w(4 + Kr, (uint8_t*)&q->dummy_w[r][0],
                                    (r==0) ? segm.F : 0);

    if (openair_lte_rm_turbo_rx(RTC,
                                nb_q, // G
                                q->w[r],
                                (uint8_t*) &q->dummy_w[r][0],
                                g_bits + r_offset,
                                segm.C,
                                OPENAIR_NSOFT,
                                0,   //Uplink
                                1,
                                0 /* rvidx */,
                                1 /* clear */,
                                Qm /* Qm */,
                                1,
                                r,
                                &E)==-1) {
      fprintf(stderr, "ulsch_decoding: Problem in rate matching\n");
      return(-1);
    }

    r_offset += E;

    openair_sub_block_deinterleaving_turbo(4 + Kr, &q->d[r][96], q->w[r]);

    if (segm.C == 1) {
      crc_type = OPENAIR_CRC24_A;
    } else {
      crc_type = OPENAIR_CRC24_B;
    }

    ret = openair_3gpp_turbo_decoder16(&q->d[r][96],
       q->c[r],
       Kr,
       f1f2mat_old[iind*2],
       f1f2mat_old[(iind*2)+1],
       10,//MAX_TURBO_ITERATIONS,
       crc_type,
       (r==0) ? segm.F : 0);

    if (ret != (1 + 10)) {
      if (r==0) {
        uint32_t len = Kr_bytes - (segm.F / 8) - ((segm.C > 1) ? 3 : 0);
        memcpy(data, &q->c[0][segm.F / 8], len);
        offset = len;
      } else {
        uint32_t len = Kr_bytes - ((segm.C > 1) ? 3 : 0);
        memcpy(&data[offset], q->c[r], len);
        offset += len;
      }
    } else {
      return SRSLTE_ERROR;
    }
  }

  return SRSLTE_SUCCESS;
}