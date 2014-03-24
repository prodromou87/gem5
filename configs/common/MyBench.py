#Mybench.py

#For some reason this imports the LiveProcess object
from Caches import *

#400.perlbench
perlbench = LiveProcess()
perlbench.executable =  '/home/prodromou/Desktop/SPEC_2006/benchspec/CPU2006/400.perlbench/run/run_base_test_alpha.0000/perlbench_base.alpha'
perlbench.cmd = [perlbench.executable] + ['-I/home/prodromou/Desktop/SPEC_2006/benchspec/CPU2006/400.perlbench/run/run_base_test_alpha.0000/lib', '/home/prodromou/Desktop/SPEC_2006/benchspec/CPU2006/400.perlbench/run/run_base_test_alpha.0000/diffmail.pl', '2 550 15 24 23 100']
perlbench.output = '/tmp/diffmail.2.550.15.24.23.100.out'
perlbench.errout = '/tmp/diffmail.2.550.15.24.23.100.err'
