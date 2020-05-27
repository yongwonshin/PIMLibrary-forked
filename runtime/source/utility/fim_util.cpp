#include "utility/fim_util.h"

__host__ void get_fim_block_info(FimBlockInfo* fbi) { memcpy(fbi, &vega20_fbi, sizeof(FimBlockInfo)); }

__host__ __device__ void reduce_sum_for_gemv(void* out, void* in, int out_size, int reduce_size)
{
    half t_output;
    half* output = (half*)out;
    half* input = (half*)in;
    int out_num = out_size / sizeof(half) / reduce_size;

    printf("out_num:%d,  out_size:%d,  reduce_size:%d\n", out_num, out_size, reduce_size);

    for (int i = 0; i < out_num; i++) {
        t_output = 0;
        for (int j = 0; j < reduce_size; j++) {
            t_output += input[j];
        }
        output[i] = t_output;
        input += reduce_size;
    }
}

__host__ __device__ uint32_t mask_by_bit(uint32_t value, uint32_t start, uint32_t end)
{
    int length = start - end + 1;
    value = value >> end;
    return value & ((1 << length) - 1);
}

__host__ __device__ uint64_t addr_gen(uint32_t chan, uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t row,
                                      uint32_t col)
{
    uint64_t addr = 0;

    FimBlockInfo* fbi = &vega20_fbi;

    addr = rank;

    addr <<= fbi->num_row_bit;
    addr |= row;

    addr <<= fbi->num_col_high_bit;
    addr |= mask_by_bit(col, 4, 2);

    addr <<= fbi->num_bank_high_bit;
    addr |= mask_by_bit(bank, 1, 1);

    addr <<= fbi->num_bankgroup_bit;
    addr |= bankgroup;

    addr <<= fbi->num_bank_low_bit;
    addr |= mask_by_bit(bank, 0, 0);

    addr <<= fbi->num_chan_bit - 1;
    addr |= mask_by_bit(chan, fbi->num_chan_bit - 1, 1);

    addr <<= 1;
    addr |= mask_by_bit(col, 1, 1);

    addr <<= 1;
    addr |= mask_by_bit(chan, 0, 0);

    addr <<= 1;
    addr |= mask_by_bit(col, 0, 0);

    addr <<= fbi->num_offset_bit;

    return addr;
}

__host__ __device__ uint64_t addr_gen_safe(uint32_t chan, uint32_t rank, uint32_t bg, uint32_t bank, uint32_t& row,
                                           uint32_t& col)
{
    FimBlockInfo* fbi = &vega20_fbi;

    while (col >= fbi->num_col / fbi->bl) {
        row++;
        col -= (fbi->num_col / fbi->bl);
    }

    if (row >= fbi->num_row) {
    }

    return addr_gen(chan, rank, bg, bank, row, col);
}

#ifdef EMULATOR
extern uint64_t g_fba;
extern int g_ridx;
extern int g_idx[64];
extern int m_width;
extern FimMemTraceData* g_fmtd16;

__device__ void GEN_WRITE_CMD(volatile uint8_t* __restrict__ dst, volatile uint8_t* __restrict__ src)
{
    int bid = hipBlockIdx_x;
    int tid = hipThreadIdx_x;
    int ridx = atomicAdd(&g_ridx, 1);

    g_fmtd16[ridx].block_id = bid;
    g_fmtd16[ridx].thread_id = tid;
    g_fmtd16[ridx].cmd = 'W';
    g_fmtd16[ridx].addr = (uint64_t)dst - g_fba;
    memcpy(g_fmtd16[ridx].data, (uint8_t*)src, 16);
}

__device__ void GEN_READ_CMD(volatile uint8_t* __restrict__ dst, volatile uint8_t* __restrict__ src, bool is_output)
{
    int bid = hipBlockIdx_x;
    int tid = hipThreadIdx_x;
    int ridx = atomicAdd(&g_ridx, 1);

    g_fmtd16[ridx].block_id = bid;
    g_fmtd16[ridx].thread_id = tid;
    g_fmtd16[ridx].cmd = (is_output == true) ? 'O' : 'R';
    g_fmtd16[ridx].addr = (uint64_t)src - g_fba;
}

