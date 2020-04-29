#include <assert.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include "fim_runtime_api.h"
#include "half.hpp"
#include "utility/fim_dump.hpp"

using half_float::half;

#define IN_LENGTH (256)
#define OUT_LENGTH (4096)

using namespace std;

int fim_gemv(void)
{
    int ret = 0;

    /* __FIM_API__ call : Initialize FimRuntime */
    FimInitialize(RT_TYPE_HIP, FIM_FP16);

    /* __FIM_API__ call : Create FIM Buffer Object */
    FimBo* host_input = FimCreateBo(IN_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_weight = FimCreateBo(IN_LENGTH, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_reordered_weight = FimCreateBo(IN_LENGTH, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_FIM);
    FimBo* device_input = FimCreateBo(IN_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_DEVICE);
    FimBo* device_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_DEVICE);
    FimBo* preloaded_weight = FimCreateBo(IN_LENGTH, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_FIM);
    FimBo* host_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* golden_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);

    /* Initialize the input, weight, output data */
    load_data("../test_vectors/load/gemv/input_256x1.dat", (char*)host_input->data, host_input->size);
    load_data("../test_vectors/load/gemv/weight_256x4096.dat", (char*)host_weight->data, host_weight->size);
    load_data("../test_vectors/load/gemv/output_4096x1.dat", (char*)golden_output->data, golden_output->size);
    FimCopyMemory(device_input, host_input, HOST_TO_DEVICE);

    /* __FIM_API__ call : Preload weight data on FIM memory */
    FimConvertDataLayout(host_reordered_weight, host_weight, OP_GEMV);
    FimCopyMemory(preloaded_weight, host_reordered_weight, HOST_TO_DEVICE);

    /* __FIM_API__ call : Execute FIM kernel (GEMV) */
    FimExecuteGEMV(device_output, device_input, preloaded_weight);

    FimCopyMemory(host_output, device_output, DEVICE_TO_HOST);
    dump_data("../test_vectors/dump/gemv/preloaded_weight_256x4096.dat", (char*)preloaded_weight->data,
              preloaded_weight->size);
    dump_data("../test_vectors/dump/gemv/output_4096x1.dat", (char*)host_output->data, host_output->size);

    ret = compare_data((char*)golden_output->data, (char*)host_output->data, host_output->size);

    /* __FIM_API__ call : Destroy FIM Buffer Object */
    FimDestroyBo(host_input);
    FimDestroyBo(host_weight);
    FimDestroyBo(host_output);
    FimDestroyBo(device_input);
    FimDestroyBo(device_output);
    FimDestroyBo(preloaded_weight);
    FimDestroyBo(host_reordered_weight);
    FimDestroyBo(golden_output);

    /* __FIM_API__ call : Deinitialize FimRuntime */
    FimDeinitialize();

    return ret;
}

