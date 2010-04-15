/*****************************************************************************
 * common.h: h264 encoder
 *****************************************************************************
 * Copyright (C) 2003-2008 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef X264_COMMON_H
#define X264_COMMON_H

/****************************************************************************
 * Macros
 ****************************************************************************/
#define X264_MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define X264_MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define X264_MIN3(a,b,c) X264_MIN((a),X264_MIN((b),(c)))
#define X264_MAX3(a,b,c) X264_MAX((a),X264_MAX((b),(c)))
#define X264_MIN4(a,b,c,d) X264_MIN((a),X264_MIN3((b),(c),(d)))
#define X264_MAX4(a,b,c,d) X264_MAX((a),X264_MAX3((b),(c),(d)))
#define XCHG(type,a,b) do{ type t = a; a = b; b = t; } while(0)
#define IS_DISPOSABLE(type) ( type == X264_TYPE_B )
#define FIX8(f) ((int)(f*(1<<8)+.5))

#define CHECKED_MALLOC( var, size )\
do {\
    var = x264_malloc( size );\
    if( !var )\
        goto fail;\
} while( 0 )
#define CHECKED_MALLOCZERO( var, size )\
do {\
    CHECKED_MALLOC( var, size );\
    memset( var, 0, size );\
} while( 0 )

#define X264_BFRAME_MAX 16
#define X264_THREAD_MAX 128
#define X264_PCM_COST (386*8)
#define X264_LOOKAHEAD_MAX 250
// arbitrary, but low because SATD scores are 1/4 normal
#define X264_LOOKAHEAD_QP 12

// number of pixels (per thread) in progress at any given time.
// 16 for the macroblock in progress + 3 for deblocking + 3 for motion compensation filter + 2 for extra safety
#define X264_THREAD_HEIGHT 24

/* WEIGHTP_FAKE is set when mb_tree & psy are enabled, but normal weightp is disabled
 * (such as in baseline). It checks for fades in lookahead and adjusts qp accordingly
 * to increase quality. Defined as (-1) so that if(i_weighted_pred > 0) is true only when
 * real weights are being used. */

#define X264_WEIGHTP_FAKE (-1)

#define NALU_OVERHEAD 5 // startcode + NAL type costs 5 bytes per frame
#define FILLER_OVERHEAD (NALU_OVERHEAD+1)

/****************************************************************************
 * Includes
 ****************************************************************************/
#include "osdep.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

/* Unions for type-punning.
 * Mn: load or store n bits, aligned, native-endian
 * CPn: copy n bits, aligned, native-endian
 * we don't use memcpy for CPn because memcpy's args aren't assumed to be aligned */
typedef union { uint16_t i; uint8_t  c[2]; } MAY_ALIAS x264_union16_t;
typedef union { uint32_t i; uint16_t b[2]; uint8_t  c[4]; } MAY_ALIAS x264_union32_t;
typedef union { uint64_t i; uint32_t a[2]; uint16_t b[4]; uint8_t c[8]; } MAY_ALIAS x264_union64_t;
#define M16(src) (((x264_union16_t*)(src))->i)
#define M32(src) (((x264_union32_t*)(src))->i)
#define M64(src) (((x264_union64_t*)(src))->i)
#define CP16(dst,src) M16(dst) = M16(src)
#define CP32(dst,src) M32(dst) = M32(src)
#define CP64(dst,src) M64(dst) = M64(src)

#ifdef X264_HIGH_DEPTH_SUPPORT
    typedef uint16_t pixel_t;
# else
    typedef uint8_t pixel_t;
#endif

#include "x264.h"
#include "bs.h"
#include "set.h"
#include "predict.h"
#include "pixel.h"
#include "mc.h"
#include "frame.h"
#include "dct.h"
#include "cabac.h"
#include "quant.h"

/****************************************************************************
 * General functions
 ****************************************************************************/
/* x264_malloc : will do or emulate a memalign
 * you have to use x264_free for buffers allocated with x264_malloc */
void *x264_malloc( int );
void  x264_free( void * );

/* x264_slurp_file: malloc space for the whole file and read it */
char *x264_slurp_file( const char *filename );