__device__ void BLOCK_SYNC(int cu_ch_idx, bool block_all_chan)
{
    __syncthreads();
    if (hipThreadIdx_x % 2 == 0) {
        FimBlockInfo* fbi = &vega20_fbi;
        int tid = hipThreadIdx_x;
        int num_chan = fbi->num_fim_chan;
        int ridx;

        if (block_all_chan == true) {
            for (int cidx = 0; cidx < num_chan; cidx++) {
                ridx = atomicAdd(&g_ridx, 1);
                g_fmtd16[ridx].block_id = cidx;
                g_fmtd16[ridx].thread_id = tid;
                g_fmtd16[ridx].cmd = 'B';
                g_fmtd16[ridx].addr = 0;
            }
        } else {
            ridx = atomicAdd(&g_ridx, 1);
            g_fmtd16[ridx].block_id = cu_ch_idx;
            g_fmtd16[ridx].thread_id = tid;
            g_fmtd16[ridx].cmd = 'B';
            g_fmtd16[ridx].addr = 0;
        }
    }
}

#else /* TARGET */

__device__ void GEN_WRITE_CMD(volatile uint8_t* __restrict__ dst, volatile uint8_t* __restrict__ src)
{
    asm volatile("global_store_dwordx4 %0, v[27:30], off\n\t" ::"v"(dst) : "v27", "v28", "v29", "v30");
}

__device__ void GEN_READ_CMD(volatile uint8_t* __restrict__ dst, volatile uint8_t* __restrict__ src,
                             bool is_output = false)
{
    asm volatile("global_load_dwordx4 v[27:30], %0, off\n\t" ::"v"(src) : "v27", "v28", "v29", "v30");
}

__device__ void BLOCK_SYNC(void) { __syncthreads(); }

#endif /* EMULATOR */

__device__ void add_transaction_all_1cu_2th(volatile uint8_t* __restrict__ fim_addr, bool is_write, uint32_t bg,
                                            uint32_t bank, uint32_t row, uint32_t col, uint8_t* burst, uint64_t offset,
                                            int loop_cnt)
{
    uint64_t t_addr;
    FimBlockInfo* fbi = &vega20_fbi;

    for (int cidx = 0; cidx < fbi->num_fim_chan; cidx++) {
        for (int rank = 0; rank < fbi->num_fim_rank; rank++) {
            uint32_t local_row = row;
            uint32_t local_col = col;
            for (int lc = 0; lc < loop_cnt; lc++) {
                t_addr = addr_gen_safe(cidx, rank, bg, bank, local_row, local_col);
                if (is_write) {
                    GEN_WRITE_CMD(&fim_addr[t_addr + offset], burst + offset);
                } else {
                    GEN_READ_CMD(null_bst + offset, &fim_addr[t_addr + offset]);
                }
                local_col++;
            }
        }
    }
    BLOCK_SYNC();
}

__device__ void change_fim_mode_1cu_2th(volatile uint8_t* __restrict__ fim_ctr, FimMode mode1, FimMode mode2,
                                        uint8_t* change_mode_bin, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
#ifdef EMULATOR
    /* RA13 and RA12 is swapped in Aquabolt-XL core-die, we need to emulate this behavior in emulator mode */
    /* 0x17ff : RA12<->RA13 swapped address in vega20 memory map */
    uint32_t hab_row_addr = 0x17ff;
    uint32_t sb_row_addr = 0x1fff;
#else  /* TARGET */
    uint32_t hab_row_addr = 0x27ff;
    uint32_t sb_row_addr = 0x2fff;
#endif /* EMULATOR */

    if (mode1 == SB_MODE) {
        if (mode2 == HAB_MODE) {
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 0, hab_row_addr, 0x1f, change_mode_bin, offset);
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 1, hab_row_addr, 0x1f, change_mode_bin, offset);
            if (fbi->num_banks >= 2) {
                add_transaction_all_1cu_2th(fim_ctr, true, 2, 0, hab_row_addr, 0x1f, change_mode_bin, offset);
                add_transaction_all_1cu_2th(fim_ctr, true, 2, 1, hab_row_addr, 0x1f, change_mode_bin, offset);
            }
        }
    } else if (mode1 == HAB_MODE) {
        if (mode2 == SB_MODE) {
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 0, sb_row_addr, 0x1f, change_mode_bin, offset);
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 1, sb_row_addr, 0x1f, change_mode_bin, offset);
        } else if (mode2 == HAB_FIM_MODE) {
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 0, 0x3fff, 0x0, change_mode_bin, offset);
        }
    } else if (mode1 == HAB_FIM_MODE) {
        if (mode2 == HAB_MODE) {
            add_transaction_all_1cu_2th(fim_ctr, true, 0, 0, 0x3fff, 0x0, change_mode_bin, offset);
        }
    }
    BLOCK_SYNC();
}

