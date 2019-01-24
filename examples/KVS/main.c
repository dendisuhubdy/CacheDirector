/* 
 * An emulated KVS
 * 
 * This application considers the second 64B of packets as keys and it performs an operation (read/write) on the value indexed by the received key.
 * 
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#define _BSD_SOURCE             /* See feature_test_macros(7) */
#include <endian.h>
#include <rte_slice.h>
#include <signal.h>
#include <math.h>
#include <stdbool.h>

#define SLICE /* If defined, the initial array would be allocated based on slice-aware memory allocation. */
#define HASWELL

//#define TX_LOOP		/* To ensure that all processed packet are sent */

#define CYCLE_MEASURE		/* To measure the processing cycles per packet */

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define NB_KEYS (1024*1024*1024L)/64

#define READ_PERCENT 100
//#define DEBUG
// #define PRINT_TIMESTAMP

static volatile bool force_quit;

static void
signal_handler(int signum)
{
        if (signum == SIGINT || signum == SIGTERM) {
                printf("\n\nSignal %d received, preparing to exit...\n",
                                signum);
                force_quit = true;
        }
}

static const struct rte_eth_conf port_conf_default = {
        .rxmode = {
                .max_rx_pkt_len = ETHER_MAX_LEN,
        },
};

/* Simple Memcached application */

/*
 * Key/Value struct
 */

typedef struct {
	union{
		uint8_t key[64];	/* 64B Key*/
		uint8_t value[64];	/* 64B Value*/ 
	};
} rte_keyvalue;

/*
 * Print MAC address
 */

static inline 
void print_mac(struct ether_addr *addr)
{
	printf("MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			addr->addr_bytes[0], addr->addr_bytes[1],
			addr->addr_bytes[2], addr->addr_bytes[3],
			addr->addr_bytes[4], addr->addr_bytes[5]);
}

 /* Swap MAC */
static inline 
void ether_mirror(struct ether_hdr *eth)
{
    /* void * memcpy ( void * destination, const void * source, size_t num );*/

    uint8_t tmpa[6]; /* 48-bit MAC address */
    memcpy(tmpa, &eth->d_addr, 6);
    memcpy(&eth->d_addr, &eth->s_addr, 6);
    memcpy(&eth->s_addr, tmpa, 6);
}

/* Read Key */
static inline
unsigned long long read_key_from_packet(struct rte_mbuf *m)
{
	uint8_t *data=rte_pktmbuf_mtod(m, uint8_t *);


	#ifdef DEBUG
		#ifdef	PRINT_TIMESTAMP
			/* Print Timestamp - Tag*/
			printf("Tag: %02" PRIx8"", *(data+54));
			printf("%02" PRIx8"\n", *(data+55));
			/* Print Timestamp - Packet ID*/
			printf("ID: %02" PRIx8"", *(data+56));
		    printf("%02" PRIx8"", *(data+57));
			printf("%02" PRIx8"", *(data+58));
			printf("%02" PRIx8"\n", *(data+59));
		#endif
	#endif
	
	/* Print Key */
	/* Check this link for endian coversion: https://linux.die.net/man/3/endian */
	rte_keyvalue *key= (rte_keyvalue*)(data+65);
	uint64_t *key_64=(uint64_t*)key;
	#ifdef DEBUG
		printf("Key-Hexadecimal: %"PRIx64"\n",be64toh(*key_64));
		printf("Key-Decimal: %"PRIu64"\n",be64toh(*key_64));
	#endif

	/* Convert Big endian to little endian */
	return be64toh(*key_64);
}

/* Write Value */
static inline
void write_value_to_packet(struct rte_mbuf *m, unsigned long long *found_value)
{
	uint8_t *data=rte_pktmbuf_mtod(m, uint8_t *);
	/*void * memcpy ( void * destination, const void * source, size_t num );*/
	memcpy(data+65,found_value,64);
}

/* Create array */
static void**
create_array(unsigned long long nb_keys
	#ifdef SLICE
		,uint16_t slice
	#endif
		)
		