/* mdate: return the current date in microsecond */
int64_t x264_mdate( void );

/* x264_param2string: return a (malloced) string containing most of
 * the encoding options */
char *x264_param2string( x264_param_t *p, int b_res );

int x264_nal_encode( uint8_t *dst, x264_nal_t *nal, int b_annexb, int b_long_startcode );

/* log */
void x264_log( x264_t *h, int i_level, const char *psz_fmt, ... );

void x264_reduce_fraction( int *n, int *d );
void x264_init_vlc_tables();

static ALWAYS_INLINE uint8_t x264_clip_uint8( int x )
{
    return x&(~255) ? (-x)>>31 : x;
}

static ALWAYS_INLINE int x264_clip3( int v, int i_min, int i_max )
{
    return ( (v < i_min) ? i_min : (v > i_max) ? i_max : v );
}

static ALWAYS_INLINE double x264_clip3f( double v, double f_min, double f_max )
{
    return ( (v < f_min) ? f_min : (v > f_max) ? f_max : v );
}

static ALWAYS_INLINE int x264_median( int a, int b, int c )
{
    int t = (a-b)&((a-b)>>31);
    a -= t;
    b += t;
    b -= (b-c)&((b-c)>>31);
    b += (a-b)&((a-b)>>31);
    return b;
}

static ALWAYS_INLINE void x264_median_mv( int16_t *dst, int16_t *a, int16_t *b, int16_t *c )
{
    dst[0] = x264_median( a[0], b[0], c[0] );
    dst[1] = x264_median( a[1], b[1], c[1] );
}

static ALWAYS_INLINE int x264_predictor_difference( int16_t (*mvc)[2], intptr_t i_mvc )
{
    int sum = 0;
    for( int i = 0; i < i_mvc-1; i++ )
    {
        sum += abs( mvc[i][0] - mvc[i+1][0] )
             + abs( mvc[i][1] - mvc[i+1][1] );
    }
    return sum;
}

static ALWAYS_INLINE uint16_t x264_cabac_mvd_sum( uint8_t *mvdleft, uint8_t *mvdtop )
{
    int amvd0 = abs(mvdleft[0]) + abs(mvdtop[0]);
    int amvd1 = abs(mvdleft[1]) + abs(mvdtop[1]);
    amvd0 = (amvd0 > 2) + (amvd0 > 32);
    amvd1 = (amvd1 > 2) + (amvd1 > 32);
    return amvd0 + (amvd1<<8);
}

extern const uint8_t x264_exp2_lut[64];
extern const float x264_log2_lut[128];
extern const float x264_log2_lz_lut[32];

/* Not a general-purpose function; multiplies input by -1/6 to convert
 * qp to qscale. */
static ALWAYS_INLINE int x264_exp2fix8( float x )
{
    int i = x*(-64.f/6.f) + 512.5f;
    if( i < 0 ) return 0;
    if( i > 1023 ) return 0xffff;
    return (x264_exp2_lut[i&63]+256) << (i>>6) >> 8;
}

static ALWAYS_INLINE float x264_log2( uint32_t x )
{
    int lz = x264_clz( x );
    return x264_log2_lut[(x<<lz>>24)&0x7f] + x264_log2_lz_lut[lz];
}

/****************************************************************************
 *
 ****************************************************************************/
enum slice_type_e
{
    SLICE_TYPE_P  = 0,
    SLICE_TYPE_B  = 1,
    SLICE_TYPE_I  = 2,
    SLICE_TYPE_SP = 3,
    SLICE_TYPE_SI = 4
};

static const char slice_type_to_char[] = { 'P', 'B', 'I', 'S', 'S' };

enum sei_payload_type_e
{
    SEI_BUFFERING_PERIOD       = 0,
    SEI_PIC_TIMING             = 1,
    SEI_PAN_SCAN_RECT          = 2,
    SEI_FILLER                 = 3,
    SEI_USER_DATA_REGISTERED   = 4,
    SEI_USER_DATA_UNREGISTERED = 5,
    SEI_RECOVERY_POINT         = 6,
};

