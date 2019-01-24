/* 
 * LLC-slice-related functions for calculating the slice number and finding the appropriate offset
 * For more info refer to: https://github.com/aliireza/slice-aware
 *
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 */

#ifndef _RTE_SLICE_H_
#define _RTE_SLICE_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <rte_msr.h>

//#define UNCORE /* If defined, the mapping will be fined through polling for both Haswell and Skylake. */

/* Architecture dependent values for LLC hash function
 *
 * Xeon-E5-2667 v3 (Haswell)
 * 8 cores in each socket -> 8 slices (2.5MB)
 * Each physical address will be mapped to a slice by XOR-ing the selected bits
 * The bits in physical_addr will be selected by using a hash_x
 * x=0,1,2
 * Then we will XOR all of those bits
 * Bitx = XOR( and(physical_addr,hash_x))
 * Output slice is the decimal value of: (Bit2.Bit1.Bit0)
 *
 * Xeon-Gold-6134 (SkyLake Server)
 * 8 cores in each socket and 18 slices per socket (1.375MB)
 * Mapping is not known yet
 */

/*
 * Haswell Hash Function
 */

/* Number of hash functions/Bits for slices */
#define bitNum 3
/* Bit0 hash*/
#define hash_0 0x1B5F575440
/* Bit1 hash*/
#define hash_1 0x2EB5FAA880
/* Bit2 hash*/
#define hash_2 0x3CCCC93100

/* Socket Number & Core Numbers for Xeon-E5-2667 and Xeon-Gold-6134 */

#define socketNumber 2
#define coreNumber 8

/*
 * Cache Hierarchy Characteristics
 */

#define LINE  64
#define L1_SIZE (32UL*1024)
#define L1_WAYS 8
#define L1_SETS (L1_SIZE/LINE)/(L1_WAYS)

#define SKYLAKE_LLC_SIZE (24.75UL*1024*1024)
#define SKYLAKE_LLC_WAYS 11
#define SKYLAKE_LLC_SETS (LLC_SIZE/LINE)/(LLC_WAYS*NUMBER_SLICES)
#define SKYLAKE_SLICE_SIZE (LLC_SIZE/(NUMBER_SLICES))
#define SKYLAKE_L2_SIZE (1UL*1024*1024)
#define SKYLAKE_L2_WAYS 16
#define SKYLAKE_L2_SETS (L2_SIZE/LINE)/(L2_WAYS)

#define HASWELL_LLC_SIZE (20UL*1024*1024)
#define HASWELL_LLC_WAYS 20
#define HASWELL_LLC_SETS (LLC_SIZE/LINE)/(LLC_WAYS*NUMBER_SLICES)
#define HASWELL_SLICE_SIZE (LLC_SIZE/(NUMBER_SLICES))
#define HASWELL_L2_SIZE (256UL*1024)
#define HASWELL_L2_WAYS 8
#define HASWELL_L2_SETS (L2_SIZE/LINE)/(L2_WAYS)

/* Set indexes */

#define SKYLAKE_L3_INDEX_PER_SLICE 0x1FFC0 /* 11 bits - [16-6] - 2048 sets per slice + 11 way for each slice (1.375MB) */
#define SKYLAKE_L2_INDEX 0xFFC0 /* 10 bits - [15-6] - 1024 sets + 16 way for each core  */
#define SKYLAKE_L1_INDEX 0xFC0 /* 6 bits - [11-6] - 64 sets + 8 way for each core  */
#define SKYLAKE_L3_INDEX_STRIDE 0x20000 /* Offset required to get the same indexes bit 17 = bit 16 (MSB bit of L3_INDEX_PER_SLICE) + 1 */
#define SKYLAKE_L2_INDEX_STRIDE 0x10000 /* Offset required to get the same indexes bit 16 = bit 15 (MSB bit of L2_INDEX) + 1 */

