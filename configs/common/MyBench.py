#Mybench.py

#For some reason this imports the LiveProcess object
from Caches import *

#400.perlbench
perlbench = LiveProcess()
perlbench.executable =  './perlbench_base.alpha'
perlbench.cmd = [perlbench.executable] + ['-I.', '-I./lib', 'suns.pl']

bzip2 = LiveProcess()
bzip2.executable =  './bzip2_base.alpha'
bzip2.cmd = [bzip2.executable] + ['byoudoin.jpg', '5']

gcc = LiveProcess()
gcc.executable =  './gcc_base.alpha'
gcc.cmd = [gcc.executable] + ['integrate.i' ,'-o integrate.s']

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
stdin='atari_atari.tst'
gobmk.cmd = [gobmk.executable]
gobmk.input=stdin

hmmer = LiveProcess()
hmmer.executable =  './hmmer_base.alpha'
hmmer.cmd = [hmmer.executable] + ['--fixed', '0', '--mean', '425', '--num', '85000', '--sd', '300', '--seed', '0', 'leng100.hmm']

sjeng = LiveProcess()
sjeng.executable =  './sjeng_base.alpha'
sjeng.cmd = [sjeng.executable] + ['train.txt']

libquantum = LiveProcess()
libquantum.executable =  './libquantum_base.alpha'
libquantum.cmd = [libquantum.executable] + ['143', '25']

h264ref = LiveProcess()
h264ref.executable =  './h264ref_base.alpha'
h264ref.cmd = [h264ref.executable] + ['-d', 'foreman_train_encoder_baseline.cfg']

lbm = LiveProcess()
lbm.executable =  './lbm_base.alpha'
lbm.cmd = [lbm.executable] + ['300', 'reference.dat', '0', '1', '100_100_130_cf_b.of']

sphinx3 = LiveProcess()
sphinx3.executable =  './sphinx_livepretend_base.alpha'
sphinx3.cmd = [sphinx3.executable] + ['ctlfile', '.', 'args.an4']

specrand = LiveProcess()
specrand.executable =  './specrand_base.alpha'
specrand.cmd = [specrand.executable] + ['1', '3']
