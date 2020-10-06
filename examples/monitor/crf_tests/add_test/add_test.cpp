#include <stdio.h>
#include <iostream>
#include "hip/hip_runtime.h"
#include "hip/hip_fp16.h"
#include "half.hpp"

#define TARGET_MASK (0x1FFFFFFFF)
//#define TARGET_MASK (0xFFFFFFFFFFFFFFFF)
#define CHANNEL 8

extern "C" uint64_t fmm_map_fim(uint32_t, uint32_t, uint64_t);

#define CHECK(cmd)                                                                                              \
    {                                                                                                           \
        hipError_t error = cmd;                                                                                 \
        if (error != hipSuccess) {                                                                              \
            fprintf(stderr, "error: '%s'(%d) at %s:%d\n", hipGetErrorString(error), error, __FILE__, __LINE__); \
            exit(EXIT_FAILURE);                                                                                 \
        }                                                                                                       \
    }

__host__ void PrintHalf(uint64_t* data)
{
    for (int i = 0; i < 16; i++) {
        half_float::half x = ((half_float::half*)data)[i];
        printf("%f ", float(x));
    }
    printf("\n");
}

__device__ inline void R_CMD(volatile uint8_t* addr)
{
    asm volatile("global_load_dwordx4 v[24:27], %0, off, glc, slc\n\t" ::"v"(addr) : "v24", "v25", "v26", "v27");
}

__device__ inline void W_CMD(volatile uint8_t* addr)
{
    asm volatile("global_store_dwordx4 %0, v[24:27], off, glc, slc\n\t" ::"v"(addr) : "v24", "v25", "v26", "v27");
}

__device__ inline void W_CMD_R(volatile uint8_t* addr, volatile uint8_t* src) { ((int4*)addr)[0] = ((int4*)src)[0]; }

/*
__device__ inline void W_CMD_R(uint8_t* addr, uint8_t* src)
{
    if (hipThreadIdx_x == 0) {
        asm volatile("global_load_dwordx4 v[20:23], %0, off, glc, slc\n\t" ::"v"(src) : "v20", "v21", "v22", "v23");
        asm volatile("s_waitcnt vmcnt(0) lgkmcnt(0)");
        asm volatile("global_store_dwordx4 %0, v[20:23], off, glc, slc\n\t" ::"v"(addr) : "v20", "v21", "v22", "v23");
    } else {
        asm volatile("global_load_dwordx4 v[24:27], %0, off, glc, slc\n\t" ::"v"(src) : "v24", "v25", "v26", "v27");
        asm volatile("s_waitcnt vmcnt(0) lgkmcnt(0)");
        asm volatile("global_store_dwordx4 %0, v[24:27], off, glc, slc\n\t" ::"v"(addr) : "v24", "v25", "v26", "v27");
    }
}
*/

__device__ inline void B_CMD(int type)
{
    if (type == 0) {
        __syncthreads();
//        asm volatile("s_waitcnt vmcnt(0) lgkmcnt(0)");
    } else {
        __threadfence();
//        asm volatile("s_waitcnt vmcnt(0) lgkmcnt(0)");
    }
}

__host__ __device__ inline unsigned int mask_by_bit(unsigned int value, int start, int end)
{
    int length = start - end + 1;
    value = value >> end;
    return value & ((1 << length) - 1);
}

__host__ __device__ uint64_t addr_gen(unsigned int ch, unsigned int rank, unsigned int bg, unsigned int ba, unsigned int row,
                             unsigned int col)
{
    int num_row_bit_ = 14;
    int num_col_high_bit_ = 3;
    int num_bank_high_bit_ = 1;
    int num_bankgroup_bit_ = 2;
    int num_bank_low_bit_ = 1;
    int num_chan_bit_ = 6;
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

#if 1
    uint64_t mask = 0x1FFFFFFFF;
    addr &= mask;
#endif

    return addr;
}