#define HASWELL_L3_INDEX_PER_SLICE 0x1FFC0 /* 11 bits - [16-6] - 2048 sets per slice + 20 way for each slice (2.5MB) */
#define HASWELL_L2_INDEX 0x7FC0 /* 9 bits - [14-6] - 512 sets + 8 way for each core  */
#define HASWELL_L1_INDEX 0xFC0 /* 6 bits - [11-6] - 64 sets + 8 way for each core  */
#define HASWELL_L3_INDEX_STRIDE 0x20000 /* Offset required to get the same indexes bit 17 = bit 16 (MSB bit of L3_INDEX_PER_SLICE) + 1 */
#define HASWELL_L2_INDEX_STRIDE 0x8000 /* Offset required to get the same indexes bit 15 = bit 14 (MSB bit of L2_INDEX) + 1 */

#ifdef SKYLAKE
#define L3_INDEX_PER_SLICE SKYLAKE_L3_INDEX_PER_SLICE
#define L2_INDEX SKYLAKE_L2_INDEX
#define L1_INDEX SKYLAKE_L1_INDEX
#define LLC_SIZE SKYLAKE_LLC_SIZE
#define LLC_WAYS SKYLAKE_LLC_WAYS
#define LLC_SETS SKYLAKE_LLC_SETS
#define SLICE_SIZE SKYLAKE_SLICE_SIZE
#define L2_SIZE SKYLAKE_L2_SIZE
#define L2_WAYS SKYLAKE_L2_WAYS
#define L2_SETS SKYLAKE_L2_SETS
#define L3_INDEX_STRIDE SKYLAKE_L3_INDEX_STRIDE
#define L2_INDEX_STRIDE SKYLAKE_L2_INDEX_STRIDE
#else
#define L3_INDEX_PER_SLICE HASWELL_L3_INDEX_PER_SLICE
#define L2_INDEX HASWELL_L2_INDEX
#define L1_INDEX HASWELL_L1_INDEX
#define LLC_SIZE HASWELL_LLC_SIZE
#define LLC_WAYS HASWELL_LLC_WAYS
#define LLC_SETS HASWELL_LLC_SETS
#define SLICE_SIZE HASWELL_SLICE_SIZE
#define L2_SIZE HASWELL_L2_SIZE
#define L2_WAYS HASWELL_L2_WAYS
#define L2_SETS HASWELL_L2_SETS
#define L3_INDEX_STRIDE HASWELL_L3_INDEX_STRIDE
#define L2_INDEX_STRIDE HASWELL_L2_INDEX_STRIDE
#endif

/* Function for XOR-ing all bits
 * ma: masked physical address
 */

inline uint64_t
rte_xorall64(uint64_t ma)
{
        return __builtin_parityll(ma);
}


/* Calculate slice based on the physical address - Haswell */
inline uint8_t
calculateSlice_HF_haswell(uint64_t pa)
{
	uint8_t sliceNum=0;
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_2));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_1));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_0));
	return sliceNum;
}

/* Calculate slice based on the virtual address - Haswell and SkyLake */
uint8_t calculateSlice_uncore(void* va);



/* Find the next slice for the input coreNumber, using calculateSlice_uncore */
uint16_t sliceFinder_uncore(void* va, uint8_t desiredSlice);


/* Find the next slice for the input coreNumber, using calculateSlice_HF_haswell */
inline uint16_t
sliceFinder_HF_haswell(uint64_t pa, uint8_t desiredSlice)
{
  uint16_t offset=0;
  while(desiredSlice!=calculateSlice_HF_haswell(pa+(uint64_t)offset))
  {
    /* Slice mapping will change for each cacheline which is 64 Bytes */
    offset+=LINE;
  }
  return offset;
}

/* Calculate the index of set for a physical address */
inline uint64_t
indexCalculator(uint64_t addr_in, int cacheLevel)
{

	uint64_t index;

	if (cacheLevel==1)
	{
		index=(addr_in)&L1_INDEX;
		index = index >> 6;

	}
	else if (cacheLevel==2)
	{
		index=(addr_in)&L2_INDEX;
		index = index >> 6;
	}
	else if (cacheLevel==3)
	{
		index=(addr_in)&L3_INDEX_PER_SLICE;
		index = index >> 6;
	}
	else
	{
		exit(EXIT_FAILURE);
	}
	return index;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SLICE_H_ */