__device__ void park_in_1cu_2th(volatile uint8_t* __restrict__ fim_ctr, uint64_t offset)
{
    uint32_t cidx;
    uint32_t rank;
    uint32_t b;
    uint32_t bg;
    uint64_t t_addr;

    FimBlockInfo* fbi = &vega20_fbi;

    BLOCK_SYNC();
    for (cidx = 0; cidx < fbi->num_fim_chan; cidx++) {
        for (rank = 0; rank < fbi->num_fim_rank; rank++) {
            for (b = 0; b < fbi->num_banks / fbi->num_bank_groups; b++) {
                for (bg = 0; bg < fbi->num_bank_groups; bg++) {
                    t_addr = addr_gen(cidx, rank, bg, b, (1 << 12), 0);
                    GEN_READ_CMD(null_bst + offset, &fim_ctr[t_addr + offset]);
                }
            }
        }
    }
    BLOCK_SYNC();
}

__device__ void park_out_1cu_2th(volatile uint8_t* __restrict__ fim_ctr, uint64_t offset)
{
    uint32_t cidx;
    uint32_t rank;
    uint64_t t_addr;
    FimBlockInfo* fbi = &vega20_fbi;

    for (cidx = 0; cidx < fbi->num_fim_chan; cidx++) {
        for (rank = 0; rank < fbi->num_fim_rank; rank++) {
            t_addr = addr_gen(cidx, rank, 0, 0, (1 << 12), 0);
            GEN_READ_CMD(null_bst + offset, &fim_ctr[t_addr + offset]);

            t_addr = addr_gen(cidx, rank, 0, 1, (1 << 12), 0);
            GEN_READ_CMD(null_bst + offset, &fim_ctr[t_addr + offset]);
        }
    }
    BLOCK_SYNC();
}

__device__ void program_srf_1cu_2th(volatile uint8_t* __restrict__ fim_ctr, uint8_t* srf_bin, uint32_t srf_bin_size,
                                    uint64_t offset)
{
    uint32_t cidx;
    uint32_t rank;
    uint64_t t_ctr_addr;
    uint64_t t_srf_addr;
    FimBlockInfo* fbi = &vega20_fbi;
    uint32_t trans_size = fbi->trans_size;

    for (cidx = 0; cidx < fbi->num_fim_chan; cidx++) {
        for (rank = 0; rank < fbi->num_fim_rank; rank++) {
            t_ctr_addr = addr_gen(cidx, rank, 0, 0, 0x3fff, 0x1);
            t_srf_addr = (cidx * fbi->num_fim_rank + rank) * trans_size;
            GEN_WRITE_CMD(&fim_ctr[t_ctr_addr + offset], srf_bin + t_srf_addr + offset);
        }
    }
    BLOCK_SYNC();
}

__device__ void program_crf_1cu_2th(volatile uint8_t* __restrict__ fim_ctr, uint8_t* crf_bin, uint32_t cmd_size,
                                    uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;

    for (int i = 0; i < cmd_size; i += fbi->trans_size) {
        add_transaction_all_1cu_2th(fim_ctr, true, 0, 1, 0x3fff, 0x4 + i, crf_bin + i, offset);
    }
}

__device__ void compute_elt_op_1cu_2th(volatile uint8_t* __restrict__ fim_input0,
                                       volatile uint8_t* __restrict__ fim_input1,
                                       volatile uint8_t* __restrict__ fim_output, int num_tile, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;

    for (int i = 0; i < num_tile; i++) {
        add_transaction_all_1cu_2th(fim_input0, false, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_input1, false, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);

        add_transaction_all_1cu_2th(fim_input0, false, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_input1, false, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 1, 0, fbi->num_grf * i + fbi->num_grf - 1, null_bst, offset,
                                    1);
    }
}

__device__ void compute_relu_1cu_2th(volatile uint8_t* __restrict__ fim_output, volatile uint8_t* __restrict__ fim_data,
                                     int num_tile, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;

    for (int i = 0; i < num_tile; i++) {
        add_transaction_all_1cu_2th(fim_data, false, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, false, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_output, true, 0, 1, 0, fbi->num_grf * i + fbi->num_grf - 1, null_bst, offset,
                                    1);
    }
}

