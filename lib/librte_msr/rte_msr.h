/* 
 * Functions for reading and writing MSR registers, configuring CHA/CBO registers,
 * polling an address, and finding the slice counter (i.e., CBO/CHA) with highest number
 * 
 * For more info refer to: https://github.com/aliireza/slice-aware
 *
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 *
 */

#ifndef _RTE_MSR_H_
#define _RTE_MSR_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for reading and writing MSR registers, configuring CHA/CBO registers,
 * polling an address, and finding the slice counter (i.e., CBO/CHA) with highest number
 */

/*
 * The code for the functions rdmsr_on_cpu_0 and wrmsr_on_cpu_0 are
 * originally part of msr-tools.
 * The rest of the code has been inspired by Clémentine Maurice paper and her repository:
 * Paper:
 * 	Reverse Engineering Intel Last-Level Cache Complex Addressing Using Performance Counters
 * 	https://dl.acm.org/citation.cfm?id=2939211 doi>10.1007/978-3-319-26362-5_3
 * Repository:
 * 	https://github.com/clementine-m/msr-uncore-cbo
 */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <cpuid.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif

/*
 * Definitions + MSR-related addresses
 */

/* Architecture related */
#define HASWELL /* Can be changed to HASWELL or SKYLAKE */
#define SKYLAKE_SERVER_MODEL 85
#define HASWELL_SERVER_MODEL 63

/* Number of polling for acquiring the slice */
#define NUMBER_POLLING 750

/* CBO/CHA addresses + values
 * For more info:
 * Check:
 * "Intel Xeon Processor E5 and E7 V3 Family Uncore Performance Monitoring"
 * Link: https://www.intel.com/content/www/us/en/processors/xeon/xeon-e5-v3-uncore-performance-monitoring.html
 * Or:
 * "Intel® Xeon® Processor Scalable Family Uncore Reference Manual"
 * Link: https://www.intel.com/content/www/us/en/processors/xeon/scalable/xeon-scalable-uncore-performance-monitoring-manual.html
 * Summary:
 *
 * For Setting up a Monitoring Session:
 * a)  Freeze all uncore counter -> Set U_MSR_PMON_GLOBAL_CTRL.frz_all
 * or
 * Freeze the box's counters -> Set Cn_MSR_PMON_BOX_CTL.frz
 *
 * b)  Enable counting for each monitor -> e.g., Set C0_MSR_PMON_CTL2.en
 *
 * c)  Select Event to monitor: program .ev_sel and .unmask_bits
 * e.g., Set C0_MSR_PMON_CTL2.{ev_sel,unmask} based on the table
 *
 * d)  Reset counters in each CHAx/CBox
 * e.g., For each CHA/CBo -> Set Cn_MSR_PMON_BOX_CTL[1:0] to 0x3
 *
 * e)  Select how to gather data -> Skip if polling, which is our case
 *
 * f)  Enable counting at the global level -> Set U_MSR_PMON_GLOBAL_CTRL.unfrz_all
 * or
 * Enable counting at the box level -> Set Cn_MSR_PMON_BOX_CTL.frz to 0
 */

/* MSR Addresses */

#define PMON_GLOBAL_CTL_ADDRESS 0x700

/* Addresses will be set in msr_utils.c */
extern const unsigned long long * CHA_CBO_EVENT_ADDRESS;
extern const unsigned long long * CHA_CBO_CTL_ADDRESS;
extern const unsigned long long * CHA_CBO_FILTER_ADDRESS;
extern const unsigned long long * CHA_CBO_COUNTER_ADDRESS;

/* MSR Values */
#define ENABLE_COUNT_SKYLAKE 0x2000000000000000
#define DISABLE_COUNT_SKYLAKE 0x8000000000000000
#define ENABLE_COUNT_HASWELL 0x20000000
#define DISABLE_COUNT_HASWELL 0x80000000
#define SELECTED_EVENT 0x441134 /* Event: LLC_LOOKUP Mask: Any request (All snooping signals) */
#define RESET_COUNTERS 0x30002
#define FILTER_BOX_VALUE_SKYLAKE 0x01FE0000
#define FILTER_BOX_VALUE_HASWELL 0x007E0000

#ifdef SKYLAKE
#define NUMBER_SLICES 28 /* Maximum number of slices in SkyLake architecture */
#define ENABLE_COUNT ENABLE_COUNT_SKYLAKE
#define DISABLE_COUNT DISABLE_COUNT_SKYLAKE
#define FILTER_BOX_VALUE FILTER_BOX_VALUE_SKYLAKE
#else
#define NUMBER_SLICES 8 /* Can be different for different CPUs */
#define ENABLE_COUNT ENABLE_COUNT_HASWELL
#define DISABLE_COUNT DISABLE_COUNT_HASWELL
#define FILTER_BOX_VALUE FILTER_BOX_VALUE_HASWELL
#endif

/*
 * Read an MSR on CPU 0
 */

uint64_t rdmsr_on_cpu_0(uint32_t reg);

/*
 * Write to an MSR on CPU 0
 */

void wrmsr_on_cpu_0(uint32_t reg, int valcnt, uint64_t *regvals);

/*
 * Polling one address
 */

void polling(void* address);

/*
 * Initialize uncore registers (CBo/CHA and Global MSR) before polling
 */

void uncore_init(void);

/*
 * Read the CBo/CHA counters' value and find the one with maximum number
 */

int find_CHA_CBO(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MSR_H_ */
