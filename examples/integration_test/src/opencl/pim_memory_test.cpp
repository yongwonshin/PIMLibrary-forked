/*
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <assert.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <random>
#include "utility/pim_debug.hpp"
#include "utility/pim_profile.h"
#include "half.hpp"
#include "pim_runtime_api.h"

#define IN_LENGTH 1024 * 256
#define BATCH_DIM 1
using half_float::half;
using namespace std;

template <class T>
void fill_uniform_random_values(void* data, uint32_t count, T start, T end)
{
    std::random_device rd;
    std::mt19937 mt(rd());

    std::uniform_real_distribution<double> dist(start, end);

    for (int i = 0; i < count; i++) ((T*)data)[i] = dist(mt);
}

bool simple_pim_alloc_free()
{
    PimBo pim_weight = {.mem_type = MEM_TYPE_PIM, .size = IN_LENGTH * sizeof(half)};
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);

    int ret = PimAllocMemory(&pim_weight);
    if (ret) return false;

    ret = PimFreeMemory(&pim_weight);
    if (ret) return false;

    PimDeinitialize();

    return true;
}

bool pim_repeat_allocate_free(void)
{
    PimBo pim_weight = {.mem_type = MEM_TYPE_PIM, .size = IN_LENGTH * sizeof(half)};

    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);

    int i = 0;
    while (i < 100) {
        int ret = PimAllocMemory(&pim_weight);
        if (ret) return false;
        ret = PimFreeMemory(&pim_weight);
        if (ret) return false;
        i++;
    }

    PimDeinitialize();

    return true;
}

bool test_memcpy_bw_host_device()
{
    int ret = 0;

    /* __PIM_API__ call : Initialize PimRuntime */
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);

    /* __PIM_API__ call : Create PIM Buffer Object */
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* device_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* host_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);

    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);

    PimCopyMemory(device_input, host_input, HOST_TO_DEVICE);
    PimCopyMemory(host_output, device_input, DEVICE_TO_HOST);

    ret = compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(device_input);
    PimFreeMemory(host_output);

    PimDeinitialize();

    return true;
}

bool test_memcpy_bw_device_device()
{
    int ret = 0;

    /* __PIM_API__ call : Initialize PimRuntime */
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);

    /* __PIM_API__ call : Create PIM Buffer Object */
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* device_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* device_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* host_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);

    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);

    PimCopyMemory(device_input, host_input, HOST_TO_DEVICE);
    PimCopyMemory(device_output, device_input, DEVICE_TO_DEVICE);
    PimCopyMemory(host_output, device_output, DEVICE_TO_HOST);

    ret = compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(device_input);
    PimFreeMemory(device_output);
    PimFreeMemory(host_output);

    PimDeinitialize();

    return true;
}

bool test_memcpy_bw_host_pim()
{
    int ret = 0;
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* pim_buffer = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_PIM);
    PimBo* host_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);

    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);
    PimCopyMemory(pim_buffer, host_input, HOST_TO_PIM);
    PimCopyMemory(host_output, pim_buffer, PIM_TO_HOST);
    ret = compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(pim_buffer);
    PimFreeMemory(host_output);

    PimDeinitialize();

    return true;
}

bool test_memcpy_bw_device_pim()
{
    int ret = 0;

    /* __PIM_API__ call : Initialize PimRuntime */
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);
    // PimInitialize(RT_TYPE_HIP, PIM_FP16);

    /* __PIM_API__ call : Create PIM Buffer Object */
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* device_buffer_src = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* device_buffer_dst = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* pim_buffer = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_PIM);
    PimBo* host_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);

    PimCopyMemory(device_buffer_src, host_input, HOST_TO_DEVICE);
    PimCopyMemory(pim_buffer, device_buffer_src, DEVICE_TO_PIM);
    PimCopyMemory(device_buffer_dst, pim_buffer, PIM_TO_DEVICE);
    PimCopyMemory(host_output, device_buffer_dst, DEVICE_TO_HOST);

    ret = compare_half_relative((half_float::half*)host_output->data, (half_float::half*)host_input->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(device_buffer_src);
    PimFreeMemory(device_buffer_dst);
    PimFreeMemory(pim_buffer);
    PimFreeMemory(host_output);

    PimDeinitialize();

    return true;
}