__device__ void compute_bn_1cu_2th(volatile uint8_t* __restrict__ fim_data, int num_tile, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
    for (int i = 0; i < num_tile; i++) {
        add_transaction_all_1cu_2th(fim_data, false, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, false, 0, 0, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, true, 0, 0, 0, fbi->num_grf * (num_tile + i), null_bst, offset,
                                    fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, false, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, false, 0, 1, 0, fbi->num_grf * i, null_bst, offset, fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, true, 0, 1, 0, fbi->num_grf * (num_tile + i), null_bst, offset,
                                    fbi->num_grf);
        add_transaction_all_1cu_2th(fim_data, true, 0, 1, 0, fbi->num_grf * (num_tile + i) + fbi->num_grf - 1, null_bst,
                                    offset, 1);
    }
}

__device__ int get_num_tile(int dim)
{
    FimBlockInfo* fbi = &vega20_fbi;
    int num_parallelism = fbi->num_fim_blocks * fbi->num_fim_chan * fbi->num_fim_rank * fbi->num_grf;
    int tile = dim / num_parallelism;

    return tile;
}

__device__ int get_result_col(int dim)
{
    FimBlockInfo* fbi = &vega20_fbi;

    return dim / (fbi->num_fim_blocks * fbi->num_fim_chan * fbi->num_fim_rank);
}

__device__ int gemv_get_result_col(int input_dim, int output_dim, int num_in_tile, int num_out_tile)
{
    FimBlockInfo* fbi = &vega20_fbi;

    return num_out_tile * num_in_tile / 2 * fbi->num_grf_A * fbi->num_grf_B;
}

__device__ void read_result_1cu_2th(volatile uint8_t* __restrict__ output, volatile uint8_t* __restrict__ fim_data,
                                    FimBankType bank_type, int out_dim, uint32_t s_row, uint32_t s_col, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
    uint32_t cidx = 0;
    uint32_t rank = 0;
    uint32_t bg = 0;
    uint32_t bank = 0;
    uint32_t row = 0;
    uint32_t col = 0;
    uint64_t t_addr;

    for (int x = 0; x < out_dim; x += fbi->num_grf) {
        row = s_row;
        col = s_col;
        for (int grf_idx = 0; grf_idx < fbi->num_grf; grf_idx++) {
            t_addr = addr_gen_safe(cidx, rank, bg, bank + (uint32_t)bank_type, row, col);
            GEN_READ_CMD(output + x + grf_idx + offset, &fim_data[t_addr + offset], true);
            col++;
        }

        bank += (fbi->num_banks / fbi->num_fim_blocks);
        if (bank >= (fbi->num_banks / fbi->num_bank_groups)) {
            bg++;
            bank = 0;
        }
        if (bg >= fbi->num_bank_groups) {
            bg = 0;
            rank++;
        }
        if (rank >= fbi->num_fim_rank) {
            rank = 0;
            cidx++;
        }
        if (cidx >= fbi->num_fim_chan) {
            cidx = 0;
            s_row = row;
            s_col = col;
        }
    }
}

__device__ void read_result_2bank_1cu_2th(volatile uint8_t* __restrict__ output,
                                          volatile uint8_t* __restrict__ fim_data, int out_dim, uint32_t s_row,
                                          uint32_t s_col, uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
    uint32_t cidx = 0;
    uint32_t rank = 0;
    uint32_t bg = 0;
    uint32_t bank = 0;
    uint32_t row = 0;
    uint32_t col = 0;
    uint64_t t_addr;

    for (int x = 0; x < out_dim; x += fbi->num_grf) {
        row = s_row;
        col = s_col;

        for (int grf_idx = 0; grf_idx < fbi->num_grf; grf_idx++) {
            t_addr = addr_gen_safe(cidx, rank, bg, bank, row, col);
            GEN_READ_CMD(output + x + grf_idx + offset, &fim_data[t_addr + offset], true);
            col++;
        }

        bank++;

        if (bank >= (fbi->num_banks / fbi->num_bank_groups)) {
            bg++;
            bank = 0;
        }
        if (bg >= fbi->num_bank_groups) {
            bg = 0;
            rank++;
        }
        if (rank >= fbi->num_fim_rank) {
            rank = 0;
            cidx++;
        }
        if (cidx >= fbi->num_fim_chan) {
            cidx = 0;
            s_row = row;
            s_col = col;
        }
    }
}

