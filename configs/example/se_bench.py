# Copyright (c) 2012-2013 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2006-2008 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Steve Reinhardt

# Simple test script
#
# "m5 test.py"

import optparse
import sys

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal

addToPath('../common')
addToPath('../ruby')
addToPath('../topologies')

import Options
import Ruby
import Simulation
import CacheConfig
import MemConfig
from Caches import *
from cpu2000 import *

#Prodromou: Include SPEC2k6 files
import SPEC2k6_train
import SPEC2k6_ref

def get_processes(options):
    """Interprets provided options and returns a list of processes"""

    multiprocesses = []
    inputs = []
    outputs = []
    errouts = []
    pargs = []

    workloads = options.cmd.split(';')
    if options.input != "":
        inputs = options.input.split(';')
    if options.output != "":
        outputs = options.output.split(';')
    if options.errout != "":
        errouts = options.errout.split(';')
    if options.options != "":
        pargs = options.options.split(';')

    # Prodromou: Add required options here so I don't have to 
    # re-write them every time
    #options.cpu_type = "detailed"
    #options.caches = True

    #Prodromou: Invoke the benchmarks
    if options.benchmark:
	if options.bench_size == 'train':
	    if options.benchmark == 'perlbench':
		process = SPEC2k6_train.perlbench
	    elif options.benchmark == 'bzip2':
		process = SPEC2k6_train.bzip2
	    elif options.benchmark == 'gcc':
		process = SPEC2k6_train.gcc
	    elif options.benchmark == 'mcf':
		process = SPEC2k6_train.mcf
	    elif options.benchmark == 'milc':
		process = SPEC2k6_train.milc
	    elif options.benchmark == 'gobmk':
		process = SPEC2k6_train.gobmk
	    elif options.benchmark == 'hmmer':
		process = SPEC2k6_train.hmmer
	    elif options.benchmark == 'sjeng':
		process = SPEC2k6_train.sjeng
	    elif options.benchmark == 'libquantum':
		process = SPEC2k6_train.libquantum
	    elif options.benchmark == 'h264ref':
		process = SPEC2k6_train.h264ref
	    elif options.benchmark == 'lbm':
		process = SPEC2k6_train.lbm
	    elif options.benchmark == 'sphinx3':
		process = SPEC2k6_train.sphinx3
	    elif options.benchmark == 'specrand':
		process = SPEC2k6_train.specrand
	    else:
		 print "Error: Unknown Benchmark"
		 sys.exit(1)
	elif options.bench_size == 'ref':
	    if options.benchmark == 'perlbench':
                process = SPEC2k6_ref.perlbench
            elif options.benchmark == 'bzip2':
                process = SPEC2k6_ref.bzip2
            elif options.benchmark == 'gcc':
                process = SPEC2k6_ref.gcc
            elif options.benchmark == 'mcf':
                process = SPEC2k6_ref.mcf
            elif options.benchmark == 'milc':
                process = SPEC2k6_ref.milc
            elif options.benchmark == 'gobmk':
                process = SPEC2k6_ref.gobmk
            elif options.benchmark == 'hmmer':
                process = SPEC2k6_ref.hmmer
            elif options.benchmark == 'sjeng':
                process = SPEC2k6_ref.sjeng
            elif options.benchmark == 'libquantum':
                process = SPEC2k6_ref.libquantum
            elif options.benchmark == 'h264ref':
                process = SPEC2k6_ref.h264ref
            elif options.benchmark == 'lbm':
                process = SPEC2k6_ref.lbm
            elif options.benchmark == 'sphinx3':
                process = SPEC2k6_ref.sphinx3
            elif options.benchmark == 'specrand':
                process = SPEC2k6_ref.specrand
            elif options.benchmark == 'bwaves':
                process = SPEC2k6_ref.bwaves
            elif options.benchmark == 'gamess':
                process = SPEC2k6_ref.gamess
            elif options.benchmark == 'zeusmp':
                process = SPEC2k6_ref.zeusmp
            elif options.benchmark == 'leslie3d':
                process = SPEC2k6_ref.leslie3d
            elif options.benchmark == 'GemsFDTD':
                process = SPEC2k6_ref.GemsFDTD
            elif options.benchmark == 'tonto':
                process = SPEC2k6_ref.tonto
            elif options.benchmark == 'namd':
                process = SPEC2k6_ref.namd
            elif options.benchmark == 'dealII':
                process = SPEC2k6_ref.dealII
            elif options.benchmark == 'soplex':
                process = SPEC2k6_ref.soplex
            elif options.benchmark == 'povray':
                process = SPEC2k6_ref.povray
            elif options.benchmark == 'omnetpp':
                process = SPEC2k6_ref.omnetpp
            elif options.benchmark == 'astar':
                process = SPEC2k6_ref.astar
            elif options.benchmark == 'xalancbmk':
                process = SPEC2k6_ref.xalancbmk
            elif options.benchmark == 'gromacs':
                process = SPEC2k6_ref.gromacs
            elif options.benchmark == 'cactusADM':
                process = SPEC2k6_ref.cactusADM
            elif options.benchmark == 'calculix':
                process = SPEC2k6_ref.calculix
            elif options.benchmark == 'wrf':
                process = SPEC2k6_ref.wrf
	    elif options.benchmark == 'perlbench_x86':
                process = SPEC2k6_ref.perlbench_x86
            elif options.benchmark == 'bzip2_x86':
                process = SPEC2k6_ref.bzip2_x86
            elif options.benchmark == 'gcc_x86':
                process = SPEC2k6_ref.gcc_x86
            elif options.benchmark == 'mcf_x86':
                process = SPEC2k6_ref.mcf_x86
            elif options.benchmark == 'milc_x86':
                process = SPEC2k6_ref.milc_x86
            elif options.benchmark == 'gobmk_x86':
                process = SPEC2k6_ref.gobmk_x86
            elif options.benchmark == 'hmmer_x86':
                process = SPEC2k6_ref.hmmer_x86
            elif options.benchmark == 'sjeng_x86':
                process = SPEC2k6_ref.sjeng_x86
            elif options.benchmark == 'libquantum_x86':
                process = SPEC2k6_ref.libquantum_x86
            elif options.benchmark == 'h264ref_x86':
                process = SPEC2k6_ref.h264ref_x86
            elif options.benchmark == 'lbm_x86':
                process = SPEC2k6_ref.lbm_x86
            elif options.benchmark == 'sphinx3_x86':
                process = SPEC2k6_ref.sphinx3_x86
            elif options.benchmark == 'specrand_x86':
                process = SPEC2k6_ref.specrand_x86
            elif options.benchmark == 'bwaves_x86':
                process = SPEC2k6_ref.bwaves_x86
            elif options.benchmark == 'gamess_x86':
                process = SPEC2k6_ref.gamess_x86
            elif options.benchmark == 'zeusmp_x86':
                process = SPEC2k6_ref.zeusmp_x86
            elif options.benchmark == 'leslie3d_x86':
                process = SPEC2k6_ref.leslie3d_x86
            elif options.benchmark == 'GemsFDTD_x86':
                process = SPEC2k6_ref.GemsFDTD_x86
            elif options.benchmark == 'tonto_x86':
                process = SPEC2k6_ref.tonto_x86
            elif options.benchmark == 'namd_x86':
                process = SPEC2k6_ref.namd_x86
            elif options.benchmark == 'dealII_x86':
                process = SPEC2k6_ref.dealII_x86
            elif options.benchmark == 'soplex_x86':
                process = SPEC2k6_ref.soplex_x86
            elif options.benchmark == 'povray_x86':
                process = SPEC2k6_ref.povray_x86
            elif options.benchmark == 'omnetpp_x86':
                process = SPEC2k6_ref.omnetpp_x86
            elif options.benchmark == 'astar_x86':
                process = SPEC2k6_ref.astar_x86
            elif options.benchmark == 'xalancbmk_x86':
                process = SPEC2k6_ref.xalancbmk_x86
            elif options.benchmark == 'gromacs_x86':
                process = SPEC2k6_ref.gromacs_x86
            elif options.benchmark == 'cactusADM_x86':
                process = SPEC2k6_ref.cactusADM_x86
            elif options.benchmark == 'calculix_x86':
                process = SPEC2k6_ref.calculix_x86
            elif options.benchmark == 'wrf_x86':
                process = SPEC2k6_ref.wrf_x86
            else:
                 print "Error: Unknown Benchmark"
                 sys.exit(1)
	else:
	    print "Error: Not supported benchmark size"
	    sys.exit(1)
	
	multiprocesses.append(process)
	return multiprocesses, 1

    idx = 0
    for wrkld in workloads:
        process = LiveProcess()
        process.executable = wrkld

        if len(pargs) > idx:
            process.cmd = [wrkld] + pargs[idx].split()
        else:
            process.cmd = [wrkld]

        if len(inputs) > idx:
            process.input = inputs[idx]
        if len(outputs) > idx:
            process.output = outputs[idx]
        if len(errouts) > idx:
            process.errout = errouts[idx]

        multiprocesses.append(process)
        idx += 1

    if options.smt:
        assert(options.cpu_type == "detailed" or options.cpu_type == "inorder")
        return multiprocesses, idx
    else:
        return multiprocesses, 1


parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)

if '--ruby' in sys.argv:
    Ruby.define_options(parser)

(options, args) = parser.parse_args()

if args:
    print "Error: script doesn't take any positional arguments"
    sys.exit(1)

multiprocesses = []
numThreads = 1

if options.bench:
    apps = options.bench.split("-")
    if len(apps) != options.num_cpus:
        print "number of benchmarks not equal to set num_cpus!"
        sys.exit(1)

    for app in apps:
        try:
            if buildEnv['TARGET_ISA'] == 'alpha':
                exec("workload = %s('alpha', 'tru64', 'ref')" % app)
            else:
                exec("workload = %s(buildEnv['TARGET_ISA'], 'linux', 'ref')" % app)
            multiprocesses.append(workload.makeLiveProcess())
        except:
            print >>sys.stderr, "Unable to find workload for %s: %s" % (buildEnv['TARGET_ISA'], app)
            sys.exit(1)
#Prodromou: Need to add this
elif options.benchmark:
    multiprocesses, numThreads = get_processes(options)
elif options.cmd:
    multiprocesses, numThreads = get_processes(options)
else:
    print >> sys.stderr, "No workload specified. Exiting!\n"
    sys.exit(1)


(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)
CPUClass.numThreads = numThreads

MemClass = Simulation.setMemClass(options)