typedef struct
{
    x264_sps_t *sps;
    x264_pps_t *pps;

    int i_type;
    int i_first_mb;
    int i_last_mb;

    int i_pps_id;

    int i_frame_num;

    int b_mbaff;
    int b_field_pic;
    int b_bottom_field;

    int i_idr_pic_id;   /* -1 if nal_type != 5 */

    int i_poc;
    int i_delta_poc_bottom;

    int i_delta_poc[2];
    int i_redundant_pic_cnt;

    int b_direct_spatial_mv_pred;

    int b_num_ref_idx_override;
    int i_num_ref_idx_l0_active;
    int i_num_ref_idx_l1_active;

    int b_ref_pic_list_reordering_l0;
    int b_ref_pic_list_reordering_l1;
    struct
    {
        int idc;
        int arg;
    } ref_pic_list_order[2][16];

    /* P-frame weighting */
    x264_weight_t weight[32][3];

    int i_mmco_remove_from_end;
    int i_mmco_command_count;
    struct /* struct for future expansion */
    {
        int i_difference_of_pic_nums;
        int i_poc;
    } mmco[16];

    int i_cabac_init_idc;

    int i_qp;
    int i_qp_delta;
    int b_sp_for_swidth;
    int i_qs_delta;

    /* deblocking filter */
    int i_disable_deblocking_filter_idc;
    int i_alpha_c0_offset;
    int i_beta_offset;

} x264_slice_header_t;

typedef struct x264_lookahead_t
{
    volatile uint8_t              b_exit_thread;
    uint8_t                       b_thread_active;
    uint8_t                       b_analyse_keyframe;
    int                           i_last_keyframe;
    int                           i_slicetype_length;
    x264_frame_t                  *last_nonb;
    x264_synch_frame_list_t       ifbuf;
    x264_synch_frame_list_t       next;
    x264_synch_frame_list_t       ofbuf;
} x264_lookahead_t;

/* From ffmpeg
 */
#define X264_SCAN8_SIZE (6*8)
#define X264_SCAN8_LUMA_SIZE (5*8)
#define X264_SCAN8_0 (4+1*8)

static const int x264_scan8[16+2*4+3] =
{
    /* Luma */
    4+1*8, 5+1*8, 4+2*8, 5+2*8,
    6+1*8, 7+1*8, 6+2*8, 7+2*8,
    4+3*8, 5+3*8, 4+4*8, 5+4*8,
    6+3*8, 7+3*8, 6+4*8, 7+4*8,

    /* Cb */
    1+1*8, 2+1*8,
    1+2*8, 2+2*8,

    /* Cr */
    1+4*8, 2+4*8,
    1+5*8, 2+5*8,

    /* Luma DC */
    4+5*8,

    /* Chroma DC */
    5+5*8, 6+5*8
};
/*
   0 1 2 3 4 5 6 7
 0
 1   B B   L L L L
 2   B B   L L L L
 3         L L L L
 4   R R   L L L L
 5   R R   DyDuDv
*/

typedef struct x264_ratecontrol_t   x264_ratecontrol_t;

struct x264_t
{
    /* encoder parameters */
    x264_param_t    param;

    x264_t          *thread[X264_THREAD_MAX+1];
    x264_pthread_t  thread_handle;
    int             b_thread_active;
    int             i_thread_phase; /* which thread to use for the next frame */
    int             i_threadslice_start; /* first row in this thread slice */
    int             i_threadslice_end; /* row after the end of this thread slice */

    /* bitstream output */
    struct
    {
        int         i_nal;
        int         i_nals_allocated;
        x264_nal_t  *nal;
        int         i_bitstream;    /* size of p_bitstream */
        uint8_t     *p_bitstream;   /* will hold data for all nal */
        bs_t        bs;
    } out;

    uint8_t *nal_buffer;
    int      nal_buffer_size;

    /**** thread synchronization starts here ****/

    /* frame number/poc */
    int             i_frame;
    int             i_frame_num;

