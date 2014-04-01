#SPEC2k6_ref.py

#For some reason this imports the LiveProcess object
from Caches import *

#bzip2 "input.combined 200";
#gcc "scilab.i -o scilab.s";
#gobmk "--quiet --mode gtp" -i "13x13.tst";
#h264ref "-d foreman_ref_encoder_baseline.cfg";
#hmmer "nph3.hmm swiss41";
#lbm "3000 reference.dat 0 0 100_100_130_ldc.of";
#libquantum "1397 8";
#mcf "inp.in";
#milc < su3imp.in
#perlbench "-I./lib diffmail.pl 4 800 10 17 19 300";
#sjeng "ref.txt";
#sphinx3 "ctlfile . args.an4";

#400.perlbench
perlbench = LiveProcess()
perlbench.executable =  './perlbench_base.alpha'
perlbench.cmd = [perlbench.executable] + ['-I./lib', 'diffmail.pl', '4', '800', '10', '17', '19', '300']

bzip2 = LiveProcess()
bzip2.executable =  './bzip2_base.alpha'
bzip2.cmd = [bzip2.executable] + ['input.combined', '200']

gcc = LiveProcess()
gcc.executable =  './gcc_base.alpha'
gcc.cmd = [gcc.executable] + ['scilab.i', '-o', 'scilab.s']

mcf = LiveProcess()
mcf.executable =  './mcf_base.alpha'
mcf.cmd = [mcf.executable] + ['inp.in']

milc=LiveProcess()
milc.executable = './milc_base.alpha'
stdin='su3imp.in'
milc.cmd = [milc.executable]
milc.input=stdin

gobmk=LiveProcess()
gobmk.executable = './gobmk_base.alpha'
stdin='13x13.tst'
gobmk.cmd = [gobmk.executable] + ['--quiet', '--mode', 'gtp']
gobmk.input=stdin

hmmer = LiveProcess()
hmmer.executable =  './hmmer_base.alpha'
hmmer.cmd = [hmmer.executable] + ['nph3.hmm', 'swiss41']

sjeng = LiveProcess()
sjeng.executable =  './sjeng_base.alpha'
sjeng.cmd = [sjeng.executable] + ['ref.txt']

libquantum = LiveProcess()
libquantum.executable =  './libquantum_base.alpha'
libquantum.cmd = [libquantum.executable] + ['1397', '8']

h264ref = LiveProcess()
h264ref.executable =  './h264ref_base.alpha'
h264ref.cmd = [h264ref.executable] + ['-d', 'foreman_ref_encoder_baseline.cfg']

lbm = LiveProcess()
lbm.executable =  './lbm_base.alpha'
lbm.cmd = [lbm.executable] + ['3000', 'reference.dat', '0', '0', '100_100_130_ldc.of']

sphinx3 = LiveProcess()
sphinx3.executable =  './sphinx_livepretend_base.alpha'
sphinx3.cmd = [sphinx3.executable] + ['ctlfile', '.', 'args.an4']

specrand = LiveProcess()
specrand.executable =  './specrand_base.alpha'
specrand.cmd = [specrand.executable] + ['1255432124', '234923']