__global__ void add_test(volatile uint8_t* fim_ctr, volatile uint8_t* fim_data, volatile uint8_t* fim_data2, volatile uint8_t* output,
                         volatile uint8_t* crf_binary, volatile uint8_t* hab_to_fim, volatile uint8_t* fim_to_hab, volatile uint8_t* test_input1)
{
    uint64_t offset = hipThreadIdx_x * 0x10;
    uint64_t addr;
    int ch = CHANNEL;

    for (int i = 0; i < (0x40000 >> 1); i++) {
        ((half*)fim_data)[0x0 + i] = 1;
        ((half*)fim_data2)[0x0 + i] = 2;
    }
    B_CMD(0);

#if 0
    /* write to odd banks in reverse order */
    W_CMD(&fim_ctr[(0x31e000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x30e000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x31a000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x30a000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x316000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x306000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x312000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x302000 & TARGET_MASK) + offset]);
    B_CMD(1);
    /* write to even banks in reverse order */
    W_CMD(&fim_ctr[(0x31c000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x30c000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x318000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x308000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x314000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x304000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x310000 & TARGET_MASK) + offset]);
    B_CMD(1);
    W_CMD(&fim_ctr[(0x300000 & TARGET_MASK) + offset]);
    B_CMD(1);
#endif

    /* park in */
    addr = addr_gen(ch, 0, 0, 0, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 0, 2, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 1, 0, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 1, 2, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 2, 0, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 2, 2, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 3, 0, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    addr = addr_gen(ch, 0, 3, 2, 0, 0);
    W_CMD_R(&fim_data[addr + offset], test_input1 + hipThreadIdx_x * 16);
    B_CMD(1);

    /* change SB mode to HAB mode */
    addr = addr_gen(ch, 0, 2, 0, 0x27ff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);
    addr = addr_gen(ch, 0, 2, 1, 0x27ff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);
    addr = addr_gen(ch, 0, 0, 0, 0x27ff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);
    addr = addr_gen(ch, 0, 0, 1, 0x27ff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);

    /* set crf binary */
    addr = addr_gen(ch, 0, 0, 1, 0x3fff, 0x4);
    W_CMD_R(&fim_ctr[addr + offset], crf_binary + hipThreadIdx_x * 16);
    B_CMD(1);

    /* change HAB mode to HAB_FIM mode */
    addr = addr_gen(ch, 0, 0, 0, 0x3fff, 0x0);
    W_CMD_R(&fim_ctr[addr + offset], hab_to_fim + hipThreadIdx_x * 16);
    B_CMD(1);

    /* add */
    addr = addr_gen(ch, 0, 0, 0, 0, 0);
    R_CMD(&fim_data[addr+ offset]); // MOV even_bank to grf_A
    B_CMD(1);
    addr = addr_gen(ch, 0, 0, 0, 0, 0);
    R_CMD(&fim_data2[0x0 + offset]); // ADD grf_A, even_bank
    B_CMD(1);
    addr = addr_gen(ch, 0, 0, 0, 0, 0);
    W_CMD(&output[0x0 + offset]); // NOP
//    W_CMD(&output[0x0 + offset]);
//    W_CMD(&output[0x0 + offset]);
//    W_CMD(&output[0x0 + offset]);
//    W_CMD(&output[0x0 + offset]);
    B_CMD(1);

    /* change HAB_FIM mode to HAB mode */
    addr = addr_gen(ch, 0, 0, 0, 0x3fff, 0x0);
    W_CMD_R(&fim_ctr[addr + offset], fim_to_hab + hipThreadIdx_x * 16);
    B_CMD(1);

    /* change HAB mode to SB mode */
    addr = addr_gen(ch, 0, 0, 0, 0x2fff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 0, 1, 0x2fff, 0x1f);
    W_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);

    /* park out */
    addr = addr_gen(ch, 0, 0, 0, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 0, 1, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 0, 2, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 0, 3, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 1, 0, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 1, 1, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 1, 2, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 1, 3, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 2, 0, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 2, 1, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 2, 2, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 2, 3, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 3, 0, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 3, 1, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 3, 2, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    addr = addr_gen(ch, 0, 3, 3, 0, 0);
    R_CMD(&fim_ctr[addr + offset]);
    B_CMD(1);
}

int main(int argc, char* argv[])
{
    uint64_t fim_base;
    uint64_t *mode1_d, *mode2_d, *crf_bin_d, *test1_d;
    uint64_t *mode1_h, *mode2_h, *crf_bin_h, *test1_h;
    uint64_t *output_h;
    size_t N = 4;
    size_t Nbytes = N * sizeof(uint64_t);
    static int device = 0;

    CHECK(hipSetDevice(device));
    hipDeviceProp_t props;
    CHECK(hipGetDeviceProperties(&props, device /*deviceID*/));
    printf ("info: running on device %s global mem size: %zu\n", props.name, props.totalGlobalMem);

    // Get GPU ID
    FILE* fd;
    char path[256];
    uint32_t gpu_id;

    snprintf(path, 256, "/sys/devices/virtual/kfd/kfd/topology/nodes/2/gpu_id");
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
    uint64_t bsize = 8589934592; //8 * 1024 * 1024 * 1024;
    // uint64_t bsize = 17179869184;  // 16 * 1024 * 1024 * 1024;
    fim_base = fmm_map_fim(2, gpu_id, bsize);
    std::cout << std::hex << "fimBaseAddr = " << fim_base << std::endl;

    crf_bin_h = (uint64_t*)malloc(Nbytes);
    CHECK(crf_bin_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    mode1_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode1_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    mode2_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode2_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    test1_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode2_h == 0 ? hipErrorOutOfMemory : hipSuccess);
    output_h = (uint64_t*)malloc(Nbytes);
    CHECK(mode2_h == 0 ? hipErrorOutOfMemory : hipSuccess);

    crf_bin_h[0] = 0x1910000088800000;
    crf_bin_h[1] = 0xf000000000000000;
//    crf_bin_h[1] = 0xf000000000000004;
    crf_bin_h[2] = 0x0000000000000000;
    crf_bin_h[3] = 0x0000000000000000;
    mode1_h[0] = 0x0000000000000001;
    mode1_h[1] = 0x0000010000000000;
    mode1_h[2] = 0x0000000000000000;
    mode1_h[3] = 0x0000000000000000;
    mode2_h[0] = 0x0000000000000000;
    mode2_h[1] = 0x0000000000000000;
    mode2_h[2] = 0x0000000000000000;
    mode2_h[3] = 0x0000000000000000;
    test1_h[0] = 0x0000000000000002;
    test1_h[1] = 0x0000000000000000;
    test1_h[2] = 0x0000000000000000;
    test1_h[3] = 0x0000000000000000;

    CHECK(hipMalloc(&crf_bin_d, Nbytes));
    CHECK(hipMalloc(&mode1_d, Nbytes));
    CHECK(hipMalloc(&mode2_d, Nbytes));
    CHECK(hipMalloc(&test1_d, Nbytes));

    CHECK(hipMemcpy(crf_bin_d, crf_bin_h, Nbytes, hipMemcpyHostToDevice));
    CHECK(hipMemcpy(mode1_d, mode1_h, Nbytes, hipMemcpyHostToDevice));
    CHECK(hipMemcpy(mode2_d, mode2_h, Nbytes, hipMemcpyHostToDevice));
    CHECK(hipMemcpy(test1_d, test1_h, Nbytes, hipMemcpyHostToDevice));

    const unsigned blocks = 1;
    const unsigned threadsPerBlock = 2;

    hipLaunchKernelGGL(add_test, dim3(blocks), dim3(threadsPerBlock), 0, 0, (uint8_t*)fim_base, (uint8_t*)fim_base,
                       (uint8_t*)fim_base + 0x100000, (uint8_t*)fim_base + 0x200000, (uint8_t*)crf_bin_d,
                       (uint8_t*)mode1_d, (uint8_t*)mode2_d, (uint8_t*)test1_d);

    hipDeviceSynchronize();

    uint64_t addr_offset;
    addr_offset = addr_gen(CHANNEL, 0, 0, 0, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 0, 2, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 1, 0, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 1, 2, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 2, 0, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 2, 2, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 3, 0, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);
    addr_offset = addr_gen(CHANNEL, 0, 3, 2, 0, 0);
    CHECK(hipMemcpy(output_h, (uint8_t*)fim_base + 0x200000 + addr_offset, Nbytes, hipMemcpyDeviceToHost));
    PrintHalf(output_h);

    free(mode1_h);
    free(mode2_h);
    free(crf_bin_h);
    free(test1_h);
    free(output_h);

    hipFree(mode1_d);
    hipFree(mode2_d);
    hipFree(crf_bin_d);
    hipFree(test1_d);
}