    int             i_thread_frames; /* Number of different frames being encoded by threads;
                                      * 1 when sliced-threads is on. */
    int             i_nal_type;
    int             i_nal_ref_idc;

    int             i_disp_fields;  /* Number of displayed fields (both coded and implied via pic_struct) */
    int             i_disp_fields_last_frame;
    int             i_prev_duration; /* Duration of previous frame */
    int             i_coded_fields; /* Number of coded fields (both coded and implied via pic_struct) */
    int             i_cpb_delay;    /* Equal to number of fields preceding this field
                                     * since last buffering_period SEI */
    int             i_coded_fields_lookahead; /* Use separate counters for lookahead */
    int             i_cpb_delay_lookahead;

    /* We use only one SPS and one PPS */
    x264_sps_t      sps_array[1];
    x264_sps_t      *sps;
    x264_pps_t      pps_array[1];
    x264_pps_t      *pps;
    int             i_idr_pic_id;

    /* Timebase multiplier for DTS compression */
    int             i_dts_compress_multiplier;

    /* quantization matrix for decoding, [cqm][qp%6][coef] */
    int             (*dequant4_mf[4])[16];   /* [4][6][16] */
    int             (*dequant8_mf[2])[64];   /* [2][6][64] */
    /* quantization matrix for trellis, [cqm][qp][coef] */
    int             (*unquant4_mf[4])[16];   /* [4][52][16] */
    int             (*unquant8_mf[2])[64];   /* [2][52][64] */
    /* quantization matrix for deadzone */
    uint16_t        (*quant4_mf[4])[16];     /* [4][52][16] */
    uint16_t        (*quant8_mf[2])[64];     /* [2][52][64] */
    uint16_t        (*quant4_bias[4])[16];   /* [4][52][16] */
    uint16_t        (*quant8_bias[2])[64];   /* [2][52][64] */

    /* mv/ref cost arrays.  Indexed by lambda instead of
     * qp because, due to rounding, some quantizers share
     * lambdas.  This saves memory. */
    uint16_t *cost_mv[92];
    uint16_t *cost_mv_fpel[92][4];

    const uint8_t   *chroma_qp_table; /* includes both the nonlinear luma->chroma mapping and chroma_qp_offset */

    /* Slice header */
    x264_slice_header_t sh;

    /* cabac context */
    x264_cabac_t    cabac;

    struct
    {
        /* Frames to be encoded (whose types have been decided) */
        x264_frame_t **current;
        /* Unused frames: 0 = fenc, 1 = fdec */
        x264_frame_t **unused[2];

        /* Unused blank frames (for duplicates) */
        x264_frame_t **blank_unused;

        /* frames used for reference + sentinels */
        x264_frame_t *reference[16+2];

        int i_last_keyframe; /* Frame number of the last keyframe */

        int i_input;    /* Number of input frames already accepted */

        int i_max_dpb;  /* Number of frames allocated in the decoded picture buffer */
        int i_max_ref0;
        int i_max_ref1;
        int i_delay;    /* Number of frames buffered for B reordering */
        int     i_bframe_delay;
        int64_t i_bframe_delay_time;
        int64_t i_init_delta;
        int64_t i_prev_reordered_pts[2];
        int64_t i_largest_pts;
        int64_t i_second_largest_pts;
        int b_have_lowres;  /* Whether 1/2 resolution luma planes are being used */
        int b_have_sub8x8_esa;
    } frames;

    /* current frame being encoded */
    x264_frame_t    *fenc;

    /* frame being reconstructed */
    x264_frame_t    *fdec;

    /* references lists */
    int             i_ref0;
    x264_frame_t    *fref0[16+3];     /* ref list 0 */
    int             i_ref1;
    x264_frame_t    *fref1[16+3];     /* ref list 1 */
    int             b_ref_reorder[2];

    /* hrd */
    int initial_cpb_removal_delay;
    int initial_cpb_removal_delay_offset;
    int64_t first_pts;

    /* Current MB DCT coeffs */
    struct
    {
        ALIGNED_16( int16_t luma16x16_dc[16] );
        ALIGNED_16( int16_t chroma_dc[2][4] );
        // FIXME share memory?
        ALIGNED_16( int16_t luma8x8[4][64] );
        ALIGNED_16( int16_t luma4x4[16+8][16] );
    } dct;

