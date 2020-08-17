#include <stdio.h>
#include <iostream>
#include "hip/hip_runtime.h"

extern "C" uint64_t fmm_map_fim(uint32_t, uint32_t, uint64_t);

#define RADEON7 1

#define FIM_RESERVED_8GB (0x200000000)
#define FIM_RESERVED_16GB (0x400000000)
#define CHANNEL (64)
#define CH_BIT (6)

#define PREPARE_KERNEL 1
#define PARK_IN 1
#define CHANGE_SB_HAB 1
#define PROGRAM_CRF 1
#define CHANGE_HAB_HABFIM 1
#define PROGRAM_CRF 1
#define COMPUTE_ELT_OP 1
#define CHANGE_HABFIM_HAB 1
#define CHANGE_HAB_SB 1
#define PARK_OUT 1

#define CHECK(cmd)                                                                                              \
    {                                                                                                           \
        hipError_t error = cmd;                                                                                 \
        if (error != hipSuccess) {                                                                              \
            fprintf(stderr, "error: '%s'(%d) at %s:%d\n", hipGetErrorString(error), error, __FILE__, __LINE__); \
            exit(EXIT_FAILURE);                                                                                 \
        }                                                                                                       \
    }

__device__ inline void R_CMD(uint8_t* src)
{
    asm volatile("global_load_dwordx4 v[24:27], %0, off, glc, slc" ::"v"(src));
}

__device__ inline void W_CMD(uint8_t* dst)
{
    asm volatile("global_store_dwordx4 %0, v[24:27], off, glc, slc" ::"v"(dst));
}

__device__ inline void W_CMD_R(uint8_t* dst, uint8_t* src)
{
    switch (hipThreadIdx_x) {
        case 0:
            asm volatile("global_load_dwordx4 v[8:11], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[8:11], off, glc, slc" ::"v"(dst));
            break;
        case 1:
            asm volatile("global_load_dwordx4 v[12:15], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[12:15], off, glc, slc" ::"v"(dst));
            break;
        case 2:
            asm volatile("global_load_dwordx4 v[16:19], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[16:19], off, glc, slc" ::"v"(dst));
            break;
        case 3:
            asm volatile("global_load_dwordx4 v[20:23], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[20:23], off, glc, slc" ::"v"(dst));
            break;
        case 4:
            asm volatile("global_load_dwordx4 v[24:27], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[24:27], off, glc, slc" ::"v"(dst));
            break;
        case 5:
            asm volatile("global_load_dwordx4 v[28:31], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[28:31], off, glc, slc" ::"v"(dst));
            break;
        case 6:
            asm volatile("global_load_dwordx4 v[32:35], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[32:35], off, glc, slc" ::"v"(dst));
            break;
        case 7:
            asm volatile("global_load_dwordx4 v[36:39], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[36:39], off, glc, slc" ::"v"(dst));
            break;
        case 8:
            asm volatile("global_load_dwordx4 v[40:43], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[40:43], off, glc, slc" ::"v"(dst));
            break;
        case 9:
            asm volatile("global_load_dwordx4 v[44:47], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[44:47], off, glc, slc" ::"v"(dst));
            break;
        case 10:
            asm volatile("global_load_dwordx4 v[48:51], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[48:51], off, glc, slc" ::"v"(dst));
            break;
        case 11:
            asm volatile("global_load_dwordx4 v[52:55], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[52:55], off, glc, slc" ::"v"(dst));
            break;
        case 12:
            asm volatile("global_load_dwordx4 v[56:59], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[56:59], off, glc, slc" ::"v"(dst));
            break;
        case 13:
            asm volatile("global_load_dwordx4 v[60:63], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[60:63], off, glc, slc" ::"v"(dst));
            break;
        case 14:
            asm volatile("global_load_dwordx4 v[64:67], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[64:67], off, glc, slc" ::"v"(dst));
            break;
        case 15:
            asm volatile("global_load_dwordx4 v[68:71], %0, off, glc, slc" ::"v"(src));
            asm volatile("global_store_dwordx4 %0, v[68:71], off, glc, slc" ::"v"(dst));
            break;
        default:
            break;
    }
}