# Check -- do not allow SMT with multiple CPUs
if options.smt and options.num_cpus > 1:
    fatal("You cannot use SMT with multiple CPUs!")

np = options.num_cpus

#PRODROMOU: Set the instruction window
system = System(cpu = [CPUClass(cpu_id=i) for i in xrange(np)],
                mem_mode = test_mem_mode,
                mem_ranges = [AddrRange(options.mem_size)],
                cache_line_size = options.cacheline_size)

# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage = options.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(clock =  options.sys_clock,
                                   voltage_domain = system.voltage_domain)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(clock = options.cpu_clock,
                                       voltage_domain =
                                       system.cpu_voltage_domain)

# All cpus belong to a common cpu_clk_domain, therefore running at a common
# frequency.
for cpu in system.cpu:
    cpu.clk_domain = system.cpu_clk_domain

# Sanity check
if options.fastmem:
    if CPUClass != AtomicSimpleCPU:
        fatal("Fastmem can only be used with atomic CPU!")
    if (options.caches or options.l2cache):
        fatal("You cannot use fastmem in combination with caches!")

if options.simpoint_profile:
    if not options.fastmem:
        # Atomic CPU checked with fastmem option already
        fatal("SimPoint generation should be done with atomic cpu and fastmem")
    if np > 1:
        fatal("SimPoint generation not supported with more than one CPUs")

for i in xrange(np):
    if options.smt:
        system.cpu[i].workload = multiprocesses
    elif len(multiprocesses) == 1:
        system.cpu[i].workload = multiprocesses[0]
    else:
        system.cpu[i].workload = multiprocesses[i]

    if options.fastmem:
        system.cpu[i].fastmem = True

    if options.simpoint_profile:
        system.cpu[i].simpoint_profile = True
        system.cpu[i].simpoint_interval = options.simpoint_interval

    if options.checker:
        system.cpu[i].addCheckerCpu()

    system.cpu[i].createThreads()

if options.ruby:
    if not (options.cpu_type == "detailed" or options.cpu_type == "timing"):
        print >> sys.stderr, "Ruby requires TimingSimpleCPU or O3CPU!!"
        sys.exit(1)

    # Set the option for physmem so that it is not allocated any space
    system.physmem = MemClass(range=AddrRange(options.mem_size),
                              null = True)

    options.use_map = True
    Ruby.create_system(options, system)
    assert(options.num_cpus == len(system.ruby._cpu_ruby_ports))

    for i in xrange(np):
        ruby_port = system.ruby._cpu_ruby_ports[i]

        # Create the interrupt controller and connect its ports to Ruby
        # Note that the interrupt controller is always present but only
        # in x86 does it have message ports that need to be connected
        system.cpu[i].createInterruptController()

        # Connect the cpu's cache ports to Ruby
        system.cpu[i].icache_port = ruby_port.slave
        system.cpu[i].dcache_port = ruby_port.slave
        if buildEnv['TARGET_ISA'] == 'x86':
            system.cpu[i].interrupts.pio = ruby_port.master
            system.cpu[i].interrupts.int_master = ruby_port.slave
            system.cpu[i].interrupts.int_slave = ruby_port.master
            system.cpu[i].itb.walker.port = ruby_port.slave
            system.cpu[i].dtb.walker.port = ruby_port.slave
else:
    system.membus = CoherentBus()
    system.system_port = system.membus.slave
    CacheConfig.config_cache(options, system)
    MemConfig.config_mem(options, system)

root = Root(full_system = False, system = system)
Simulation.run(options, root, system, FutureClass)
