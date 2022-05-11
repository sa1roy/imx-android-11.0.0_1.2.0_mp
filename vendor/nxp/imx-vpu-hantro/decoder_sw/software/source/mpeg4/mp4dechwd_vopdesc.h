/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef DECVOPDESC_H_DEFINED
#define DECVOPDESC_H_DEFINED

#include "basetype.h"

typedef struct DecVopDesc_t {
  u32 vop_number;
  u32 vop_number_in_seq;
  u32 vop_time_increment;
  u32 modulo_time_base;
  u32 prev_vop_time_increment;
  u32 prev_modulo_time_base;
  u32 trb;
  u32 trd;
  u32 tics_from_prev;   /* tics (1/vop_time_increment_resolution
                             * seconds) since previous vop */
  u32 intra_dc_vlc_thr;
  u32 vop_coding_type;
  u32 total_mb_in_vop;
  u32 vop_width;   /* in macro blocks */
  u32 vop_height;  /* in macro blocks */
  u32 q_p;
  u32 fcode_fwd;
  u32 fcode_bwd;
  u32 vop_coded;
  u32 vop_rounding_type;
  /* following three parameters will be read from group of VOPs header
   * and will be updated based on time codes received in VOP header */
  u32 time_code_hours;
  u32 time_code_minutes;
  u32 time_code_seconds;
  u32 gov_counter; /* number of groups of VOPs */

  /* for interlace support */
  u32 top_field_first;
  u32 alt_vertical_scan_flag;

} DecVopDesc;

#endif
