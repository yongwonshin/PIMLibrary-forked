/*
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include "emulator/PimEmulator.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include "half.hpp"
#include "hip/hip_runtime.h"
#include "utility/pim_dump.hpp"
#include "utility/pim_log.h"
#include "utility/pim_util.h"

namespace pim
{
namespace runtime
{
namespace emulator
{
PimEmulator::PimEmulator(void)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called ";
    get_pim_block_info(&fbi_);
    rocm_path = std::getenv("ROCM_PATH");
}

PimEmulator* PimEmulator::get_instance(void)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    static PimEmulator* instance_ = new PimEmulator();

    return instance_;
}

int PimEmulator::initialize(void)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " Intialization done ";
    int ret = 0;

    return ret;
}

int PimEmulator::deinitialize(void)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;

    return ret;
}

int PimEmulator::convert_mem_trace_from_16B_to_32B(PimMemTraceData* fmtd32, int* fmtd32_size, PimMemTraceData* fmtd16,
                                                   int fmtd16_size, PimOpType op_type)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;

    TraceParser trace_converter;
    trace_converter.coalesce_trace(fmtd32, fmtd32_size, fmtd16, fmtd16_size);

    DLOG(INFO) << "fmtd16_size : " << fmtd16_size;

#ifdef DEBUG_PIM
    const char* op_str = get_pim_op_string(op_type);
    const char* test_vector_path = TEST_VECTORS_DATA;
    std::string dump_data = TEST_VECTORS_DATA;
    dump_data.append("dump/");
    dump_data.append(op_str);
    std::string dump_fmtd16 = dump_data + "/fmtd16.dat";
    std::string dump_fmtd32 = dump_data + "/fmtd32.dat";
    dump_fmtd<16>(dump_fmtd16.c_str(), fmtd16, fmtd16_size);
    dump_fmtd<32>(dump_fmtd32.c_str(), fmtd32, fmtd32_size[0]);
#endif

    return ret;
}

int PimEmulator::execute_gemv_tile_tree(PimBo* output, PimBo* pim_data, PimMemTraceData* fmtd32, int fmtd32_size,
                                        PimOpType op_type, uint64_t pim_base_addr, uint8_t* temp_buf)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;
    uint16_t* sim_output = nullptr;

    int out_dim = pim_data->bshape.h;
    int batch_dim = output->bshape.n;
    int num_input_tile = pim_data->bshape.w / 128;
    sim_output = new uint16_t[out_dim * batch_dim];
    std::string rocm_path = ROCM_PATH;

    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t tmp_data_addr = reinterpret_cast<uint64_t>(temp_buf);
    uint64_t pim_data_addr = reinterpret_cast<uint64_t>(pim_data->data);

    pim_sim_.preload_data_with_addr(pim_data_addr - pim_base_addr, pim_data->data, pim_data->size);
    pim_sim_.execute_kernel((void*)fmtd32, fmtd32_size);
    pim_sim_.read_result_gemv_tree(sim_output, tmp_data_addr - pim_base_addr, out_dim, batch_dim, num_input_tile);

    if (output->mem_type != MEM_TYPE_HOST) {
        for (int i = 0; i < output->bshape.n; i++) {
            hipMemcpy((half*)output->data + i * pim_data->bshape_r.h, (half*)sim_output + i * pim_data->bshape.h,
                      pim_data->bshape_r.h * sizeof(half), hipMemcpyHostToDevice);
        }
    }

    delete sim_output;

    return ret;
}

int PimEmulator::execute_gemv_tile_accum(PimBo* output, PimBo* pim_data, PimMemTraceData* fmtd32, int fmtd32_size,
                                         PimOpType op_type, uint64_t pim_base_addr, uint8_t* temp_buf)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;
    uint16_t* sim_output = nullptr;

    int out_dim = pim_data->bshape.h * output->bshape.n;
    sim_output = new uint16_t[out_dim];
    std::string rocm_path = ROCM_PATH;

    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t tmp_data_addr = reinterpret_cast<uint64_t>(temp_buf);
    uint64_t pim_data_addr = reinterpret_cast<uint64_t>(pim_data->data);

    pim_sim_.preload_data_with_addr(pim_data_addr - pim_base_addr, pim_data->data, pim_data->size);
    pim_sim_.execute_kernel((void*)fmtd32, fmtd32_size);
    pim_sim_.read_result_gemv(sim_output, tmp_data_addr - pim_base_addr, out_dim);

    if (output->mem_type != MEM_TYPE_HOST) {
        for (int i = 0; i < output->bshape.n; i++) {
            hipMemcpy((half*)output->data + i * pim_data->bshape_r.h, (half*)sim_output + i * pim_data->bshape.h,
                      pim_data->bshape_r.h * sizeof(half), hipMemcpyHostToDevice);
        }
    }

    delete sim_output;

    return ret;
}

int PimEmulator::execute_gemv_add_tile_accum(PimBo* output, PimBo* pim_data, PimMemTraceData* fmtd32, int fmtd32_size,
                                             PimOpType op_type, uint64_t pim_base_addr, uint8_t* temp_buf)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;
    uint16_t* sim_output = nullptr;

    int num_batch = output->bshape.n;
    int out_num = pim_data->bshape.h;
    int out_dim = out_num * num_batch;
    sim_output = new uint16_t[out_dim];
    std::string rocm_path = ROCM_PATH;
    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t tmp_data_addr = reinterpret_cast<uint64_t>(temp_buf);
    uint64_t pim_data_addr = reinterpret_cast<uint64_t>(pim_data->data);
    uint64_t output_addr = reinterpret_cast<uint64_t>(output->data);

    pim_sim_.preload_data_with_addr(pim_data_addr - pim_base_addr, pim_data->data, pim_data->size);
    pim_sim_.execute_kernel((void*)fmtd32, fmtd32_size);
    pim_sim_.read_result_gemv(sim_output, tmp_data_addr - pim_base_addr, out_dim);

    int out_num_r = pim_data->bshape_r.h;
    int out_size_r = out_num_r * num_batch * sizeof(uint16_t);
    void* output_host = malloc(out_size_r);
    hipMemcpy(output_host, output->data, out_size_r, hipMemcpyDeviceToHost);
    for (int b = 0; b < num_batch; b++) {
        for (int i = 0; i < out_num_r; i++) {
            (reinterpret_cast<half_float::half*>(output_host))[b * out_num_r + i] +=
                (reinterpret_cast<half_float::half*>(sim_output))[b * out_num + i];
        }
    }
    hipMemcpy(output->data, output_host, out_size_r, hipMemcpyHostToDevice);

    free(output_host);
    delete sim_output;

    return ret;
}

int PimEmulator::execute_bn(PimBo* output, PimBo* pim_data, PimMemTraceData* fmtd32, int fmtd32_size,
                            uint64_t pim_base_addr, uint8_t* temp_buf)
{
    DLOG(INFO) << "[START] " << __FUNCTION__ << " called";
    int ret = 0;
    int num_element = 0;
    uint16_t* sim_output = nullptr;

    num_element = output->size / sizeof(uint16_t);
    sim_output = new uint16_t[num_element];
    std::string rocm_path = ROCM_PATH;
    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t tmp_data_addr = reinterpret_cast<uint64_t>(temp_buf);
    uint64_t pim_data_addr = reinterpret_cast<uint64_t>(pim_data->data);
    uint64_t output_addr = reinterpret_cast<uint64_t>(output->data);

    pim_sim_.preload_data_with_addr(pim_data_addr - pim_base_addr, pim_data->data, pim_data->size);
    pim_sim_.execute_kernel((void*)fmtd32, fmtd32_size);
    pim_sim_.read_result(sim_output, output_addr - pim_base_addr, output->size);

    if (output->mem_type != MEM_TYPE_HOST)
        hipMemcpy((void*)output->data, (void*)sim_output, output->size, hipMemcpyHostToDevice);

    delete sim_output;

    return ret;
}

int PimEmulator::execute_elt_op(PimBo* output, PimBo* operand0, PimBo* operand1, PimMemTraceData* fmtd32,
                                int fmtd32_size, uint64_t pim_base_addr)
{
    DLOG(INFO) << "called";
    int ret = 0;
    int num_element = 0;
    uint16_t* sim_output = nullptr;

    num_element = output->size / sizeof(uint16_t);
    sim_output = new uint16_t[num_element];
    std::string rocm_path = ROCM_PATH;
    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t input0_addr = reinterpret_cast<uint64_t>(operand0->data);
    uint64_t input1_addr = reinterpret_cast<uint64_t>(operand1->data);
    uint64_t output_addr = reinterpret_cast<uint64_t>(output->data);

    pim_sim_.preload_data_with_addr(input0_addr - pim_base_addr, operand0->data, operand0->size);
    pim_sim_.preload_data_with_addr(input1_addr - pim_base_addr, operand1->data, operand1->size);
    pim_sim_.execute_kernel((void*)fmtd32, (size_t)fmtd32_size);
    pim_sim_.read_result(sim_output, output_addr - pim_base_addr, output->size);
    if (output->mem_type != MEM_TYPE_HOST)
        hipMemcpy((void*)output->data, (void*)sim_output, output->size, hipMemcpyHostToDevice);

    delete sim_output;

    return ret;
}

int PimEmulator::execute_relu(PimBo* output, PimBo* pim_data, PimMemTraceData* fmtd32, int fmtd32_size,
                              uint64_t pim_base_addr)
{
    DLOG(INFO) << "called";
    int ret = 0;
    int num_element = 0;
    uint16_t* sim_output = nullptr;

    num_element = output->size / sizeof(uint16_t);
    sim_output = new uint16_t[num_element];
    std::string rocm_path = ROCM_PATH;
    pim_sim_.initialize(rocm_path + "/include/dramsim2/ini/HBM2_samsung_2M_16B_x64.ini",
                        rocm_path + "/include/dramsim2/ini/system_hbm_vega20.ini", 256 * 64 * 2, 64, 1);
    uint64_t pim_data_addr = reinterpret_cast<uint64_t>(pim_data->data);
    uint64_t output_addr = reinterpret_cast<uint64_t>(output->data);

    pim_sim_.preload_data_with_addr(pim_data_addr - pim_base_addr, pim_data->data, pim_data->size);
    pim_sim_.execute_kernel((void*)fmtd32, (size_t)fmtd32_size);
    pim_sim_.read_result(sim_output, output_addr - pim_base_addr, output->size);

    if (output->mem_type != MEM_TYPE_HOST)
        hipMemcpy((void*)output->data, (void*)sim_output, output->size, hipMemcpyHostToDevice);

    delete sim_output;

    return ret;
}

} /* namespace emulator */
} /* namespace runtime */
} /* namespace pim */