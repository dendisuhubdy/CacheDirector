/* 
 * Functions for reading and writing MSR registers, configuring CHA/CBO registers,
 * polling an address, and finding the slice counter (i.e., CBO/CHA) with highest number
 * 
 * For more info refer to: https://github.com/aliireza/slice-aware
 *
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 *
 */

#include <rte_msr.h>

/* MSR Addresses */

const unsigned long long * CHA_CBO_EVENT_ADDRESS = (unsigned long long []) {0x0E01, 0x0E11, 0x0E21, 0x0E31, 0x0E41, 0x0E51, 0x0E61, 0x0E71, 0x0E81, 0x0E91,

                                  0x0EA1, 0x0EB1, 0x0EC1, 0x0ED1, 0x0EE1, 0x0EF1, 0x0F01, 0x0F11, 0x0F21, 0x0F31, 0x0F41,

                                  0x0F51, 0x0F61, 0x0F71, 0x0F81, 0x0F91, 0x0FA1, 0x0FB1};

const unsigned long long * CHA_CBO_CTL_ADDRESS = (unsigned long long []) {0x0E00, 0x0E10, 0x0E20, 0x0E30, 0x0E40, 0x0E50, 0x0E60, 0x0E70, 0x0E80, 0x0E90,

                                  0x0EA0, 0x0EB0, 0x0EC0, 0x0ED0, 0x0EE0, 0x0EF0, 0x0F00, 0x0F10, 0x0F20, 0x0F30, 0x0F40,

                                  0x0F50, 0x0F60, 0x0F70, 0x0F80, 0x0F90, 0x0FA0, 0x0FB0};

const unsigned long long * CHA_CBO_FILTER_ADDRESS = (unsigned long long []) {0x0E05, 0x0E15, 0x0E25, 0x0E35, 0x0E45, 0x0E55, 0x0E65, 0x0E75, 0x0E85, 0x0E95,

                                  0x0EA5, 0x0EB5, 0x0EC5, 0x0ED5, 0x0EE5, 0x0EF5, 0x0F05, 0x0F15, 0x0F25, 0x0F35, 0x0F45,

                                  0x0F55, 0x0F65, 0x0F75, 0x0F85, 0x0F95, 0x0FA5, 0x0FB5};

const unsigned long long * CHA_CBO_COUNTER_ADDRESS = (unsigned long long []) {0x0E08, 0x0E18, 0x0E28, 0x0E38, 0x0E48, 0x0E58, 0x0E68, 0x0E78, 0x0E88, 0x0E98,

                                  0x0EA8, 0x0EB8, 0x0EC8, 0x0ED8, 0x0EE8, 0x0EF8, 0x0F08, 0x0F18, 0x0F28, 0x0F38, 0x0F48,
                                                                                                                                0x0F58, 0x0F68, 0x0F78, 0x0F88, 0x0F98, 0x0FA8, 0x0FB8};



/*
 * Read an MSR on CPU 0
 */

uint64_t rdmsr_on_cpu_0(uint32_t reg)
{

	uint64_t data;
	int cpu = 0;
	const char* msr_file_name = "/dev/cpu/0/msr";

	static int fd = -1;


	if (fd < 0) {
		fd = open(msr_file_name, O_RDONLY);
		if(fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
					cpu);
				exit(3);
			} else {
				perror("rdmsr: open");
				exit(127);
			}
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d cannot read "
				"MSR 0x%08"PRIx32"\n",
				cpu, reg);
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

  //close(fd);

	return data;
}

/*
 * Write to an MSR on CPU 0
 */

void
wrmsr_on_cpu_0(uint32_t reg, int valcnt, uint64_t *regvals)
{

	uint64_t data;
	const char* msr_file_name = "/dev/cpu/0/msr";
	int cpu = 0;

	static int fd = -1;

	if(fd < 0){
		fd = open(msr_file_name, O_WRONLY);
		if (fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
					cpu);
				exit(3);
			} else {
				perror("wrmsr: open");
				exit(127);
			}
		}
	}

	while (valcnt--) {
		data=*regvals++;
		if (pwrite(fd, &data, sizeof data, reg) != sizeof data) {
			if (errno == EIO) {
				fprintf(stderr,
					"wrmsr: CPU %d cannot set MSR "
					"0x%08"PRIx32" to 0x%016"PRIx64"\n",
					cpu, reg, data);
				exit(4);
			} else {
				perror("wrmsr: pwrite");
				exit(127);
			}
		}
	}

  //close(fd);

	return;
}

/*
 * Polling one address
 */

void
polling(void* address)
{
        unsigned long i;
        for (i=0;i<NUMBER_POLLING;i++) {
                _mm_clflush(address);
        }
}


/*
 * Initialize uncore registers (CBo/CHA and Global MSR) before polling
 */

void
uncore_init(void)
{

        int i;

        /* Setup monitoring session */

        /* Disable counters */
        uint64_t register_value[] = {DISABLE_COUNT};
        register_value[0]=DISABLE_COUNT;
        wrmsr_on_cpu_0(PMON_GLOBAL_CTL_ADDRESS,1,register_value);

        /* Select the event to monitor */
        register_value[0]=SELECTED_EVENT;
        for(i=0; i<NUMBER_SLICES; i++) {
                wrmsr_on_cpu_0(CHA_CBO_EVENT_ADDRESS[i], 1, register_value);
        }

        /* Reset CHA Counters */
        register_value[0]=RESET_COUNTERS;
        for(i=0; i<NUMBER_SLICES; i++) {
                wrmsr_on_cpu_0(CHA_CBO_CTL_ADDRESS[i],1,register_value);
        }

        /* Set Filter BOX */
        register_value[0]=FILTER_BOX_VALUE;
        for(i=0; i<NUMBER_SLICES; i++) {
                wrmsr_on_cpu_0(CHA_CBO_FILTER_ADDRESS[i], 1, register_value);
        }

        /* Enable counting */
        register_value[0]=ENABLE_COUNT;
        wrmsr_on_cpu_0(PMON_GLOBAL_CTL_ADDRESS, 1, register_value);
}

/*
 * Read the CBo/CHA counters' value and find the one with maximum number
 */

int
find_CHA_CBO(void)
{
        int i;
        unsigned long long* CHA_CBO_value = calloc(NUMBER_SLICES, sizeof(unsigned long long));

        /* Read CHA/CBo counter's value */
        for(i=0; i<NUMBER_SLICES; i++){
                CHA_CBO_value[i] = rdmsr_on_cpu_0(CHA_CBO_COUNTER_ADDRESS[i]);
        }

        /* Find maximum */
        unsigned long long max_value=0;
        int max_index=0;
        for(i=0; i<NUMBER_SLICES; i++){
                //printf(" %llu", CHA_CBO_value[i]);
                if(CHA_CBO_value[i]>max_value){
                        max_value=CHA_CBO_value[i];
                        max_index=i;
                }
        }
		//printf("\n");
        return max_index;
}

