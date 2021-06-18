/*
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#ifndef _PIM_INFO_H_
#define _PIM_INFO_H_

typedef enum __PimAddrMap {
    AMDGPU_VEGA20,
} PimAddrMap;

typedef struct __PimBlockInfo {
    PimAddrMap pim_addr_map;
    int num_banks;
    int num_bank_groups;
    int num_rank_bit;
    int num_row_bit;
    int num_col_high_bit;
    int num_bank_high_bit;
    int num_bankgroup_bit;
    int num_bank_low_bit;
    int num_chan_bit;
    int num_col_low_bit;
    int num_offset_bit;
    int num_grf;
    int num_grf_A;
    int num_grf_B;
    int num_srf;
    int num_col;
    int num_row;
    int bl;
    int num_pim_blocks;
    int num_pim_rank;
    int num_pim_chan;
    int trans_size;
    int num_out_per_grf;
} PimBlockInfo;

typedef struct __PimMemTraceData {
    uint8_t data[32];
    uint64_t addr;
    int block_id;
    int thread_id;
    char cmd;
} PimMemTraceData;

typedef enum __PimBankType {
    EVEN_BANK,
    ODD_BANK,
    ALL_BANK,
} PimBankType;

typedef enum __PimMode {
    SB_MODE,
    HAB_MODE,
    HAB_PIM_MODE,
} PimMode;

#ifdef EMULATOR
typedef struct __PimMemTracer {
    uint64_t g_fba;
    PimMemTraceData* g_fmtd16;
    int g_ridx[64];
    int g_idx[64];
    int m_width;
} PimMemTracer;
#endif

#endif /* _PIM_INFO_H_ */