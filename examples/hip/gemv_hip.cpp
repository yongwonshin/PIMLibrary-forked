#include <gtest/gtest.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include "fim_runtime_api.h"
#include "hip/hip_fp16.h"
#include "utility/fim_util.h"

#define WIDTH (2048)
#define HEIGHT (1024)
#define INPUT (WIDTH)
#define OUTPUT (HEIGHT)

using namespace std;

int fim_gemv_hip(void)
{
    int ret = 0;

    FimBo hostInput = {.size = INPUT * sizeof(FP16), .memType = MEM_TYPE_HOST};
    FimBo hostWeight = {.size = WIDTH * HEIGHT * sizeof(FP16), .memType = MEM_TYPE_HOST};
    FimBo hostOutput = {.size = OUTPUT * sizeof(FP16), .memType = MEM_TYPE_HOST};
    FimBo deviceInput = {.size = INPUT * sizeof(FP16), .memType = MEM_TYPE_DEVICE};
    FimBo deviceOutput = {.size = OUTPUT * sizeof(FP16), .memType = MEM_TYPE_DEVICE};
    FimBo fimWeight = {.size = WIDTH * HEIGHT * sizeof(FP16), .memType = MEM_TYPE_FIM};

    /* __FIM_API__ call : Initialize FimRuntime */
    FimInitialize(RT_TYPE_HIP, FIM_FP16);

    /* __FIM_API__ call : Allocate host(CPU) memory */
    FimAllocMemory(&hostInput);
    FimAllocMemory(&hostWeight);
    FimAllocMemory(&hostOutput);
    /* __FIM_API__ call : Allocate device(GPU) memory */
    FimAllocMemory(&deviceInput);
    FimAllocMemory(&deviceOutput);
    /* __FIM_API__ call : Allocate device(FIM) memory */
    FimAllocMemory(&fimWeight);

    FimExecute(&deviceOutput, &deviceInput, &fimWeight, OP_DUMMY);

    /* Initialize the input, weight, output data */
    initTestVector(hostInput, hostWeight, hostOutput);

    /* __FIM_API__ call : Copy operand data from host to device */
    FimCopyMemory(&deviceInput, &hostInput, HOST_TO_DEVICE);

    /* __FIM_API__ call : Preload weight data on FIM memory */
    FimConvertDataLayout(&fimWeight, &hostWeight, OP_GEMV);

    /* __FIM_API__ call : Execute FIM kernel (GEMV) */
    FimExecute(&deviceOutput, &deviceInput, &fimWeight, OP_GEMV);
    /* Verify the results */
    verifyResult(hostInput, hostWeight, hostOutput, deviceOutput);

    /* __FIM_API__ call : Free device(GPU) memory */
    FimFreeMemory(&deviceInput);
    /* __FIM_API__ call : Free device(FIM) memory */
    FimFreeMemory(&fimWeight);
    /* __FIM_API__ call : Free host(CPU) memory */
    FimFreeMemory(&hostInput);
    FimFreeMemory(&hostWeight);

    /* __FIM_API__ call : Deinitialize FimRuntime */
    FimDeinitialize();

    return ret;
}

TEST(IntergrationTest, FimGEMV) { EXPECT_TRUE(fim_gemv_hip()); }