__device__ inline void W_CMD_R_2TH(uint8_t* dst, uint8_t* src)
{
    if (hipThreadIdx_x == 0) {
        asm volatile("global_load_dwordx4 v[20:23], %0, off, glc, slc" ::"v"(src));
        asm volatile("global_store_dwordx4 %0, v[20:23], off, glc, slc" ::"v"(dst));
    } else {
        asm volatile("global_load_dwordx4 v[24:27], %0, off, glc, slc" ::"v"(src));
        asm volatile("global_store_dwordx4 %0, v[24:27], off, glc, slc" ::"v"(dst));
    }
}

__device__ inline void B_CMD(int type) { (type == 0) ? __syncthreads() : __threadfence(); }

__device__ inline unsigned int mask_by_bit(unsigned int value, int start, int end)
{
    int length = start - end + 1;
    value = value >> end;
    return value & ((1 << length) - 1);
}

// 64CH, 32GB address map
// rank 1b | row 14b | col(msb) 3b | ba(msb) 1b | bg 2b | ba(lsb) 1b | chan(msb) 5b | col(lsb) 1b | chan[0] 1b |
// col(lsb) 1b | col(bst) 5b
__device__ uint64_t addr_gen(unsigned int ch, unsigned int rank, unsigned int bg, unsigned int ba, unsigned int row,
                             unsigned int col)
{
    int num_row_bit_ = 14;
    int num_col_high_bit_ = 3;
    int num_bank_high_bit_ = 1;
    int num_bankgroup_bit_ = 2;
    int num_bank_low_bit_ = 1;
    int num_chan_bit_ = CH_BIT;
    int num_col_low_bit_ = 1;
    int num_offset_bit_ = 5;

    uint64_t addr = rank;

    addr <<= num_row_bit_;
    addr |= row;

    addr <<= num_col_high_bit_;
    addr |= mask_by_bit(col, 4, 2);  // HARDCODED. FIXME

    addr <<= num_bank_high_bit_;
    addr |= mask_by_bit(ba, 1, 1);

    addr <<= num_bankgroup_bit_;
    addr |= bg;

    addr <<= num_bank_low_bit_;
    addr |= mask_by_bit(ba, 0, 0);

    addr <<= num_chan_bit_ - 1;
    addr |= mask_by_bit(ch, num_chan_bit_ - 1, 1);

    addr <<= 1;
    addr |= mask_by_bit(col, 1, 1);

    addr <<= 1;
    addr |= mask_by_bit(ch, 0, 0);

    addr <<= 1;
    addr |= mask_by_bit(col, 0, 0);

    addr <<= num_offset_bit_;

#ifdef RADEON7
    uint64_t mask = 0x1FFFFFFFF;
    addr &= mask;
#endif
    return addr;
}

