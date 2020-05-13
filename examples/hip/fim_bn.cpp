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

using namespace std;

int fim_bn_1(void)
{
    int ret = 0;

    const int BATCH = 2;
    const int CH = 64;
    const int WIDTH = 1024;
    const int HEIGHT = 1;
    const int PARAMS = 4;

    /* __FIM_API__ call : Initialize FimRuntime */
    FimInitialize(RT_TYPE_HIP, FIM_FP16);

    /* __FIM_API__ call : Create FIM Buffer Object */
    FimBo* host_input = FimCreateBo(WIDTH, HEIGHT, CH, BATCH, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_beta = FimCreateBo(1, 1, CH, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_gamma = FimCreateBo(1, 1, CH, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_scale = FimCreateBo(1, 1, CH, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_shift = FimCreateBo(1, 1, CH, 1, FIM_FP16, MEM_TYPE_HOST);
    FimBo* host_output = FimCreateBo(WIDTH, HEIGHT, CH, BATCH, FIM_FP16, MEM_TYPE_HOST);
    FimBo* golden_output = FimCreateBo(WIDTH, HEIGHT, CH, BATCH, FIM_FP16, MEM_TYPE_HOST);
    FimBo* device_output = FimCreateBo(WIDTH, HEIGHT, CH, BATCH, FIM_FP16, MEM_TYPE_DEVICE);
    FimBo* preloaded_fim_input = FimCreateBo(WIDTH, HEIGHT, CH, BATCH, FIM_FP16, MEM_TYPE_FIM);

    /* Initialize the input, output data */
    std::string test_vector_data = TEST_VECTORS_DATA;
    test_vector_data.append("/test_vectors/");

    std::string input = test_vector_data + "load/bn/input_256KB.dat";
    std::string beta = test_vector_data + "load/bn/beta_128B.dat";
    std::string gamma = test_vector_data + "load/bn/gamma_128B.dat";
    std::string scale = test_vector_data + "load/bn/beta_128B.dat";
    std::string shift = test_vector_data + "load/bn/shift_128B.dat";
    std::string output = test_vector_data + "load/bn/output_256KB.dat";
    std::string preload_input = test_vector_data + "dump/bn/preloaded_input_256KB.dat";
    std::string output_dump = test_vector_data + "dump/bn/output_256KB.dat";

    load_data(input.c_str(), (char*)host_input->data, host_input->size);
    load_data(beta.c_str(), (char*)host_beta->data, host_beta->size);
    load_data(gamma.c_str(), (char*)host_gamma->data, host_gamma->size);
    load_data(scale.c_str(), (char*)host_scale->data, host_scale->size);
    load_data(shift.c_str(), (char*)host_shift->data, host_shift->size);
    load_data(output.c_str(), (char*)golden_output->data, golden_output->size);

    /* __FIM_API__ call : Preload weight data on FIM memory */
    FimConvertDataLayout(preloaded_fim_input, host_input, OP_BN);

    /* __FIM_API__ call : Execute FIM kernel */
    FimExecuteBN(device_output, preloaded_fim_input, host_beta, host_gamma, host_scale, host_shift);

    FimCopyMemory(host_output, device_output, DEVICE_TO_HOST);

    ret = compare_data((char*)golden_output->data, (char*)host_output->data, host_output->size);

    dump_data(preload_input.c_str(), (char*)preloaded_fim_input->data, preloaded_fim_input->size);
    dump_data(output_dump.c_str(), (char*)host_output->data, host_output->size);

    /* __FIM_API__ call : Free memory */
    FimDestroyBo(host_input);
    FimDestroyBo(host_beta);
    FimDestroyBo(host_gamma);
    FimDestroyBo(host_shift);
    FimDestroyBo(host_scale);
    FimDestroyBo(host_output);
    FimDestroyBo(golden_output);
    FimDestroyBo(device_output);
    FimDestroyBo(preloaded_fim_input);

    /* __FIM_API__ call : Deinitialize FimRuntime */
    FimDeinitialize();

    return ret;
}
TEST(HIPIntegrationTest, FimBN1) { EXPECT_TRUE(fim_bn_1() == 0); }
