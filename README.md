# CacheDirector

CacheDirector is a network I/O solution that exploits [slice-aware memory management][slice-aware-repo], extends Data Direct I/O ([DDIO][ddio]),  and sends each packet's header directly to the correct slice in the LLC; hence, the CPU core that is responsible for processing a packet can access the packet header in fewer CPU cycles. For more information, check out our [paper][cachedirector-eurosys-paper].


## Building CacheDirector

To build, you can use the default DPDK build system.
```
make install T=x86_64-native-linuxapp-gcc
```
The current version of CacheDirector is tested on systems with Intel(R) Xeon(R) E5-2667 v3 and Intel(R) Xeon(R) Gold 6134, which are equipped with Mellanox ConnectX-4 MT27700 card. 

### Extra Consideration 

- Please set `CONFIG_RTE_PKTMBUF_HEADROOM` according to your CPU architecture. You can find more information in `./config/common_base`.
- Please define/undefine `HASWELL` or `SKYLAKE` in `./lib/librte_msr/rte_msr.h` according to your CPU architecture.
- Please define/undefine `UNCORE` in `./lib/librte_slice/rte_slice.h` for using uncore performance counters, rather than hash function. It is important to notice that hash function only works on Haswell architecture.

## Using CacheDirector with FastClick

You can try CacheDirector with any DPDK-based application. However, if you want to use [FastClick][fastclick] or [Metron][metron]. You have to modify `$fastclick-path/userlevel/dpdk.mk` to support new libraries added for CacheDirector, as below:

```
ifneq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MSR)            += -lrte_msr 	  # Added
_LDLIBS-$(CONFIG_RTE_LIBRTE_SLICE)          += -lrte_slice  # Added
_LDLIBS-$(CONFIG_RTE_LIBRTE_KVARGS)         += -lrte_kvargs
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF)           += -lrte_mbuf
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF_OFFLOAD)   += -lrte_mbuf_offload
_LDLIBS-$(CONFIG_RTE_LIBRTE_IP_FRAG)        += -lrte_ip_frag
```

Then, you need to configure and make FastClick:

```
./configure RTE_SDK=$CACHEDIRECTORPATH RTE_TARGET=x86_64-native-linuxapp-gcc --enable-multithread --disable-linuxmodule --enable-intel-cpu --enable-user-multithread --verbose CFLAGS="-std=gnu11 -O3" CXXFLAGS="-std=gnu++14 -O3" --disable-dynamic-linking --enable-poll --enable-bound-port-transfer --enable-dpdk --enable-batch --with-netmap=no --enable-zerocopy --disable-dpdk-pool --disable-dpdk-packet --enable-local --enable-nanotimestamp --enable-ipsec --enable-analysis --disable-ip6 --enable-simple --enable-etherswitch --enable-json --enable-all-elements

make

```


## Building an emulated slice-aware Key-Value Store (KVS)

```
export RTE_SDK=$CACHEDIRECTOR_PATH
export RTE_TARGET=x86_64-native-linuxapp-gcc
cd examples/KVS
make
```


## Citing our paper

If you use CacheDirector or [slice-aware memory management][slice-aware-repo] in any context, please cite our [paper][cachedirector-eurosys-paper]:

```
@inproceedings{farshin-slice-aware,
 author = {Farshin, Alireza and Roozbeh, Amir and Maguire Jr., Gerald Q. and Kosti\'{c}, Dejan},
 title = {Make the Most out of Last Level Cache in Intel Processors},
 booktitle = {Proceedings of the Fourteenth EuroSys Conference},
 series = {EuroSys '19},
 year = {2019},
 isbn = {978-1-4503-6281-8/19/03},
 location = {Dresden, Germany},
 doi = {10.1145/3302424.3303977},
 acmid = {3303977},
 publisher = {ACM},
 address = {New York, NY, USA},
}
```

## Getting Help

If you have any questions regarding our code or the paper, you can contact Amir Roozbeh (amirrsk at kth.se) and/or Alireza Farshin (farshin at kth.se).

[cachedirector-eurosys-paper]: https://people.kth.se/~farshin/documents/slice-aware-eurosys19.pdf
[slice-aware-repo]: https://github.com/aliireza/slice-aware
[ddio]: https://www.intel.com/content/www/us/en/io/data-direct-i-o-technology.html
[fastclick]: https://github.com/tbarbette/fastclick/
[metron]: https://github.com/tbarbette/fastclick/tree/metron