__global__ void gemv_fim(uint8_t* fim_ctr, uint8_t* weight, uint8_t* gemv_tmp_buffer, uint8_t* input, int batch_dim,
                         int n_in_tile, int n_out_tile, uint8_t* crf_binary, int crf_size, uint8_t* hab_to_fim,
                         uint8_t* fim_to_hab)
{
#if PREPARE_KERNEL
    int num_col = 32;
    int num_bg = 4;
    int num_ba = 4;
    int num_grf = 8;
    int trans_size = 32;
    int loc, row, col;

    int num_in_tile = n_in_tile;
    int num_out_tile = n_out_tile;

    int gidx = hipThreadIdx_x / 2;
    uint64_t offset = (hipThreadIdx_x % 2) * 0x10;
    uint64_t addr;
#endif
    /* Radeon7(VEGA20) memory is 16GB but our target is 32GB system */
    /* so program_crf and chagne_fim_mode functions can not access to over 8GB in our system */
#if PARK_IN
    if (hipThreadIdx_x < num_bg * num_ba * 2) {
        addr = addr_gen(hipBlockIdx_x, 0, gidx / num_ba, gidx % num_ba, (1 << 12), 0);
        R_CMD(&fim_ctr[addr + offset]);
    }
    B_CMD(0);
#endif

#if CHANGE_SB_HAB
    if (hipThreadIdx_x < 4) {
        addr = addr_gen(hipBlockIdx_x, 0, 0, gidx, 0x27ff, 0x1f);
        W_CMD(&fim_ctr[addr + offset]);

        addr = addr_gen(hipBlockIdx_x, 0, 2, gidx, 0x27ff, 0x1f);
        W_CMD(&fim_ctr[addr + offset]);
    }
    B_CMD(0);
#endif

#if PROGRAM_CRF
    if (hipThreadIdx_x < 2 * crf_size) {
        addr = addr_gen(hipBlockIdx_x, 0, 0, 1, 0x3fff, 0x4 + gidx);
        W_CMD_R(&fim_ctr[addr + offset], crf_binary + hipThreadIdx_x * 16);
    }
    B_CMD(0);
#endif

#if COMPUTE_GEMV
    if (hipThreadIdx_x < 2 * num_grf) {
        for (int b_idx = 0; b_idx < batch_dim; b_idx++) {
            for (int o_idx = 0; o_idx < num_out_tile; o_idx++) {
                if (hipThreadIdx_x < 2) {
                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, 0x3fff, 0x0);
                    W_CMD_R(&fim_ctr[addr + offset], hab_to_fim + hipThreadIdx_x * 16);
                }
                B_CMD(0);

                uint64_t i_offset = b_idx * num_in_tile * num_grf + gidx;
                int r_offset = o_idx * num_in_tile / 2;

                for (int i_idx = 0; i_idx < num_in_tile; i_idx += 2) {
                    uint64_t i_addr = (i_offset + i_idx * num_grf) * trans_size;
                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, 0x3fff, 0x8 + gidx);
                    W_CMD_R(&fim_ctr[addr + offset], &fim_input[i_addr + offset]);
                    B_CMD(0);

                    row = (i_idx / 2 + r_offset) * 2;
                    col = gidx;

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row, col);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row, col + 8);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row, col + 16);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row, col + 24);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row + 1, col);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row + 1, col + 8);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row + 1, col + 16);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, row + 1, col + 24);
                    R_CMD(&fim_weight[addr + offset]);
                    B_CMD(0);
                }

                for (int i_idx = 1; i_idx < num_in_tile; i_idx += 2) {
                    uint64_t i_addr = (i_offset + i_idx * num_grf) * trans_size;
                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, 0x3fff, 0x8 + gidx);
                    W_CMD_R(&fim_ctr[addr + offset], &fim_input[i_addr + offset]);
                    B_CMD(0);

                    row = (i_idx / 2 + r_offset) * 2;
                    col = gidx;

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row, col);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row, col + 8);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row, col + 16);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row, col + 24);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row + 1, col);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row + 1, col + 8);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row + 1, col + 16);
                    R_CMD(&fim_weight[addr + offset]);

                    addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row + 1, col + 24);
                    R_CMD(&fim_weight[addr + offset]);
                    B_CMD(0);
                }
                loc = b_idx * num_out_tile * num_grf + o_idx * num_grf + gidx;
                row = loc / num_col;
                col = loc % num_col;

                addr = addr_gen(hipBlockIdx_x, 0, 0, 1, row, col);
                W_CMD(&fim_gemv_tmp_buffer[addr + offset]);
                B_CMD(0);

                if (hipThreadIdx_x < 2) {
                    addr = addr_gen(hipBlockIdx_x, 0, 0, 0, 0x3fff, 0x0);
                    W_CMD_R(&fim_ctr[addr + offset], fim_to_hab + hipThreadIdx_x * 16);
                }
                B_CMD(0);
            }
        }
    }
#endif

#if CHANGE_HAB_SB
    if (hipThreadIdx_x < 4) {
        addr = addr_gen(hipBlockIdx_x, 0, 0, gidx, 0x2fff, 0x1f);
        W_CMD(&fim_ctr[addr + offset]);
    }
    B_CMD(0);
#endif

#if PARK_OUT
    if (hipThreadIdx_x < 4) {
        addr = addr_gen(hipBlockIdx_x, 0, 0, gidx, (1 << 12), 0);
        R_CMD(&fim_ctr[addr + offset]);
    }
    B_CMD(0);
