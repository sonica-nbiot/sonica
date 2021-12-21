#ifndef OPENAIR_PHY_DEFS
#define OPENAIR_PHY_DEFS

#include <stdint.h>

#define OPENAIR_CRC24_A 0
#define OPENAIR_CRC24_B 1
#define OPENAIR_CRC16 2
#define OPENAIR_CRC8 3

#define OPENAIR_MAX_TURBO_ITERATIONS_MBSFN 8
#define OPENAIR_MAX_TURBO_ITERATIONS 4

#define OPENAIR_NSOFT 1827072
#define OPENAIR_LTE_NULL 2

#define OPENAIR_MAX_NUM_DLSCH_SEGMENTS 16
#define OPENAIR_MAX_NUM_ULSCH_SEGMENTS OPENAIR_MAX_NUM_DLSCH_SEGMENTS
#define OPENAIR_MAX_DLSCH_PAYLOAD_BYTES (OPENAIR_MAX_NUM_DLSCH_SEGMENTS*768)
#define OPENAIR_MAX_ULSCH_PAYLOAD_BYTES (OPENAIR_MAX_NUM_ULSCH_SEGMENTS*768)

/** \fn lte_segmentation(uint8_t *input_buffer,
              uint8_t **output_buffers,
            uint32_t B,
            uint32_t *C,
            uint32_t *Cplus,
            uint32_t *Cminus,
            uint32_t *Kplus,
            uint32_t *Kminus,
            uint32_t *F)
\brief This function implements the LTE transport block segmentation algorithm from 36-212, V8.6 2009-03.
@param input_buffer
@param output_buffers
@param B
@param C
@param Cplus
@param Cminus
@param Kplus
@param Kminus
@param F
*/
int32_t openair_lte_segmentation(uint8_t *input_buffer,
                                 uint8_t **output_buffers,
                                 uint32_t B,
                                 uint32_t *C,
                                 uint32_t *Cplus,
                                 uint32_t *Cminus,
                                 uint32_t *Kplus,
                                 uint32_t *Kminus,
                                 uint32_t *F);

/*!\fn void crcTableInit(void)
\brief This function initializes the different crc tables.*/
void openair_crcTableInit (void);

/*!\fn void init_td16(void)
\brief This function initializes the tables for 16-bit LLR Turbo decoder.*/
void openair_init_td16 (void);

/*!\fn uint32_t crc24a(uint8_t *inPtr, int32_t bitlen)
\brief This computes a 24-bit crc ('a' variant for overall transport block)
based on 3GPP UMTS/LTE specifications.
@param inPtr Pointer to input byte stream
@param bitlen length of inputs in bits
*/
uint32_t openair_crc24a (uint8_t *inPtr, int32_t bitlen);

/*!\fn uint32_t crc24b(uint8_t *inPtr, int32_t bitlen)
\brief This computes a 24-bit crc ('b' variant for transport-block segments)
based on 3GPP UMTS/LTE specifications.
@param inPtr Pointer to input byte stream
@param bitlen length of inputs in bits
*/
uint32_t openair_crc24b (uint8_t *inPtr, int32_t bitlen);

/*!\fn uint32_t crc16(uint8_t *inPtr, int32_t bitlen)
\brief This computes a 16-bit crc based on 3GPP UMTS specifications.
@param inPtr Pointer to input byte stream
@param bitlen length of inputs in bits*/
uint32_t openair_crc16 (uint8_t *inPtr, int32_t bitlen);

/*!\fn uint32_t crc12(uint8_t *inPtr, int32_t bitlen)
\brief This computes a 12-bit crc based on 3GPP UMTS specifications.
@param inPtr Pointer to input byte stream
@param bitlen length of inputs in bits*/
uint32_t openair_crc12 (uint8_t *inPtr, int32_t bitlen);

/*!\fn uint32_t crc8(uint8_t *inPtr, int32_t bitlen)
\brief This computes a 8-bit crc based on 3GPP UMTS specifications.
@param inPtr Pointer to input byte stream
@param bitlen length of inputs in bits*/
uint32_t openair_crc8  (uint8_t *inPtr, int32_t bitlen);



/*\fn void openair_3gpp_turbo_encoder(uint8_t *input,uint16_t input_length_bytes,uint8_t *output,uint8_t F,uint16_t interleaver_f1,uint16_t interleaver_f2)
\brief This function implements a rate 1/3 8-state parralel concatenated turbo code (3GPP-LTE).
@param input Pointer to input buffer
@param input_length_bytes Number of bytes to encode
@param output Pointer to output buffer
@param F Number of filler bits at input
@param interleaver_f1 F1 generator
@param interleaver_f2 F2 generator
*/
void openair_3gpp_turbo_encoder(uint8_t *input,
                                uint16_t input_length_bytes,
                                uint8_t *output,
                                uint8_t F,
                                uint16_t interleaver_f1,
                                uint16_t interleaver_f2);

