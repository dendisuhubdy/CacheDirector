/* 
 * LLC-slice-related functions for calculating the slice number and finding the appropriate offset
 * For more info refer to: https://github.com/aliireza/slice-aware
 *
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 */

#include <rte_slice.h>

#include <inttypes.h>
#include <stdint.h>


/* Calculate slice based on a given virtual address - Haswell and SkyLake */
uint8_t
calculateSlice_uncore(void* va)
{
        /*
         * The registers' address and their values would be selected in rte_msr.h
         * The default CPU is SkyLake now. For Haswell (#define SKYLAKE) should be changed to (#define HASWELL) in rte_msr.h
         */
        uncore_init();
        polling(va);
        return find_CHA_CBO();
}


/* Find the next chunk for the input coreNumber, using calculateSlice_uncore */
uint16_t
sliceFinder_uncore(void* va, uint8_t desiredSlice)
{
  uint16_t offset=0;
  while(desiredSlice!=calculateSlice_uncore((void*)((uint64_t)va+(uint64_t)offset)))
  {
    /* Slice mapping will change for each cacheline which is 64 Bytes */
    offset+=LINE;
  }
  return offset;
}