int fim_gemv2(void)
{
    int ret = 0;

    /* __FIM_API__ call : Initialize FimRuntime */
    FimInitialize(RT_TYPE_HIP, FIM_FP16);

    /* __FIM_API__ call : Create FIM Buffer Object */
    FimBo* host_input = FimCreateBo(IN_LENGTH * 2, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_weight = FimCreateBo(IN_LENGTH * 2, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_reordered_weight = FimCreateBo(IN_LENGTH * 2, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_FIM);
    FimBo* device_input = FimCreateBo(IN_LENGTH * 2, 1, 1, 1, FIM_FP16, MEM_TYPE_DEVICE);
    FimBo* device_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_DEVICE);
    FimBo* preloaded_weight = FimCreateBo(IN_LENGTH * 2, OUT_LENGTH, 1, 1, FIM_FP16, MEM_TYPE_FIM);
    FimBo* host_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* golden_output = FimCreateBo(OUT_LENGTH, 1, 1, 1, FIM_FP16, MEM_TYPE_HOST);

    /* Initialize the input, weight, output data */
    load_data("../test_vectors/load/gemv/input_512x1.dat", (char*)host_input->data, host_input->size);
    load_data("../test_vectors/load/gemv/weight_512x4096.dat", (char*)host_weight->data, host_weight->size);
    load_data("../test_vectors/load/gemv/output_4096x1_512.dat", (char*)golden_output->data, golden_output->size);
    FimCopyMemory(device_input, host_input, HOST_TO_DEVICE);

    /* __FIM_API__ call : Preload weight data on FIM memory */
    FimConvertDataLayout(host_reordered_weight, host_weight, OP_GEMV);
    FimCopyMemory(preloaded_weight, host_reordered_weight, HOST_TO_DEVICE);

    /* __FIM_API__ call : Execute FIM kernel (GEMV) */
    FimExecuteGEMV(device_output, device_input, preloaded_weight);

    FimCopyMemory(host_output, device_output, DEVICE_TO_HOST);
    dump_data("../test_vectors/dump/gemv/preloaded_weight_512x4096.dat", (char*)preloaded_weight->data,
              preloaded_weight->size);
    dump_data("../test_vectors/dump/gemv/output_4096x1_512.dat", (char*)host_output->data, host_output->size);

    ret = compare_data((char*)golden_output->data, (char*)host_output->data, host_output->size);

    /* __FIM_API__ call : Destroy FIM Buffer Object */
    FimDestroyBo(host_input);
    FimDestroyBo(host_weight);
    FimDestroyBo(host_output);
    FimDestroyBo(device_input);
    FimDestroyBo(device_output);
    FimDestroyBo(preloaded_weight);
    FimDestroyBo(host_reordered_weight);
    FimDestroyBo(golden_output);

    /* __FIM_API__ call : Deinitialize FimRuntime */
    FimDeinitialize();

    return ret;
}

int fim_gemv3(void)
{
    int ret = 0;
    int in_size = 800;
    int out_size = 3200;

    /* __FIM_API__ call : Initialize FimRuntime */
    FimInitialize(RT_TYPE_HIP, FIM_FP16);

    FimDesc* fim_desc = FimCreateDesc(1, 1, out_size, in_size, FIM_FP16);
    /* __FIM_API__ call : Create FIM Buffer Object */
    FimBo* host_input = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_INPUT);
    FimBo* host_weight = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_WEIGHT);
    FimBo* temp_weight = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_WEIGHT);
    FimBo* host_reordered_weight = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_WEIGHT);
    FimBo* device_input = FimCreateBo(fim_desc, MEM_TYPE_DEVICE, GEMV_INPUT);
    FimBo* device_output = FimCreateBo(fim_desc, MEM_TYPE_DEVICE, GEMV_OUTPUT);
    FimBo* preloaded_weight = FimCreateBo(fim_desc, MEM_TYPE_FIM, GEMV_WEIGHT);
    FimBo* host_output = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_OUTPUT);
    FimBo* golden_output = FimCreateBo(fim_desc, MEM_TYPE_HOST, GEMV_OUTPUT);

    /* Initialize the input, weight, output data */
    load_data("../test_vectors/load/gemv/gemv_input_1024x4096.dat", (char*)host_input->data, in_size * sizeof(half));
    load_data("../test_vectors/load/gemv/gemv_output_1024x4096.dat", (char*)golden_output->data,
              out_size * sizeof(half));
    load_data("../test_vectors/load/gemv/gemv_weight_1024x4096.dat", (char*)temp_weight->data, temp_weight->size);

    for (int i = 0; i < fim_desc->bshape_r.h; i++) {
        memcpy((half*)host_weight->data + i * fim_desc->bshape_r.w, (half*)temp_weight->data + i * fim_desc->bshape.w,
               fim_desc->bshape_r.w * sizeof(half));
    }

    FimCopyMemory(device_input, host_input, HOST_TO_DEVICE);

    /* __FIM_API__ call : Preload weight data on FIM memory */
    FimConvertDataLayout(host_reordered_weight, host_weight, OP_GEMV);
    FimCopyMemory(preloaded_weight, host_reordered_weight, HOST_TO_DEVICE);

    /* __FIM_API__ call : Execute FIM kernel (GEMV) */
    FimExecuteGEMV(device_output, device_input, preloaded_weight);

    FimCopyMemory(host_output, device_output, DEVICE_TO_HOST);
    dump_data("../test_vectors/dump/gemv/gemv_preloaded_weight_1024x4096.dat", (char*)preloaded_weight->data,
              preloaded_weight->size);
    dump_data("../test_vectors/dump/gemv/gemv_output_1024x4096.dat", (char*)host_output->data, host_output->size);

    ret = compare_data((char*)golden_output->data, (char*)host_output->data, host_output->size);

    /* __FIM_API__ call : Destroy FIM Buffer Object */
    FimDestroyBo(host_input);
    FimDestroyBo(host_weight);
    FimDestroyBo(temp_weight);
    FimDestroyBo(host_output);
    FimDestroyBo(device_input);
    FimDestroyBo(device_output);
    FimDestroyBo(preloaded_weight);
    FimDestroyBo(host_reordered_weight);
    FimDestroyDesc(fim_desc);

    /* __FIM_API__ call : Deinitialize FimRuntime */
    FimDeinitialize();

    return ret;
}

TEST(HIPIntegrationTest, FimGEMV) { EXPECT_TRUE(fim_gemv() == 0); }
TEST(HIPIntegrationTest, FimGEMV2) { EXPECT_TRUE(fim_gemv2() == 0); }
TEST(HIPIntegrationTest, FimGEMV3) { EXPECT_TRUE(fim_gemv3() == 0); }