/** \fn uint32_t sub_block_interleaving_turbo(uint32_t D, uint8_t *d,uint8_t *w)
\brief This is the subblock interleaving algorithm from 36-212 (Release 8, 8.6 2009-03), pages 15-16.
This function takes the d-sequence and generates the w-sequence.  The nu-sequence from 36-212 is implicit.
\param D Number of systematic bits plus 4 (plus 4 for termination)
\param d Pointer to input (d-sequence, turbo code output)
\param w Pointer to output (w-sequence, interleaver output)
\returns Interleaving matrix cardinality (\f$K_{\pi}\f$  from 36-212)
*/
uint32_t openair_sub_block_interleaving_turbo(uint32_t D, uint8_t *d,uint8_t *w);

void openair_sub_block_deinterleaving_turbo(uint32_t D,int16_t *d,int16_t *w);

/*
\brief This is the LTE rate matching algorithm for Turbo-coded channels (e.g. DLSCH,ULSCH).  It is taken directly from 36-212 (Rel 8 8.6, 2009-03), pages 16-18 )
\param RTC R^TC_subblock from subblock interleaver (number of rows in interleaving matrix) for up to 8 segments
\param G This the number of coded transport bits allocated in sub-frame
\param w This is a pointer to the w-sequence (second interleaver output)
\param e This is a pointer to the e-sequence (rate matching output, channel input/output bits)
\param C Number of segments (codewords) in the sub-frame
\param Nsoft Total number of soft bits (from UE capabilities in 36-306)
\param Mdlharq Number of HARQ rounds
\param Kmimo MIMO capability for this DLSCH (0 = no MIMO)
\param rvidx round index (0-3)
\param Qm modulation order (2,4,6)
\param Nl number of layers (1,2)
\param r segment number
\returns \f$E\f$, the number of coded bits per segment */


uint32_t openair_lte_rm_turbo(uint32_t RTC,
                              uint32_t G,
                              uint8_t *w,
                              uint8_t *e,
                              uint8_t C,
                              uint32_t Nsoft,
                              uint8_t Mdlharq,
                              uint8_t Kmimo,
                              uint8_t rvidx,
                              uint8_t Qm,
                              uint8_t Nl,
                              uint8_t r);

uint32_t openair_generate_dummy_w(uint32_t D, uint8_t *w,uint8_t F);

/**
\brief This is the LTE rate matching algorithm for Turbo-coded channels (e.g. DLSCH,ULSCH).  It is taken directly from 36-212 (Rel 8 8.6, 2009-03), pages 16-18 )
\param RTC R^TC_subblock from subblock interleaver (number of rows in interleaving matrix)
\param G This the number of coded transport bits allocated in sub-frame
\param w This is a pointer to the soft w-sequence (second interleaver output) with soft-combined outputs from successive HARQ rounds
\param dummy_w This is the first row of the interleaver matrix for identifying/discarding the "LTE-NULL" positions
\param soft_input This is a pointer to the soft channel output
\param C Number of segments (codewords) in the sub-frame
\param Nsoft Total number of soft bits (from UE capabilities in 36-306)
\param Mdlharq Number of HARQ rounds
\param Kmimo MIMO capability for this DLSCH (0 = no MIMO)
\param rvidx round index (0-3)
\param clear 1 means clear soft buffer (start of HARQ round)
\param Qm modulation order (2,4,6)
\param Nl number of layers (1,2)
\param r segment number
\param E_out the number of coded bits per segment
\returns 0 on success, -1 on failure
*/

int openair_lte_rm_turbo_rx(uint32_t RTC,
                            uint32_t G,
                            int16_t *w,
                            uint8_t *dummy_w,
                            int16_t *soft_input,
                            uint8_t C,
                            uint32_t Nsoft,
                            uint8_t Mdlharq,
                            uint8_t Kmimo,
                            uint8_t rvidx,
                            uint8_t clear,
                            uint8_t Qm,
                            uint8_t Nl,
                            uint8_t r,
                            uint32_t *E_out);

unsigned char openair_3gpp_turbo_decoder16(short *y,
    unsigned char *decoded_bytes,
    unsigned short n,
    unsigned short f1,
    unsigned short f2,
    unsigned char max_iterations,
    unsigned char crc_type,
    unsigned char F);

// Log hacks
#        define LOG_I(c, x...) /* */
#        define LOG_W(c, x...) /* */
#        define LOG_E(c, x...) /* */
#        define LOG_D(c, x...) /* */
#        define LOG_T(c, x...) /* */
#        define LOG_G(c, x...) /* */
#        define LOG_A(c, x...) /* */
#        define LOG_C(c, x...) /* */
#        define LOG_N(c, x...) /* */
#        define LOG_F(c, x...) /* */

#   define msg(aRGS...) LOG_D(PHY, ##aRGS)


extern unsigned short f1f2mat_old[2*188];

#endif  // OPENAIR_PHY_DEFS