{
	/* Keep the addresses for different values */
	void **array=malloc(nb_keys*sizeof(void*));
	unsigned long long i=0;
	
	#ifndef SLICE
		/* Allocate memory for nb_keys through DPDK + get physical address*/
		void *mem_virtual=(void*)rte_calloc(0,nb_keys,64,0);
		rte_iova_t mem_iova=rte_malloc_virt2iova(mem_virtual);
		unsigned long long offset=0;
	#else
		/* Allocate memory for nb_keys through DPDK + save physical addresses in an array*/
		uint64_t *array_iova=malloc(nb_keys*sizeof(uint64_t));
		void *mem_virtual=(void*)rte_calloc(0,nb_keys*8,64,0);
		rte_iova_t mem_iova=rte_malloc_virt2iova(mem_virtual);
		clock_t start,end;
		float seconds;
		unsigned long long interval = 500000;
		/* Find the first chunk mapped to the 'slice' */
		#ifndef HASWELL
			/* Use UNCORE for SkyLake */
			unsigned long long offset=sliceFinder_uncore(mem_virtual,slice);
		#else
			/* Use hash function for Haswell */
			unsigned long long offset=sliceFinder_HF_haswell((uint64_t)mem_iova,slice);
		#endif

	#endif
	
	for(i=0;i<nb_keys;i++)
	{
		#ifndef SLICE
			/* Each 64B will be a chunk */
			array[i]=(void*)((char*)mem_virtual+(unsigned long long)offset);
			offset+=64;
		#else
			/* First chunk has already been found earlier */
			if(i==0)
			{
				start=clock();
				array[0]=(void*)((char*)mem_virtual+(unsigned long long)offset);
				array_iova[0]=mem_iova+offset;
			}
			else
			{
				/* Print the estimated time for every interval */
				if(i%interval==0)
				{
					end=clock();
					seconds = (float)(end-start)/CLOCKS_PER_SEC;
					start=end;
					printf("i: %llu/%llu Remaining Time: %f min\n",i,nb_keys,((nb_keys-i)*seconds)/(60*interval));
				}
				/* Find the next chunk mapped to 'slice' */
				offset=64;
				#ifndef HASWELL
					offset+=sliceFinder_uncore(array[i-1]+offset,slice);
				#else
					offset+=sliceFinder_HF_haswell(array_iova[i-1]+offset,slice);
				#endif
				array[i]=(void*)((char*)array[i-1]+(unsigned long long)offset);
				array_iova[i]=array_iova[i-1]+offset;
			}
		#endif
		/* Initialize the values */
		uint64_t *value;
		value=(uint64_t*)array[i];
		*(value)=i*2;
		//printf("array %llu: %lx\n", i, (uint64_t)array[i]);
		
	}

	/* Validation for Haswell*/
	#ifdef SLICE 
		#ifdef HASWELL
			printf("Start verification!\n");
			for(i=0;i<nb_keys;i++)
			{
				if(calculateSlice_HF_haswell(array_iova[i])!=slice)
					printf("Error! Wrong Slice!\n");
			}
			printf("Done!\n");
		#endif
	#endif

	/* Print the info about the array */
	#ifndef SLICE
		printf("Array Allocated! Number:%llu IOVA: %lx\n", nb_keys, mem_iova);
	#else
		printf("Slice Array Allocated! Number:%llu IOVA: %lx\n", nb_keys, mem_iova);
	#endif

	printf("rte_keyvalue Size: %lu\n", sizeof(rte_keyvalue));
	return array;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	print_mac(&addr);


	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void
lcore_main(void)
{
	uint16_t port;
	uint16_t i=0;
	struct rte_mbuf *m;

	/* TODO: Use only masked ports*/
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Create Array */
	void** array=create_array(NB_KEYS
	#ifdef SLICE
		/* Pass the slice number:0 */
		,0
	#endif
		);

	uint16_t count=0;
	uint64_t TPS_num=0, packets_received=0, read_counter=0;
	#ifdef CYCLE_MEASURE
		uint64_t cycles=0;
		float cycles_avg=0, cycles_avg_old=0, cycles_std=0;
	#endif
	float TPS_std=0, TPS_avg=0, old_avg=0, TPS=0;
	uint64_t freq_MHZ=rte_get_tsc_hz()/1000000;
	//clock_t start=0,end=0;
	uint64_t start=0, end=0;
	//float seconds;
	/* Run until the application is quit or killed. */
	while (!force_quit) {

		/*
		 * Receive packets on a port and forward them on the paired
		 * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
		 */
		RTE_ETH_FOREACH_DEV(port) {
			/* Get burst of RX packets, from first port of pair. */
			struct rte_mbuf *bufs[BURST_SIZE];
			uint16_t nb_rx = rte_eth_rx_burst(port, 0,
					bufs, BURST_SIZE);

			if(likely(count!=0))
			{
				packets_received+=count;
				//seconds = (float)(end-start)/CLOCKS_PER_SEC;
				//seconds=(end-start)/rte_get_tsc_hz();	
				//TPS=count/seconds;
				TPS=(float)(count*freq_MHZ)/(end-start);
				old_avg=TPS_avg;
				TPS_num++;
				TPS_avg=TPS_avg+(float)((TPS-TPS_avg)/TPS_num);
				TPS_std=TPS_std+(float)((TPS-TPS_avg)*(TPS-old_avg));
				#ifdef CYCLE_MEASURE
					cycles=(end-start)/count;
					cycles_avg_old=cycles_avg;
					cycles_avg=cycles_avg+(float)((cycles-cycles_avg)/TPS_num);
					cycles_std=cycles_std+(float)((cycles-cycles_avg)*(cycles-cycles_avg_old));
				#endif

				count=0;
			}

			if (unlikely(nb_rx == 0))
			{
				continue;
			}
			
			//start=clock();
			start = rte_rdtsc();
			count=nb_rx;
			for(i=0;i<nb_rx;i++) {

				m = bufs[i];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));

				/* Read key*/
				unsigned long long key = read_key_from_packet(m);
				if(key<NB_KEYS)
				{
					unsigned long long *value;
					if(read_counter<READ_PERCENT) {
						/* READ Operation */

						/* Read value */
						value = array[key];
						/* Print value */
						#ifdef DEBUG
							printf("Value: %llu\n",*value);
						#endif
					} else {
						/* WRITE Operation */
						
						/* Write key into array */ 
						*(uint64_t*)array[key]=key;
						/* Ack value */
						*value=0;
					}
					/* Convert Little endian to Big endian */
                    *value=htobe64(*value);
                    /* Write value */
                    write_value_to_packet(m, value);
					read_counter=(read_counter+1)%100;
					/* Read_key again to check whether value is written correctly or not */
					#ifdef DEBUG
					key =read_key_from_packet(m);
					if (key!=be64toh(*value))
						printf("Value not written! Expected-Value: %llu Real-Value: %llu\n", (unsigned long long)be64toh(*value), key);
					#endif
					/* Swap MAC*/
					struct ether_hdr *eth;
	        		eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
	    			ether_mirror(eth);
				}
				else
				{
					printf("Out of order key!\nKey: %llu Packet Dropped!\n", key);
					/* Drop the packet! */
					//rte_pktmbuf_free((struct rte_mbuf *) m);
				}
			}
			//end=clock();
			end = rte_rdtsc();
			
			/* Send burst of TX packets, to second port of pair. */
			uint16_t nb_tx = rte_eth_tx_burst(port, 0,
                                        bufs, nb_rx);
			
			if (unlikely(nb_tx < nb_rx)) {
				#ifdef TX_LOOP
					struct rte_mbuf **not_sent=bufs+nb_tx;
					nb_rx-=nb_tx;
					do {
						nb_tx = rte_eth_tx_burst(port, 0,
                                        	not_sent, nb_rx);

						nb_rx-=nb_tx;
						not_sent+=nb_tx;
					} while (nb_rx != 0);
				#else
					/* Free any unsent packets. */
					uint16_t buf;
					for (buf = nb_tx; buf < nb_rx; buf++)
						rte_pktmbuf_free(bufs[buf]);
				#endif
				}
		}
	}
	
	if(TPS_num!=0)
	{
		printf("TPS avg: %f\n", TPS_avg);
		printf("TPS std: %f\n", sqrt(TPS_std/(TPS_num)));
		printf("Batch num: %"PRIu64"\n",TPS_num);
		printf("Packets: %"PRIu64"\n",packets_received);
		#ifdef CYCLE_MEASURE
			printf("Avg cycles-per-packet: %f\n",cycles_avg);
			printf("Avg cycles std: %f\n",sqrt(cycles_std/(TPS_num)));
		#endif
	}
	else
	{
		printf("TPS avg: 0\n");
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint16_t portid;
	force_quit=false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* TODO: Use only masked ports*/
	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 2 || (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}