#endif
}

int main(int argc, char* argv[])
{
    uint64_t fim_base;
    uint64_t *mode1_d, *mode2_d, *crf_bin_d;
    uint64_t *mode1_h, *mode2_h, *crf_bin_h;
    uint16_t* input;
    size_t N = 4;
    size_t Nbytes = N * sizeof(uint64_t);
    static int device = 0;

    int num_fim_blocks = 8;
    int num_fim_chan = CHANNEL;
    int num_grf = 8;
    int trans_size = 32;

    int in_size = 1024;
    int out_size = 4096;
    int row_offset = 0x100000;

    CHECK(hipSetDevice(device));
    hipDeviceProp_t props;
    CHECK(hipGetDeviceProperties(&props, device /*deviceID*/));

    // Get GPU ID
    FILE* fd;
    char path[256];
    uint32_t gpu_id;

    snprintf(path, 256, "/sys/devices/virtual/kfd/kfd/topology/nodes/1/gpu_id");
    fd = fopen(path, "r");
    if (!fd) return -1;
    if (fscanf(fd, "%ul", &gpu_id) != 1) return -1;
    fclose(fd);

    uint64_t ret = 0;
    /********************************************
      ARG1 : node-id
      ARG2 : gpu-id
      ARG3 : block size
    ********************************************/
    uint64_t bsize = FIM_RESERVED_8GB;
    fim_base = fmm_map_fim(1, gpu_id, bsize);
    std::cout << std::hex << "fimBaseAddr = " << fim_base << std::endl;

    crf_bin_h = (uint64_t*)malloc(Nbytes);
    CHECK(crf_bin_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    mode1_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode1_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    mode2_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode2_h == 0 ? hipErrorOutOfMemory : hipSuccess);

    crf_bin_h[0] = 0x0080103b023800e0;
    crf_bin_h[1] = 0x0080183b023800e0;
    crf_bin_h[2] = 0x07000000000000f0;
    crf_bin_h[3] = 0x0000000000000000;
    mode1_h[0] = 0x0000000000000001;
    mode1_h[1] = 0x0000000000000000;
    mode1_h[2] = 0x0000000000000000;
    mode1_h[3] = 0x0000000000000000;
    mode2_h[0] = 0x0000000000000000;
    mode2_h[1] = 0x0000000000000000;
    mode2_h[2] = 0x0000000000000000;
    mode2_h[3] = 0x0000000000000000;

    CHECK(hipMalloc(&crf_bin_d, Nbytes));
    CHECK(hipMalloc(&mode1_d, Nbytes));
    CHECK(hipMalloc(&mode2_d, Nbytes));

    CHECK(hipMemcpy(crf_bin_d, crf_bin_h, Nbytes, hipMemcpyHostToDevice));
    CHECK(hipMemcpy(mode1_d, mode1_h, Nbytes, hipMemcpyHostToDevice));
    CHECK(hipMemcpy(mode2_d, mode2_h, Nbytes, hipMemcpyHostToDevice));

    CHECK(hipMalloc(&input, in_size * sizeof(uint16_t)));

    int n_in_tile = in_size * sizeof(uint16_t) / trans_size / num_grf;
    int n_out_tile = out_size / (num_fim_chan * num_fim_blocks * num_grf);

    const unsigned blocks = 64;
    const unsigned threadsPerBlock = 64;

    hipLaunchKernelGGL(gemv_fim, dim3(blocks), dim3(threadsPerBlock), 0, 0, (uint8_t*)fim_base,
                       (uint8_t*)fim_base + 0x100000, (uint8_t*)fim_base + 0x200000, (uint8_t*)input, 1, n_in_tile,
                       n_out_tile, (uint8_t*)crf_bin_d, 1, (uint8_t*)mode1_d, (uint8_t*)mode2_d);

    free(mode1_h);
    free(mode2_h);
    free(crf_bin_h);

    hipFree(mode1_d);
    hipFree(mode2_d);
    hipFree(crf_bin_d);
    hipFree(input);
}