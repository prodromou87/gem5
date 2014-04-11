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

# FORTRAN BENCHMARKS
bwaves = LiveProcess()
bwaves.executable = './bwaves_base.alpha'
bwaves.cmd = [bwaves.executable]

gamess=LiveProcess()
gamess.executable = './gamess_base.alpha'
stdin='triazolium.config'
gamess.cmd = [gamess.executable]
gamess.input=stdin

zeusmp = LiveProcess()
zeusmp.executable = './zeusmp_base.alpha'
zeusmp.cmd = [zeusmp.executable]

leslie3d=LiveProcess()
leslie3d.executable = './leslie3d_base.alpha'
stdin='leslie3d.in'
leslie3d.cmd = [gamess.executable]
leslie3d.input=stdin

GemsFDTD = LiveProcess()
GemsFDTD.executable = './GemsFDTD_base.alpha'
GemsFDTD.cmd = [GemsFDTD.executable]

tonto = LiveProcess()
tonto.executable = './tonto_base.alpha'
tonto.cmd = [tonto.executable]

# CPP BENCHMARKS
namd = LiveProcess()
namd.executable =  './namd_base.alpha'
namd.cmd = [namd.executable] + ['--input', 'namd.input', '--iterations', '38', '--output', 'namd.out']

dealII = LiveProcess()
dealII.executable =  './dealII_base.alpha'
dealII.cmd = [dealII.executable] + ['23']

soplex = LiveProcess()
soplex.executable =  './soplex_base.alpha'
soplex.cmd = [soplex.executable] + ['-s1', '-e', '-m45000', 'pds-50.mps']

povray = LiveProcess()
povray.executable =  './povray_base.alpha'
povray.cmd = [povray.executable] + ['SPEC-benchmark-ref.ini']

omnetpp = LiveProcess()
omnetpp.executable =  './omnetpp_base.alpha'
omnetpp.cmd = [omnetpp.executable] + ['omnetpp.ini']

astar = LiveProcess()
astar.executable =  './astar_base.alpha'
astar.cmd = [astar.executable] + ['BigLakes2048.cfg']

xalancbmk = LiveProcess()
xalancbmk.executable =  './Xalan_base.alpha'
xalancbmk.cmd = [xalancbmk.executable] + ['-v', 't5.xml', 'xalanc.xsl']

# MIXED BENCHMARKS
gromacs = LiveProcess()
gromacs.executable =  './gromacs_base.alpha'
gromacs.cmd = [gromacs.executable] + ['-silent', '-deffnm', 'gromacs', '-nice', '0']

cactusADM = LiveProcess()
cactusADM.executable =  './cactusADM_base.alpha'
cactusADM.cmd = [cactusADM.executable] + ['benchADM.par']

calculix = LiveProcess()
calculix.executable =  './calculix_base.alpha'
calculix.cmd = [calculix.executable] + ['-i', 'hyperviscoplastic']

wrf = LiveProcess()
wrf.executable = './wrf_base.alpha'
wrf.cmd = [wrf.executable]

















