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

#include <assert.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include "half.hpp"
#include "pim_runtime_api.h"
#include "utility/pim_debug.hpp"
#include "utility/pim_profile.h"

using half_float::half;

int custom_gemv_beta_test_api(int in_h, int in_w, int out_h, int out_w, PimGemmOrder gemm_order, bool block)
{
    int ret = 0;
    int wei_size = 0;
    int in_size = 0;
    int out_size = 0;
    float alpha = 1.0f;
    float beta = 1.0f;
    float variation = 0.2f;
    float epsilon = 0.1f;

    PimInitialize(RT_TYPE_HIP, PIM_FP16);
    PimGemmDesc* gemm_desc = PimCreateGemmDesc(1, 1, in_h, in_w, out_h, out_w, PIM_FP16, gemm_order);
    if (gemm_order == I_X_W) {
        in_size = in_w;
        out_size = out_w;
    } else {
        in_size = in_h;
        out_size = out_h;
    }
    wei_size = in_size * out_size;
    PimBo* h_i = PimCreateBo(gemm_desc, MEM_TYPE_HOST, GEMM_INPUT);
    PimBo* h_w = PimCreateBo(gemm_desc, MEM_TYPE_HOST, GEMM_WEIGHT);
    PimBo* h_o = PimCreateBo(gemm_desc, MEM_TYPE_HOST, GEMM_OUTPUT);
    PimBo* golden = PimCreateBo(gemm_desc, MEM_TYPE_HOST, GEMM_OUTPUT);

    set_rand_half_data((half*)h_i->data, half(variation), in_size);
    set_rand_half_data((half*)h_w->data, half(variation), wei_size);
    set_half_data((half*)golden->data, half(0.0), out_size);
    PimCopyMemory(h_o, golden, HOST_TO_HOST);

    half* h_i_data = (half*)h_i->data;
    half* h_w_data = (half*)h_w->data;
    half* golden_data = (half*)golden->data;

    if (gemm_order == I_X_W) {
        matmulCPU(h_i_data, h_w_data, golden_data, in_h, out_w, in_w, half(alpha), half(beta));
    } else {
        matmulCPU(h_w_data, h_i_data, golden_data, out_h, out_w, in_h, half(alpha), half(beta));
    }

    PimBo* d_i = PimCreateBo(gemm_desc, MEM_TYPE_DEVICE, GEMM_INPUT);
    PimBo* d_w = PimCreateBo(gemm_desc, MEM_TYPE_DEVICE, GEMM_WEIGHT);
    PimBo* d_o = PimCreateBo(gemm_desc, MEM_TYPE_DEVICE, GEMM_OUTPUT);

    PimCopyMemory(d_i, h_i, HOST_TO_DEVICE);
    PimCopyMemory(d_w, h_w, HOST_TO_DEVICE);
    PimCopyMemory(d_o, h_o, HOST_TO_DEVICE);

    ret = PimExecuteGemm(d_o, d_i, d_w, d_o, PimActFunc::NONE, gemm_order, nullptr, block);
    if (ret != 0) {
        printf("PimExecuteGemm error\n");
        return ret;
    }
    PimCopyMemory(h_o, d_o, DEVICE_TO_HOST);

    ret = compare_half_relative((half*)h_o->data, (half*)golden->data, out_size, epsilon);

    PimDestroyBo(h_i);
    PimDestroyBo(h_w);
    PimDestroyBo(h_o);
    PimDestroyBo(golden);
    PimDestroyBo(d_i);
    PimDestroyBo(d_w);
    PimDestroyBo(d_o);
    PimDestroyGemmDesc(gemm_desc);
    PimDeinitialize();

    return ret;
}

TEST(HIPIntegrationTest, CustomAddmvxAyBetaAPITest)
{
    EXPECT_TRUE(custom_gemv_beta_test_api(1, 320, 1, 1280, I_X_W, true) == 0);
}

TEST(HIPIntegrationTest, CustomAddmvAxyBetaAPITest)
{
    EXPECT_TRUE(custom_gemv_beta_test_api(320, 1, 1280, 1, W_X_I, true) == 0);
}