__device__ void read_result_bn_1cu_2th(volatile uint8_t* __restrict__ output, volatile uint8_t* __restrict__ fim_data,
                                       int num_batch, int num_ch, int num_width, uint32_t s_row, uint32_t s_col,
                                       uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
    int cidx = 0;
    int rank = 0;
    int bg = 0;
    int bank = 0;
    uint64_t t_fim_addr;
    uint64_t t_out_addr;
    unsigned row;
    unsigned col;
    unsigned s_row_ch = s_row;
    unsigned s_col_ch = s_col;

    for (int ch = 0; ch < num_ch; ch++) {
        s_row = s_row_ch;
        s_col = s_col_ch;
        for (int b = 0; b < num_batch; b++) {
            for (int w = 0; w < num_width; w += fbi->num_grf) {
                row = s_row;
                col = s_col;
                for (int grf_idx = 0; grf_idx < fbi->num_grf; grf_idx++) {
                    t_fim_addr = addr_gen_safe(cidx, rank, bg, bank, row, col);
                    t_out_addr = (b * num_ch * num_width + ch * num_width + w + grf_idx) * fbi->trans_size;
                    GEN_READ_CMD(output + t_out_addr + offset, &fim_data[t_fim_addr + offset], true);
                    col++;
                }
                bank++;

                if (bank >= (fbi->num_banks / fbi->num_bank_groups)) {
                    bg++;
                    bank = 0;
                }
                if (bg >= fbi->num_bank_groups) {
                    bg = 0;
                    s_row = row;
                    s_col = col;
                }
            }
        }

        rank++;
        if (rank >= fbi->num_fim_rank) {
            rank = 0;
            cidx++;
        }
        if (cidx >= fbi->num_fim_chan) {
            cidx = 0;
            s_row_ch = row;
            s_col_ch = col;
        }
    }
}

__device__ void compute_gemv_2bank_1cu_2th(volatile uint8_t* __restrict__ fim_ctr,
                                           volatile uint8_t* __restrict__ fim_weight,
                                           volatile uint8_t* __restrict__ fim_input, int num_in_tile, int num_out_tile,
                                           int input_tile, int output_tile, int batch_idx, FimBankType bank_type,
                                           uint64_t offset)
{
    FimBlockInfo* fbi = &vega20_fbi;
    uint64_t c_addr;
    uint64_t i_addr;
    uint32_t row = 0;
    uint32_t col = (fbi->num_grf_A * fbi->num_grf_B) * (input_tile / 2 + output_tile * num_in_tile / 2);

    for (int cidx = 0; cidx < fbi->num_fim_chan; cidx++) {
        for (int rank = 0; rank < fbi->num_fim_rank; rank++) {
            for (int gidx = 0; gidx < fbi->num_grf_A; gidx++) {
                c_addr = addr_gen(cidx, rank, 0, 1, 0x3fff, 0x8 + gidx);
                i_addr =
                    (batch_idx * fbi->num_grf_A * num_in_tile + input_tile * fbi->num_grf_A + gidx) * fbi->trans_size;
                // i_addr = (input_tile * fbi->num_grf_A + gidx) * fbi->trans_size;
                GEN_WRITE_CMD(&fim_ctr[c_addr + offset], &fim_input[i_addr + offset]);
            }
            BLOCK_SYNC(cidx, false);
        }
    }
    add_transaction_all_1cu_2th(fim_ctr, false, 0, (int)bank_type, row, col, null_bst, offset,
                                fbi->num_grf_A * fbi->num_grf_B);
}

#ifdef EMULATOR
__device__ void R_CMD(uint8_t* addr)
{
    int row = hipBlockIdx_x * m_width;
    int midx = row + atomicAdd(&g_idx[hipBlockIdx_x], 1);

    memset(g_fmtd16[midx].data, 0, 16);
    g_fmtd16[midx].block_id = hipBlockIdx_x;
    g_fmtd16[midx].thread_id = hipThreadIdx_x;
    g_fmtd16[midx].addr = (uint64_t)addr - g_fba;
    g_fmtd16[midx].cmd = 'R';
}

__device__ void W_CMD(uint8_t* addr)
{
    int row = hipBlockIdx_x * m_width;
    int midx = row + atomicAdd(&g_idx[hipBlockIdx_x], 1);

    memset(g_fmtd16[midx].data, 0, 16);
    g_fmtd16[midx].block_id = hipBlockIdx_x;
    g_fmtd16[midx].thread_id = hipThreadIdx_x;
    g_fmtd16[midx].addr = (uint64_t)addr - g_fba;
    g_fmtd16[midx].cmd = 'W';
}

