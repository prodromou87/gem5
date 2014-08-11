import m5
from m5.objects import *
from Caches import *

def config_onchip_memory(options, system):
    if options.cpu_type == "arm_detailed":
        try:
            from O3_ARM_v7a import *
        except:
            print "arm_detailed is unavailable. Did you compile the O3 model?"
            sys.exit(1)
        dcache_class, icache_class, l2_cache_class, on_chip_dram = \
            O3_ARM_v7a_DCache, O3_ARM_v7a_ICache, O3_ARM_v7aL2, OnChip_WideIO_200_x128
    else:
        dcache_class, icache_class, l2_cache_class, on_chip_dram = \
            L1Cache, L1Cache, L2Cache, OnChip_WideIO_200_x128


# I need to model PRIVATE L1, PRIVATE L2 and SHARED DRAM acting as LLC

    # Create 4 On-Chip Wide I/O DRAM channels (controllers)
    system.ocram = on_chip_dram()
    system.toocrambus = CoherentBus(clk_domain = system.cpu_clk_domain,
				width=128)
    system.ocram.cpu_side = system.toocrambus.master
    system.ocram.mem_side = system.membus.slave

    for i in xrange(options.num_cpus):
	#Private L1 (I/D)
	icache = icache_class(size=options.l1i_size,
			      assoc=options.l1i_assoc)
	dcache = dcache_class(size=options.l1d_size,
			      assoc=options.l1d_assoc)

	#Private L2
	system.cpu[i].l2 = l2_cache_class(clk_domain=system.cpu_clk_domain,
					    size=options.l2_size,
					    assoc=options.l2_assoc)
	system.cpu[i].tol2bus = CoherentBus(clk_domain = system.cpu_clk_domain,
					    width=32)
	system.cpu[i].l2.cpu_side = system.cpu[i].tol2bus.master
	system.cpu[i].l2.mem_side = system.toocrambus.slave

	# When connecting the caches, the clock is also inherited
	# from the CPU in question
	if buildEnv['TARGET_ISA'] == 'x86':
	    system.cpu[i].addPrivateSplitL1Caches(icache, dcache,
						  PageTableWalkerCache(),
						  PageTableWalkerCache())
	else:
	    system.cpu[i].addPrivateSplitL1Caches(icache, dcache)

        system.cpu[i].createInterruptController()
	system.cpu[i].connectAllPorts(system.toocrambus, system.membus)


def config_cache(options, system):
    if options.cpu_type == "arm_detailed":
        try:
            from O3_ARM_v7a import *
        except:
            print "arm_detailed is unavailable. Did you compile the O3 model?"
            sys.exit(1)

    #PRODROMOU

#        dcache_class, icache_class, l2_cache_class = \
#            O3_ARM_v7a_DCache, O3_ARM_v7a_ICache, O3_ARM_v7aL2
#    else:
#        dcache_class, icache_class, l2_cache_class = \
#            L1Cache, L1Cache, L2Cache

        dcache_class, icache_class, l2_cache_class, l3_cache_class = \
            O3_ARM_v7a_DCache, O3_ARM_v7a_ICache, O3_ARM_v7aL2, O3_ARM_v7aL3
    else:
        dcache_class, icache_class, l2_cache_class, l3_cache_class = \
            L1Cache, L1Cache, L2Cache, L3Cache
    #PRODROMOU

    # Set the cache line size of the system
    system.cache_line_size = options.cacheline_size

    #PRODROMOU
    if options.l3cache:
        system.l3 = l3_cache_class(clk_domain=system.cpu_clk_domain,
                                    size=options.l3_size,
                                    assoc=options.l3_assoc)
        system.tol3bus = CoherentBus(clk_domain = system.cpu_clk_domain,
				    width=32)
        system.l3.cpu_side = system.tol3bus.master
        system.l3.mem_side = system.membus.slave

    else:
    #PRODROMOU
	if options.l2cache:
	    # Provide a clock for the L2 and the L1-to-L2 bus here as they
	    # are not connected using addTwoLevelCacheHierarchy. Use the
	    # same clock as the CPUs, and set the L1-to-L2 bus width to 32
	    # bytes (256 bits).
	    system.l2 = l2_cache_class(clk_domain=system.cpu_clk_domain,
				       size=options.l2_size,
				       assoc=options.l2_assoc)

	    system.tol2bus = CoherentBus(clk_domain = system.cpu_clk_domain,
					 width = 32)
	    system.l2.cpu_side = system.tol2bus.master
	    system.l2.mem_side = system.membus.slave

    for i in xrange(options.num_cpus):
        if options.caches:
            icache = icache_class(size=options.l1i_size,
                                  assoc=options.l1i_assoc)
            dcache = dcache_class(size=options.l1d_size,
                                  assoc=options.l1d_assoc)

            #PRODROMOU
            if options.l3cache:
                system.cpu[i].l2 = l2_cache_class(clk_domain=system.cpu_clk_domain,
                                                    size=options.l2_size,
                                                    assoc=options.l2_assoc)
                system.cpu[i].tol2bus = CoherentBus(clk_domain = system.cpu_clk_domain,
						    width=32)
                system.cpu[i].l2.cpu_side = system.cpu[i].tol2bus.master
                system.cpu[i].l2.mem_side = system.tol3bus.slave
            #PRODROMOU

            # When connecting the caches, the clock is also inherited
            # from the CPU in question
            if buildEnv['TARGET_ISA'] == 'x86':
                system.cpu[i].addPrivateSplitL1Caches(icache, dcache,
                                                      PageTableWalkerCache(),
                                                      PageTableWalkerCache())
            else:
                system.cpu[i].addPrivateSplitL1Caches(icache, dcache)

        system.cpu[i].createInterruptController()

        #PRODROMOU
        if options.l3cache:
            system.cpu[i].connectAllPorts(system.cpu[i].tol2bus, system.membus)
        elif options.l2cache:
        #PRODROMOU
#        if options.l2cache:
	    system.cpu[i].connectAllPorts(system.tol2bus, system.membus)
	else:
	    system.cpu[i].connectAllPorts(system.membus)
	
    return system


