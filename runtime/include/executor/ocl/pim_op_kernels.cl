/*
 * Copyright (C) 2022 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 * (Use of the Software is restricted to non-commercial, personal or academic, research purpose only)
 */

#define PARK_IN 1
#define CHANGE_SB_HAB 1
#define PROGRAM_CRF 1
#define CHANGE_HAB_HABPIM 1
#define COMPUTE_ELT_OP 1
#define CHANGE_HABPIM_HAB 1
#define CHANGE_HAB_SB 1
#define PARK_OUT 1

__kernel void elt_op_pim(__global uint8_t* __restrict__ operand0, __global uint8_t* __restrict__ operand1,
                         __global uint8_t* __restrict__ output, __global uint8_t* __restrict__ pim_ctr, int num_tile,
                         __global uint8_t* crf_binary, int crf_size
#ifdef EMULATOR
                         ,
                         __global PimMemTraceData* fmtd16, __global size_t* frd_size, int mt_width,
                         __global PimMemTracer* emulator_trace
#endif
                         )
{
#ifdef EMULATOR
    emulator_trace->g_fba = (uint64_t)pim_ctr;
    emulator_trace->g_fmtd16 = fmtd16;
    emulator_trace->g_ridx[get_group_id(0)] = 0;
    emulator_trace->m_width = mt_width;
    barrier(CLK_LOCAL_MEM_FENCE);
#endif
    /*
    gidx : coleasced thread index
    offset : mem address to access the 2nd half of 16 bytes.
    */
    int num_col = 32;
    int num_grf = 8;
    int num_ba = 4;
    int gidx = get_local_id(0) / 2;
    uint64_t offset = (get_local_id(0) % 2) * 0x10;
    uint64_t addr, addr_even, addr_odd;

#if PARK_IN
    park_in(pim_ctr, gidx, num_ba, offset);
#endif

    if (get_local_id(0) < 2) {
#if CHANGE_SB_HAB
        change_sb_hab(pim_ctr, offset);
#endif
#if PROGRAM_CRF
        program_crf(pim_ctr, gidx, crf_binary, offset);
#endif
#if CHANGE_HAB_HABPIM
        change_hab_habpim(pim_ctr, offset);
#endif
        B_CMD(1);
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
    if (get_local_id(0) < 16) {
#if COMPUTE_ELT_OP
        for (int tile_index = 0; tile_index < num_tile; tile_index++) {
            unsigned int loc = tile_index * num_grf + gidx;
            unsigned int row = loc / num_col;
            unsigned int col = loc % num_col;

            addr = addr_gen_(get_group_id(0), 0, 0, 0, row, col);
            addr_even = addr + offset;
            addr_odd = addr_even + 0x2000;

            /*
            compute for even bank
            1. fill to GRFA (opr0).
            2. ADD GRFA and even bank (opr1).
            3. issue NOP which inturn write data to even bank.
            */
            R_CMD(&operand0[addr_even]);
            B_CMD(1);

            R_CMD(&operand1[addr_even]);
            B_CMD(1);

            W_CMD(&output[addr_even]);
            W_CMD(&output[addr_even]);
            R_CMD(&output[addr_even]);
            B_CMD(1);

            /*
            compute for odd bank : same as even bank.
            */
            R_CMD(&operand0[addr_odd]);
            B_CMD(1);

            R_CMD(&operand1[addr_odd]);
            B_CMD(1);

            W_CMD(&output[addr_odd]);
            W_CMD(&output[addr_odd]);

            R_CMD(&output[addr_odd]);

            B_CMD(1);
        }
#endif
    }

    if (get_local_id(0) < 4) {
#if CHANGE_HABPIM_HAB
        change_habpim_hab(pim_ctr, offset);
#endif

#if CHANGE_HAB_SB
        change_hab_sb(pim_ctr, gidx, offset);
#endif
    }

#if PARK_OUT
    barrier(CLK_LOCAL_MEM_FENCE);
    park_out(pim_ctr, gidx, num_ba, offset);
#endif

#ifdef EMULATOR
    if (get_group_id(0) == 0 && get_local_id(0) == 0) {
        frd_size[0] = emulator_trace->g_ridx[0];
    }
#endif
}

#define COMPUTE_RELU 1

__kernel void relu_pim_operation(__global uint8_t* __restrict__ pim_data, __global uint8_t* __restrict__ output,
                                __global uint8_t* __restrict__ pim_ctr, int output_size, __global uint8_t* crf_binary,
                                  int crf_size

#ifdef EMULATOR
                                 ,
                                 __global PimMemTraceData* fmtd16, __global size_t* frd_size, int mt_width,
                                 __global PimMemTracer* emulator_trace
#endif
                                 )
{
#ifdef EMULATOR
    emulator_trace->g_fba = (uint64_t)pim_ctr;
    emulator_trace->g_fmtd16 = fmtd16;
    emulator_trace->g_ridx[get_group_id(0)] = 0;
    emulator_trace->m_width = mt_width;
    barrier(CLK_LOCAL_MEM_FENCE);
#endif
    int trans_size = 32;
    int num_col = 32;
    int num_pim_blocks = 8;
    int num_pim_chan = 64;
    int num_grf = 8;
    int num_ba = 4;
    int out_dim = output_size / trans_size;
    int num_tile = out_dim / (num_pim_blocks * num_pim_chan * num_grf) / 2;

    int gidx = get_local_id(0) / 2;
    uint64_t offset = (get_local_id(0) % 2) * 0x10;
    uint64_t addr, addr_even, addr_odd;

/* Radeon7(VEGA20) memory is 16GB but our target is 32GB system */
/* so program_crf and chagne_pim_mode functions can not access to over 8GB in our system */

#if PARK_IN
    park_in(pim_ctr, gidx, num_ba, offset);
#endif

    if (get_local_id(0) < 2) {
#if CHANGE_SB_HAB
        change_sb_hab(pim_ctr, offset);
#endif
#if PROGRAM_CRF
        program_crf(pim_ctr, gidx, crf_binary, offset);
#endif
#if CHANGE_HAB_HABPIM
        change_hab_habpim(pim_ctr, offset);
#endif
        B_CMD(1);
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
#if COMPUTE_RELU
    if (get_local_id(0) < 16) {
        for (int tile_idx = 0; tile_idx < num_tile; tile_idx++) {
            unsigned int loc = tile_idx * num_grf + gidx;
            unsigned int row = loc / num_col;
            unsigned int col = loc % num_col;

            addr = addr_gen_(get_group_id(0), 0, 0, 0, row, col);
            addr_even = addr + offset;
            addr_odd = addr_even + 0x2000;

            R_CMD(&pim_data[addr_even]);
            B_CMD(1);

            W_CMD(&output[addr_even]);
            R_CMD(&output[addr_even]);
            B_CMD(1);

            R_CMD(&pim_data[addr_odd]);
            B_CMD(1);

            W_CMD(&output[addr_odd]);
            R_CMD(&output[addr_odd]);
            B_CMD(1);
        }
    }
#endif

    if (get_local_id(0) < 4) {
#if CHANGE_HABPIM_HAB
        change_habpim_hab(pim_ctr, offset);
#endif

#if CHANGE_HAB_SB
        change_hab_sb(pim_ctr, gidx, offset);
#endif
    }
    barrier(CLK_LOCAL_MEM_FENCE);
#if PARK_OUT
    park_out(pim_ctr, gidx, num_ba, offset);
#endif

#ifdef EMULATOR
    if (get_group_id(0) == 0 && get_local_id(0) == 0) {
        frd_size[0] = emulator_trace->g_ridx[0];
    }
#endif
}