__device__ void W_CMD_R(uint8_t* addr, uint8_t* src)
{
    int row = hipBlockIdx_x * m_width;
    int midx = row + atomicAdd(&g_idx[hipBlockIdx_x], 1);

    memcpy(g_fmtd16[midx].data, (uint8_t*)src, 16);
    g_fmtd16[midx].block_id = hipBlockIdx_x;
    g_fmtd16[midx].thread_id = hipThreadIdx_x;
    g_fmtd16[midx].addr = (uint64_t)addr - g_fba;
    g_fmtd16[midx].cmd = 'W';
}

__device__ void B_CMD(int type)
{
    int row = hipBlockIdx_x * m_width;
    int midx = row + atomicAdd(&g_idx[hipBlockIdx_x], 1);

    memset(g_fmtd16[midx].data, 0, 16);
    g_fmtd16[midx].block_id = hipBlockIdx_x;
    g_fmtd16[midx].thread_id = hipThreadIdx_x;
    g_fmtd16[midx].addr = 0;
    g_fmtd16[midx].cmd = 'B';

    if (type == 0)
        __syncthreads();
    else
        __threadfence();
}

__device__ void record(int bid, char mtype, uint64_t paddr)
{
    int row = bid * m_width;
    int midx = row + g_idx[bid]++;

    memset(g_fmtd16[midx].data, 0, 16);
    g_fmtd16[midx].block_id = bid;
    g_fmtd16[midx].thread_id = 0;
    g_fmtd16[midx].addr = paddr;
    g_fmtd16[midx].cmd = mtype;
}
#else
__device__ void R_CMD(uint8_t* addr)
{
    asm volatile("global_load_dwordx4 v[24:27], %0, off\n\t" ::"v"(addr) : "v24", "v25", "v26", "v37");
}

__device__ void W_CMD(uint8_t* addr)
{
    asm volatile("global_store_dwordx4 %0, v[24:27], off\n\t" ::"v"(addr) : "v24", "v25", "v26", "v27");
}

__device__ void W_CMD_R(uint8_t* addr, uint8_t* src)
{
    if (hipThreadIdx_x == 0) {
        asm volatile("global_load_dwordx4 v[20:23], %0, off\n\t" ::"v"(src) : "v20", "v21", "v22", "v23");
        asm volatile("global_store_dwordx4 %0, v[20:23], off\n\t" ::"v"(addr) : "v20", "v21", "v22", "v23");
    } else {
        asm volatile("global_load_dwordx4 v[24:27], %0, off\n\t" ::"v"(src) : "v24", "v25", "v26", "v37");
        asm volatile("global_store_dwordx4 %0, v[24:27], off\n\t" ::"v"(addr) : "v24", "v25", "v26", "v27");
    }
}

__device__ void B_CMD(int type)
{
    if (type == 0)
        __syncthreads();
    else
        __threadfence();
}
#endif

size_t get_aligned_size(FimDesc* fim_desc, FimMemFlag mem_flag, FimBo* fim_bo)
{
    int n = fim_desc->bshape_r.n;
    int c = fim_desc->bshape_r.c;
    int h = fim_desc->bshape_r.h;
    int w = fim_desc->bshape_r.w;
    size_t size;

    if (mem_flag == GEMV_INPUT) {
        w = 256 * ceil((float)w / 256);
        h = 1;
        fim_desc->bshape.w = w;
    } else if (mem_flag == GEMV_WEIGHT) {
        fim_bo->bshape_r = {(uint32_t)w, (uint32_t)h, (uint32_t)c, (uint32_t)n};
        w = 256 * ceil((float)w / 256);
        h = 4096 * ceil((float)h / 4096);
        fim_desc->bshape.w = w;
        fim_desc->bshape.h = h;
        fim_bo->bshape = {(uint32_t)w, (uint32_t)h, (uint32_t)c, (uint32_t)n};
    } else if (mem_flag == GEMV_OUTPUT) {
        w = 1;
    } else if (mem_flag == ELT_OP) {
        w = (128 * 1024) * ceil((float)w / (128 * 1024));
        fim_desc->bshape.w = w;
    } else if (mem_flag == ELT_FIM_INPUT) {
        w = 2 * (64 * 1024) * ceil((float)w / (64 * 1024));
    }

    size = n * c * h * w;

    return size;
}

void pad_data(void* input, int in_size, int in_nsize, FimMemFlag mem_flag)
{
    if (mem_flag == GEMV_INPUT) {
        for (int i = in_size; i < in_nsize; i++) ((half*)input)[i] = half(0);
    }
}
