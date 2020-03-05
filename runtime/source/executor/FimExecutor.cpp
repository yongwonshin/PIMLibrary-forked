#include "executor/FimExecutor.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include "executor/fim_hip_kernels/fim_op_kernels.fimk"
#include "hip/hip_runtime.h"
#include "utility/fim_log.h"
#include "utility/fim_util.h"

#define BLOCKS 32
#define WIDTH 400
#define MT_NUM (BLOCKS * WIDTH)

namespace fim
{
namespace runtime
{
namespace executor
{
FimExecutor::FimExecutor(FimRuntimeType rt_type, FimPrecision precision)
    : rt_type_(rt_type), precision_(precision), thread_cnt_(16)
{
    DLOG(INFO) << "called ";

#ifdef EMULATOR
    fim_emulator_ = fim::runtime::emulator::FimEmulator::get_instance();
#endif
}

FimExecutor* FimExecutor::get_instance(FimRuntimeType rt_type, FimPrecision precision)
{
    DLOG(INFO) << "Called";
    static FimExecutor* instance_ = new FimExecutor(rt_type, precision);

    return instance_;
}

int FimExecutor::initialize(void)
{
    DLOG(INFO) << "Intialization done ";

    int ret = 0;
    hipGetDeviceProperties(&dev_prop_, 0);
    std::cout << " System minor " << dev_prop_.minor << std::endl;
    std::cout << " System major " << dev_prop_.major << std::endl;
    std::cout << " agent prop name " << dev_prop_.name << std::endl;
    std::cout << " hip Device prop succeeded " << std::endl;

#ifdef EMULATOR
    int dummy_size = 1;
#ifndef MULTI_CU
    int reserved_fmtd_size = 4 * 1024 * 1024;
#else
    int reserved_fmtd_size = MT_NUM * sizeof(FimMemTraceData);
#endif
    hipMalloc((void**)&fim_base_addr_, dummy_size);
    hipMalloc((void**)&d_fmtd16_, reserved_fmtd_size);
    hipMalloc((void**)&d_fmtd16_size_, sizeof(int));

    h_fmtd16_ = (FimMemTraceData*)malloc(reserved_fmtd_size);
    h_fmtd32_ = (FimMemTraceData*)malloc(reserved_fmtd_size);
    h_fmtd16_size_ = (int*)malloc(sizeof(int));
    h_fmtd32_size_ = (int*)malloc(sizeof(int));
#else
    /* TODO: get fim control base address from device driver */
    /* roct should write fim_base_va */
    FILE* fp;
    fp = fopen("fim_base_va.txt", "rt");
    fscanf(fp, "%lX", &fim_base_addr_);
    printf("fim_base_addr_ : 0x%lX\n", fim_base_addr_);
    fclose(fp);
    fmtd16_ = nullptr;
    fmtd32_ = nullptr;
    fmtd16_size_ = nullptr;
    fmtd32_size_ = nullptr;
#endif

    return ret;
}

int FimExecutor::deinitialize(void)
{
    DLOG(INFO) << "called";
    int ret = 0;

#ifdef EMULATOR
    hipFree((void*)fim_base_addr_);
    hipFree((void*)d_fmtd16_);
    hipFree((void*)d_fmtd16_size_);
    free(h_fmtd16_);
    free(h_fmtd16_size_);
    free(h_fmtd32_);
    free(h_fmtd32_size_);
#endif

    return ret;
}

int FimExecutor::execute(void* output, void* operand0, void* operand1, size_t size, FimOpType op_type)
{
    DLOG(INFO) << "called";
    int ret = 0;

    if (op_type == OP_ELT_ADD) {
    } else {
        /* todo:implement other operation function */
        return -1;
    }
    hipStreamSynchronize(NULL);

    return ret;
}

int FimExecutor::execute(FimBo* output, FimBo* operand0, FimBo* operand1, FimOpType op_type)
{
    DLOG(INFO) << "called";
    int ret = 0;
    size_t size = output->size;

    if (op_type == OP_ELT_ADD) {
    } else {
        /* todo:implement other operation function */
        hipLaunchKernelGGL(dummy_kernel, dim3(1), dim3(1), 0, 0);
        return -1;
    }
    hipStreamSynchronize(NULL);

    return ret;
}

int FimExecutor::execute(FimBo* output, FimBo* fim_data, FimOpType op_type)
{
    DLOG(INFO) << "called";
    int ret = 0;

    if (op_type == OP_ELT_ADD) {
        hipMemcpy((void*)fim_base_addr_, fim_data->data, fim_data->size, hipMemcpyHostToDevice);
#ifndef MULTI_CU
        hipLaunchKernelGGL(elt_add_fim_1cu_1th_fp16, dim3(1), dim3(1), 0, 0, (uint8_t*)fim_base_addr_,
                           (uint8_t*)fim_base_addr_, (uint8_t*)output->data, (int)output->size,
                           (FimMemTraceData*)d_fmtd16_, (int*)d_fmtd16_size_);
#else
        const unsigned blocks = BLOCKS;
        const unsigned threads_per_block = 16;
        const unsigned input_size = blocks * 1024;
        hipLaunchKernelGGL(elt_add_fim, dim3(blocks), dim3(threads_per_block), 0, 0, (uint8_t*)fim_base_addr_,
                           (uint8_t*)fim_base_addr_, (uint8_t*)output->data, input_size, d_fmtd16_,
                           (int*)d_fmtd16_size_, WIDTH);
#endif
    } else {
        /* todo:implement other operation function */
        hipLaunchKernelGGL(dummy_kernel, dim3(1), dim3(1), 0, 0);
        return -1;
    }
    hipStreamSynchronize(NULL);
#ifdef EMULATOR
    hipMemcpy((void*)h_fmtd16_size_, (void*)d_fmtd16_size_, sizeof(int), hipMemcpyDeviceToHost);
#ifndef MULTI_CU
    hipMemcpy((void*)h_fmtd16_, (void*)d_fmtd16_, sizeof(FimMemTraceData) * h_fmtd16_size_[0], hipMemcpyDeviceToHost);
#else
    /* for verifying output instantly, to be removed */
    hipMemcpy((void*)h_fmtd16_, (void*)d_fmtd16_, sizeof(FimMemTraceData) * MT_NUM, hipMemcpyDeviceToHost);
    FILE* fp2;
    fp2 = fopen("out.txt", "w");
    for (size_t i = 0; i < BLOCKS; i++) {
        for (size_t j = 0; j < h_fmtd16_size_[0]; j++) {
            int idx = i * WIDTH + j;
            fprintf(fp2, "[memt] Block: %d Thread: %d Addr: %lx Data: %lx%lx Cmd: %c\n", h_fmtd16_[idx].block_id,
                    h_fmtd16_[idx].thread_id, h_fmtd16_[idx].addr, ((uint64_t*)(h_fmtd16_[idx].data))[1],
                    ((uint64_t*)(h_fmtd16_[idx].data))[0], h_fmtd16_[idx].cmd);
        }
    }
    fclose(fp2);
#endif
    fim_emulator_->convert_mem_trace_from_16B_to_32B(h_fmtd32_, h_fmtd32_size_, h_fmtd16_, h_fmtd16_size_[0]);
    fim_emulator_->execute_fim(output, fim_data, h_fmtd32_, h_fmtd32_size_[0], op_type);
#endif

    return ret;
}

} /* namespace executor */
} /* namespace runtime */
} /* namespace fim */
