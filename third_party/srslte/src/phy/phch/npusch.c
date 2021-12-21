#include "srslte/srslte.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srslte/phy/ch_estimation/refsignal_ul.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft_precoding.h"
#include "srslte/phy/phch/npusch.h"
#include "srslte/phy/utils/bit.h"
#include "srslte/phy/utils/debug.h"
#include "srslte/phy/utils/vector.h"

// For NPSUCH, the maximum number of RE is 2 slots per sf,
// 7 symbols per slot, 12 re per symbol and maximum 10 RU
#define MAX_NPUSCH_RE (2 * 7 * 12 * 10)

#define CURRENT_SFLEN_RE SRSLTE_SF_LEN_RE(q->cell.base.nof_prb, q->cell.base.cp)

// TODO: This only handles Format 1, 15kHz case
int srslte_npusch_cp(cf_t *input,
                     cf_t *output,
                     srslte_ra_nbiot_ul_grant_t* grant,
                     bool advance_input)
{
  cf_t* in_ptr  = input;
  cf_t* out_ptr = output;

  for (uint32_t slot = 0; slot < SRSLTE_NOF_SLOTS_PER_SF; slot++) {
    for (uint32_t l = 0; l < SRSLTE_CP_NORM_NSYMB; l++) {
      if (l != 3) {
        uint32_t idx = (slot * 7 + l) * SRSLTE_NRE + grant->sc_alloc_set[0];
        if (advance_input) {
          out_ptr = &output[idx];
        } else {
          in_ptr = &input[idx];
        }
        memcpy(out_ptr, in_ptr, grant->nof_sc * sizeof(cf_t));
        if (advance_input) {
          in_ptr += grant->nof_sc;
        } else {
          out_ptr += grant->nof_sc;
        }
      }
    }
  }

  if (advance_input) {
    return in_ptr - input;
  } else {
    return out_ptr - output;
  }
}

static int npusch_get(cf_t* input, cf_t* output, srslte_ra_nbiot_ul_grant_t* grant)
{
  return srslte_npusch_cp(input, output, grant, false);
}

static int npusch_put(cf_t* input, cf_t* output, srslte_ra_nbiot_ul_grant_t* grant)
{
  return srslte_npusch_cp(input, output, grant, true);
}

static int npusch_init(srslte_npusch_t* q, bool is_ue)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    ret = SRSLTE_ERROR;
    bzero(q, sizeof(srslte_npusch_t));
    q->max_re = MAX_NPUSCH_RE;

    q->g_bits = srslte_vec_i16_malloc(q->max_re * srslte_mod_bits_x_symbol(SRSLTE_MOD_QPSK));
    if (!q->g_bits) {
      goto clean;
    }

    q->q_bits = srslte_vec_i16_malloc(q->max_re * srslte_mod_bits_x_symbol(SRSLTE_MOD_QPSK));
    if (!q->q_bits) {
      goto clean;
    }

    q->d = srslte_vec_cf_malloc(q->max_re);
    if (!q->d) {
      goto clean;
    }

    q->tx_syms = srslte_vec_cf_malloc(q->max_re);
    if (!q->tx_syms) {
      goto clean;
    }

    q->rx_syms = srslte_vec_cf_malloc(q->max_re);
    if (!q->rx_syms) {
      goto clean;
    }

    q->ce = srslte_vec_cf_malloc(q->max_re);
    if (!q->ce) {
      goto clean;
    }

    // TODO: Add BPSK when NRUsc = 1 is ready
    if (srslte_modem_table_lte(&q->mod_qpsk, SRSLTE_MOD_QPSK)) {
      goto clean;
    }
    srslte_modem_table_bytes(&q->mod_qpsk);

    if (srslte_dft_precoding_init(&q->dft_precoding, 1, is_ue)) {
      goto clean;
    }

    if (srslte_sch_init(&q->nul_sch)) {
      goto clean;
    }

    ret = SRSLTE_SUCCESS;
  }

clean:
  if (ret == SRSLTE_ERROR) {
    srslte_npusch_free(q);
  }
  return ret;
}

int srslte_npusch_init_ue(srslte_npusch_t* q)
{
  return npusch_init(q, true);
}

int srslte_npusch_init_enb(srslte_npusch_t* q)
{
  return npusch_init(q, false);
}

void srslte_npusch_free(srslte_npusch_t* q)
{
  if (q->g_bits) {
    free(q->g_bits);
  }

  if (q->q_bits) {
    free(q->q_bits);
  }

  if (q->d) {
    free(q->d);
  }

  if (q->tx_syms) {
    free(q->tx_syms);
  }

  if (q->rx_syms) {
    free(q->rx_syms);
  }

  if (q->ce) {
    free(q->ce);
  }

  srslte_modem_table_free(&q->mod_qpsk);
  srslte_dft_precoding_free(&q->dft_precoding);
  srslte_sch_free(&q->nul_sch);

  bzero(q, sizeof(srslte_npusch_t));
}

int srslte_npusch_set_cell(srslte_npusch_t* q, srslte_nbiot_cell_t cell)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q != NULL && srslte_nbiot_cell_isvalid(&cell)) {
    q->cell = cell;

    INFO("NPUSCH: Cell config n_id_ncell=%d, %d ports, %d PRBs base cell, max_symbols: %d\n",
         q->cell.n_id_ncell,
         q->cell.nof_ports,
         q->cell.base.nof_prb,
         q->max_re);

    ret = SRSLTE_SUCCESS;
  }
  return ret;
}