bool test_memcpy_bw_host_pim_device_host()
{
    int ret = 0;

    /* __PIM_API__ call : Initialize PimRuntime */
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);
    // PimInitialize(RT_TYPE_HIP, PIM_FP16);

    /* __PIM_API__ call : Create PIM Buffer Object */
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* device_buffer = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* pim_buffer = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_PIM);
    PimBo* host_output = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);

    PimCopyMemory(pim_buffer, host_input, HOST_TO_PIM);
    // PimCopyMemory(pim_buffer, device_buffer, DEVICE_TO_PIM);
    PimCopyMemory(device_buffer, pim_buffer, PIM_TO_DEVICE);
    PimCopyMemory(host_output, device_buffer, DEVICE_TO_HOST);

    ret = compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(device_buffer);
    PimFreeMemory(pim_buffer);
    PimFreeMemory(host_output);

    PimDeinitialize();

    return true;
}

bool test_memcpy_exp()
{
    int ret = 0;

    /* __PIM_API__ call : Initialize PimRuntime */
    PimInitialize(RT_TYPE_OPENCL, PIM_FP16);
    // PimInitialize(RT_TYPE_HIP, PIM_FP16);

    /* __PIM_API__ call : Create PIM Buffer Object */
    PimBo* host_input = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* device_buffer_independent = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* device_buffer_pim = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_DEVICE);
    PimBo* pim_buffer = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_PIM);
    PimBo* host_output_ind = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    PimBo* host_output_pim = PimCreateBo(BATCH_DIM, 1, 1, IN_LENGTH, PIM_FP16, MEM_TYPE_HOST);
    fill_uniform_random_values<half_float::half>(host_input->data, IN_LENGTH, (half_float::half)0.0,
                                                 (half_float::half)0.5);

    // directly copying from host to device and copying back to host
    PimCopyMemory(device_buffer_independent, host_input, HOST_TO_DEVICE);
    PimCopyMemory(host_output_ind, device_buffer_independent, DEVICE_TO_HOST);

    ret =
        compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output_ind->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "flow_1 (host-device-host) data is different" << std::endl;
    } else {
        std::cout << "flow_1 (host-device-host) has passed\n";
    }

    // flow: host-pim-device-host.
    PimCopyMemory(pim_buffer, host_input, HOST_TO_PIM);
    PimCopyMemory(device_buffer_independent, pim_buffer, PIM_TO_DEVICE);
    PimCopyMemory(host_output_pim, device_buffer_independent, DEVICE_TO_HOST);

    ret =
        compare_half_relative((half_float::half*)host_input->data, (half_float::half*)host_output_pim->data, IN_LENGTH);
    if (ret != 0) {
        std::cout << "flow_2 (host-pim-device-host) data is different" << std::endl;
    }

    ret = compare_half_relative((half_float::half*)host_output_ind->data, (half_float::half*)host_output_pim->data,
                                IN_LENGTH);
    if (ret != 0) {
        std::cout << "flow_1 and  flow_2 data is different" << std::endl;
        return false;
    }

    PimFreeMemory(host_input);
    PimFreeMemory(device_buffer_pim);
    PimFreeMemory(pim_buffer);
    PimFreeMemory(host_output_pim);
    PimFreeMemory(host_output_ind);
    PimFreeMemory(device_buffer_independent);

    PimDeinitialize();

    return true;
}

TEST(UnitTest, simplePimAllocFree) { EXPECT_TRUE(simple_pim_alloc_free()); }
TEST(UnitTest, PimRepeatAllocateFree) { EXPECT_TRUE(pim_repeat_allocate_free()); }
TEST(UnitTest, PimMemCopyHostAndDeviceTest) { EXPECT_TRUE(test_memcpy_bw_host_device()); }
TEST(UnitTest, PimMemCopyDeviceAndDeviceTest) { EXPECT_TRUE(test_memcpy_bw_device_device()); }
TEST(UnitTest, PimMemCopyHostAndPimTest) { EXPECT_TRUE(test_memcpy_bw_host_pim()); }
TEST(UnitTest, PimMemCopyDeviceAndPimTest) { EXPECT_TRUE(test_memcpy_bw_device_pim()); } /* currently failing */
/* experimental tests */
// TEST(UnitTest, PimMemCopyDeviceAndPimTest_1) { EXPECT_TRUE(test_memcpy_bw_host_pim_device_host()); }
// TEST(UnitTest, PimMemCopyExp) { EXPECT_TRUE(test_memcpy_exp()); }