    /* MB table and cache for current frame/mb */
    struct
    {
        int     i_mb_count;                 /* number of mbs in a frame */

        /* Strides */
        int     i_mb_stride;
        int     i_b8_stride;
        int     i_b4_stride;

        /* Current index */
        int     i_mb_x;
        int     i_mb_y;
        int     i_mb_xy;
        int     i_b8_xy;
        int     i_b4_xy;

        /* Search parameters */
        int     i_me_method;
        int     i_subpel_refine;
        int     b_chroma_me;
        int     b_trellis;
        int     b_noise_reduction;
        int     b_dct_decimate;
        int     i_psy_rd; /* Psy RD strength--fixed point value*/
        int     i_psy_trellis; /* Psy trellis strength--fixed point value*/

        int     b_interlaced;

        /* Allowed qpel MV range to stay within the picture + emulated edge pixels */
        int     mv_min[2];
        int     mv_max[2];
        /* Subpel MV range for motion search.
         * same mv_min/max but includes levels' i_mv_range. */
        int     mv_min_spel[2];
        int     mv_max_spel[2];
        /* Fullpel MV range for motion search */
        int     mv_min_fpel[2];
        int     mv_max_fpel[2];

        /* neighboring MBs */
        unsigned int i_neighbour;
        unsigned int i_neighbour8[4];       /* neighbours of each 8x8 or 4x4 block that are available */
        unsigned int i_neighbour4[16];      /* at the time the block is coded */
        unsigned int i_neighbour_intra;     /* for constrained intra pred */
        int     i_mb_type_top;
        int     i_mb_type_left;
        int     i_mb_type_topleft;
        int     i_mb_type_topright;
        int     i_mb_prev_xy;
        int     i_mb_top_xy;

        /**** thread synchronization ends here ****/
        /* subsequent variables are either thread-local or constant,
         * and won't be copied from one thread to another */

        /* mb table */
        int8_t  *type;                      /* mb type */
        uint8_t *partition;                 /* mb partition */
        int8_t  *qp;                        /* mb qp */
        int16_t *cbp;                       /* mb cbp: 0x0?: luma, 0x?0: chroma, 0x100: luma dc, 0x0200 and 0x0400: chroma dc  (all set for PCM)*/
        int8_t  (*intra4x4_pred_mode)[8];   /* intra4x4 pred mode. for non I4x4 set to I_PRED_4x4_DC(2) */
                                            /* actually has only 7 entries; set to 8 for write-combining optimizations */
        uint8_t (*non_zero_count)[16+4+4];  /* nzc. for I_PCM set to 16 */
        int8_t  *chroma_pred_mode;          /* chroma_pred_mode. cabac only. for non intra I_PRED_CHROMA_DC(0) */
        int16_t (*mv[2])[2];                /* mb mv. set to 0 for intra mb */
        uint8_t (*mvd[2])[8][2];            /* absolute value of mb mv difference with predict, clipped to [0,33]. set to 0 if intra. cabac only */
        int8_t   *ref[2];                   /* mb ref. set to -1 if non used (intra or Lx only) */
        int16_t (*mvr[2][32])[2];           /* 16x16 mv for each possible ref */
        int8_t  *skipbp;                    /* block pattern for SKIP or DIRECT (sub)mbs. B-frames + cabac only */
        int8_t  *mb_transform_size;         /* transform_size_8x8_flag of each mb */
        uint8_t *intra_border_backup[2][3]; /* bottom pixels of the previous mb row, used for intra prediction after the framebuffer has been deblocked */

         /* buffer for weighted versions of the reference frames */
        uint8_t *p_weight_buf[16];

        /* current value */
        int     i_type;
        int     i_partition;
        ALIGNED_4( uint8_t i_sub_partition[4] );
        int     b_transform_8x8;

        int     i_cbp_luma;
        int     i_cbp_chroma;

        int     i_intra16x16_pred_mode;
        int     i_chroma_pred_mode;