// TODO: Pregenerate sequences?
static srslte_sequence_t* get_user_sequence(srslte_npusch_t* q, uint16_t rnti, uint32_t nf,
                                            uint32_t nslot, uint32_t len)
{
  srslte_sequence_npusch(&q->tmp_seq, rnti, nf, nslot, q->cell.n_id_ncell, len);
  return &q->tmp_seq;
}

int srslte_npusch_encode(srslte_npusch_t*        q,
                         srslte_npusch_cfg_t*    cfg,
                         srslte_softbuffer_tx_t* softbuffer,
                         uint8_t*                data,
                         cf_t*                   sf_symbols)
{
  int   ret = SRSLTE_ERROR_INVALID_INPUTS;
  if (q != NULL && data != NULL && cfg != NULL) {
    if (cfg->grant.mcs.tbs == 0) {
      return SRSLTE_ERROR_INVALID_INPUTS;
    }

    if (!cfg->is_encoded) {
      int ret = srslte_nulsch_encode(&q->nul_sch, &cfg->grant, data, softbuffer, q->g_bits, q->q_bits);
      if (ret) {
        return ret;
      }

      srslte_sequence_t* seq = get_user_sequence(q, cfg->rnti, cfg->grant.tx_tti / 10,
                                                 (cfg->grant.tx_tti % 10) * 2, cfg->nbits.nof_bits);

      srslte_scrambling_bytes(seq, q->q_bits, cfg->nbits.nof_bits);

      srslte_mod_modulate_bytes(&q->mod_qpsk, q->q_bits, q->d, cfg->nbits.nof_bits);

      // TODO: support NRUsc other than 12
      srslte_dft_precoding(&q->dft_precoding, q->d, q->tx_syms, 1, cfg->grant.nof_slots * 6);

      cfg->is_encoded = true;
    }

    if (cfg->is_encoded) {
      npusch_put(&q->tx_syms[cfg->sf_idx * 6 * 2 * cfg->grant.nof_sc], sf_symbols, &cfg->grant);

      // TODO: support repetition
      cfg->num_sf++;
      cfg->sf_idx++;
    }

    ret = SRSLTE_SUCCESS;
  }

  return ret;
}


int srslte_npusch_decode(srslte_npusch_t*        q,
                         srslte_npusch_cfg_t*    cfg,
                         srslte_softbuffer_rx_t* softbuffer,
                         cf_t*                   sf_symbols,
                         cf_t*                   ce,
                         float                   noise_estimate,
                         uint8_t*                data)
{
  uint32_t n;

  if (q != NULL && sf_symbols != NULL && data != NULL && cfg != NULL) {
    uint32_t num_sf = cfg->grant.nof_slots / 2;
    uint32_t num_re_per_sf = 6 * 2 * cfg->grant.nof_sc;

    for (uint32_t i = 0; i < num_sf; i++) {
      n = npusch_get(&sf_symbols[i * CURRENT_SFLEN_RE], &q->rx_syms[i * num_re_per_sf], &cfg->grant);
      if (n != num_re_per_sf) {
        fprintf(stderr, "Error expecting %d symbols but got %d\n", num_re_per_sf, n);
        return SRSLTE_ERROR;
      }

      n = npusch_get(&ce[i * CURRENT_SFLEN_RE], &q->ce[i * num_re_per_sf], &cfg->grant);
      if (n != num_re_per_sf) {
        fprintf(stderr, "Error expecting %d symbols but got %d\n", num_re_per_sf, n);
        return SRSLTE_ERROR;
      }
    }

    srslte_predecoding_single(q->rx_syms, q->ce, q->d, NULL, cfg->nbits.nof_re, 1.0f, noise_estimate);

    // TODO: support NRUsc other than 12
    srslte_dft_precoding(&q->dft_precoding, q->d, q->rx_syms, 1, cfg->grant.nof_slots * 6);

    srslte_demod_soft_demodulate_s(cfg->grant.mcs.mod, q->rx_syms, q->q_bits, cfg->nbits.nof_re);

    srslte_sequence_t* seq = get_user_sequence(q, cfg->rnti, cfg->grant.tx_tti / 10,
                                               (cfg->grant.tx_tti % 10) * 2, cfg->nbits.nof_bits);

    srslte_scrambling_s_offset(seq, q->q_bits, 0, cfg->nbits.nof_bits);

    return srslte_nulsch_decode(&q->nul_sch, &cfg->grant, q->q_bits, q->g_bits, softbuffer, data);
  } else {
    fprintf(stderr, "srslte_npusch_decode() called with invalid parameters.\n");
    return SRSLTE_ERROR_INVALID_INPUTS;
  }
}

/* Configures the structure srslte_npusch_cfg_t from a DL grant.
 * If grant is NULL, the grant is assumed to be already stored in cfg->grant
 */
int srslte_npusch_cfg(srslte_npusch_cfg_t*        cfg,
                      srslte_ra_nbiot_ul_grant_t* grant,
                      uint16_t                    rnti)
{
  if (cfg) {
    if (grant) {
      memcpy(&cfg->grant, grant, sizeof(srslte_ra_nbiot_ul_grant_t));
    }

    srslte_ra_nbiot_ul_grant_to_nbits(&cfg->grant, &cfg->nbits);
    cfg->grant.mcs.nof_bits = cfg->nbits.nof_bits;
    cfg->sf_idx     = 0;
    cfg->rep_idx    = 0;
    cfg->num_sf     = 0;
    cfg->is_encoded = false;
    cfg->rnti       = rnti;

    return SRSLTE_SUCCESS;
  } else {
    return SRSLTE_ERROR_INVALID_INPUTS;
  }
}