/*
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#ifndef _MEMORY_MANAGER_H_
#define _MEMORY_MANAGER_H_

#include "manager/HipMemManager.h"
#include "manager/IPimMemoryManager.h"
#include "manager/OclMemManager.h"
#include "manager/PimDevice.h"
#include "manager/PimInfo.h"
#include "pim_data_types.h"

namespace pim
{
namespace runtime
{
namespace manager
{
class MemoryManager
{
   public:
    IPimMemoryManager* getPimMemoryManager(PimDevice* pim_device, PimRuntimeType rt_type, PimPrecision precision)
    {
        IPimMemoryManager* mem_manager = nullptr;

        if (rt_type == RT_TYPE_HIP) {
            mem_manager = new HipMemManager(pim_device, precision);
        } else if (rt_type == RT_TYPE_OPENCL) {
            mem_manager = new OclMemManager(pim_device, precision);
        } else {
            throw std::invalid_argument("invalid type of runtime");
        }
        return mem_manager;
    }
};

}  // namespace manager
}  // namespace runtime
}  // namespace pim

#endif  // _MEMORY_MANAGER_FACTORY_H_