        /* skip flags for i4x4 and i8x8
         * 0 = encode as normal.
         * 1 (non-RD only) = the DCT is still in h->dct, restore fdec and skip reconstruction.
         * 2 (RD only) = the DCT has since been overwritten by RD; restore that too. */
        int i_skip_intra;
        /* skip flag for motion compensation */
        /* if we've already done MC, we don't need to do it again */
        int b_skip_mc;
        /* set to true if we are re-encoding a macroblock. */
        int b_reencode_mb;
        int ip_offset; /* Used by PIR to offset the quantizer of intra-refresh blocks. */

        struct
        {
            /* space for p_fenc and p_fdec */
#define FENC_STRIDE 16
#define FDEC_STRIDE 32
            ALIGNED_16( pixel_t fenc_buf[24*FENC_STRIDE] );
            ALIGNED_16( pixel_t fdec_buf[27*FDEC_STRIDE] );

            /* i4x4 and i8x8 backup data, for skipping the encode stage when possible */
            ALIGNED_16( pixel_t i4x4_fdec_buf[16*16] );
            ALIGNED_16( pixel_t i8x8_fdec_buf[16*16] );
            ALIGNED_16( int16_t i8x8_dct_buf[3][64] );
            ALIGNED_16( int16_t i4x4_dct_buf[15][16] );
            uint32_t i4x4_nnz_buf[4];
            uint32_t i8x8_nnz_buf[4];
            int i4x4_cbp;
            int i8x8_cbp;

            /* Psy trellis DCT data */
            ALIGNED_16( int16_t fenc_dct8[4][64] );
            ALIGNED_16( int16_t fenc_dct4[16][16] );

            /* Psy RD SATD/SA8D scores cache */
            ALIGNED_16( uint64_t fenc_hadamard_cache[9] );
            ALIGNED_16( uint32_t fenc_satd_cache[32] );

            /* pointer over mb of the frame to be compressed */
            pixel_t *p_fenc[3];
            /* pointer to the actual source frame, not a block copy */
            pixel_t *p_fenc_plane[3];

            /* pointer over mb of the frame to be reconstructed  */
            pixel_t *p_fdec[3];

            /* pointer over mb of the references */
            int i_fref[2];
            pixel_t *p_fref[2][32][4+2]; /* last: lN, lH, lV, lHV, cU, cV */
            pixel_t *p_fref_w[32];  /* weighted fullpel luma */
            uint16_t *p_integral[2][16];

            /* fref stride */
            int     i_stride[3];
        } pic;

        /* cache */
        struct
        {
            /* real intra4x4_pred_mode if I_4X4 or I_8X8, I_PRED_4x4_DC if mb available, -1 if not */
            ALIGNED_8( int8_t intra4x4_pred_mode[X264_SCAN8_LUMA_SIZE] );

            /* i_non_zero_count if available else 0x80 */
            ALIGNED_4( uint8_t non_zero_count[X264_SCAN8_SIZE] );

            /* -1 if unused, -2 if unavailable */
            ALIGNED_4( int8_t ref[2][X264_SCAN8_LUMA_SIZE] );

            /* 0 if not available */
            ALIGNED_16( int16_t mv[2][X264_SCAN8_LUMA_SIZE][2] );
            ALIGNED_8( uint8_t mvd[2][X264_SCAN8_LUMA_SIZE][2] );

            /* 1 if SKIP or DIRECT. set only for B-frames + CABAC */
            ALIGNED_4( int8_t skip[X264_SCAN8_LUMA_SIZE] );

            ALIGNED_4( int16_t direct_mv[2][4][2] );
            ALIGNED_4( int8_t  direct_ref[2][4] );
            int     direct_partition;
            ALIGNED_4( int16_t pskip_mv[2] );

            /* number of neighbors (top and left) that used 8x8 dct */
            int     i_neighbour_transform_size;
            int     i_neighbour_interlaced;

            /* neighbor CBPs */
            int     i_cbp_top;
            int     i_cbp_left;
        } cache;

        /* */
        int     i_qp;       /* current qp */
        int     i_chroma_qp;
        int     i_last_qp;  /* last qp */
        int     i_last_dqp; /* last delta qp */
        int     b_variable_qp; /* whether qp is allowed to vary per macroblock */
        int     b_lossless;
        int     b_direct_auto_read; /* take stats for --direct auto from the 2pass log */
        int     b_direct_auto_write; /* analyse direct modes, to use and/or save */

        /* lambda values */
        int     i_trellis_lambda2[2][2]; /* [luma,chroma][inter,intra] */
        int     i_psy_rd_lambda;
        int     i_chroma_lambda2_offset;

        /* B_direct and weighted prediction */
        int16_t dist_scale_factor_buf[2][32][4];
        int16_t (*dist_scale_factor)[4];
        int8_t bipred_weight_buf[2][32][4];
        int8_t (*bipred_weight)[4];
        /* maps fref1[0]'s ref indices into the current list0 */
#define map_col_to_list0(col) h->mb.map_col_to_list0[(col)+2]
        int8_t  map_col_to_list0[18];
        int ref_blind_dupe; /* The index of the blind reference frame duplicate. */
    } mb;

    /* rate control encoding only */
    x264_ratecontrol_t *rc;

    /* stats */
    struct
    {
        /* Current frame stats */
        struct
        {
            /* MV bits (MV+Ref+Block Type) */
            int i_mv_bits;
            /* Texture bits (DCT coefs) */
            int i_tex_bits;
            /* ? */
            int i_misc_bits;
            /* MB type counts */
            int i_mb_count[19];
            int i_mb_count_i;
            int i_mb_count_p;
            int i_mb_count_skip;
            int i_mb_count_8x8dct[2];
            int i_mb_count_ref[2][32];
            int i_mb_partition[17];
            int i_mb_cbp[6];
            int i_mb_pred_mode[3][13];
            /* Adaptive direct mv pred */
            int i_direct_score[2];
            /* Metrics */
            int64_t i_ssd[3];
            double f_ssim;
        } frame;

        /* Cumulated stats */

        /* per slice info */
        int     i_frame_count[5];
        int64_t i_frame_size[5];
        double  f_frame_qp[5];
        int     i_consecutive_bframes[X264_BFRAME_MAX+1];
        /* */
        int64_t i_ssd_global[5];
        double  f_psnr_average[5];
        double  f_psnr_mean_y[5];
        double  f_psnr_mean_u[5];
        double  f_psnr_mean_v[5];
        double  f_ssim_mean_y[5];
        /* */
        int64_t i_mb_count[5][19];
        int64_t i_mb_partition[2][17];
        int64_t i_mb_count_8x8dct[2];
        int64_t i_mb_count_ref[2][2][32];
        int64_t i_mb_cbp[6];
        int64_t i_mb_pred_mode[3][13];
        /* */
        int     i_direct_score[2];
        int     i_direct_frames[2];
        /* num p-frames weighted */
        int     i_wpred[3];

    } stat;

    ALIGNED_16( uint32_t nr_residual_sum[2][64] );
    ALIGNED_16( uint16_t nr_offset[2][64] );
    uint32_t        nr_count[2];

    void *scratch_buffer; /* for any temporary storage that doesn't want repeated malloc */

    /* CPU functions dependents */
    x264_predict_t      predict_16x16[4+3];
    x264_predict_t      predict_8x8c[4+3];
    x264_predict8x8_t   predict_8x8[9+3];
    x264_predict_t      predict_4x4[9+3];
    x264_predict_8x8_filter_t predict_8x8_filter;

    x264_pixel_function_t pixf;
    x264_mc_functions_t   mc;
    x264_dct_function_t   dctf;
    x264_zigzag_function_t zigzagf;
    x264_quant_function_t quantf;
    x264_deblock_function_t loopf;

#ifdef HAVE_VISUALIZE
    struct visualize_t *visualize;
#endif
    x264_lookahead_t *lookahead;
};

// included at the end because it needs x264_t
#include "macroblock.h"
#include "rectangle.h"

#ifdef HAVE_MMX
#include "x86/util.h"
#endif

#endif

