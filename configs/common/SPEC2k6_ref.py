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

#X86 benchmarks
perlbench_x86 = LiveProcess()
perlbench_x86.executable =  './perlbench_base.X86_64'
perlbench_x86.cmd = [perlbench_x86.executable] + ['-I./lib', 'diffmail.pl', '4', '800', '10', '17', '19', '300']

bzip2_x86 = LiveProcess()
bzip2_x86.executable =  './bzip2_base.X86_64'
bzip2_x86.cmd = [bzip2_x86.executable] + ['input.combined', '200']

gcc_x86 = LiveProcess()
gcc_x86.executable =  './gcc_base.X86_64'
gcc_x86.cmd = [gcc_x86.executable] + ['scilab.i', '-o', 'scilab.s']

mcf_x86 = LiveProcess()
mcf_x86.executable =  './mcf_base.X86_64'
mcf_x86.cmd = [mcf_x86.executable] + ['inp.in']

milc_x86 = LiveProcess()
milc_x86.executable = './milc_base.X86_64'
milc_x86.cmd = [milc_x86.executable]
milc_x86.input='su3imp.in'

gobmk_x86 = LiveProcess()
gobmk_x86.executable = './gobmk_base.X86_64'
stdin='13x13.tst'
gobmk_x86.cmd = [gobmk_x86.executable] + ['--quiet', '--mode', 'gtp']
gobmk.input=stdin

hmmer_x86 = LiveProcess()
hmmer_x86.executable =  './hmmer_base.X86_64'
hmmer_x86.cmd = [hmmer_x86.executable] + ['nph3.hmm', 'swiss41']

sjeng_x86 = LiveProcess()
sjeng_x86.executable =  './sjeng_base.X86_64'
sjeng_x86.cmd = [sjeng_x86.executable] + ['ref.txt']

libquantum_x86 = LiveProcess()
libquantum_x86.executable =  './libquantum_base.X86_64'
libquantum_x86.cmd = [libquantum_x86.executable] + ['1397', '8']

h264ref_x86 = LiveProcess()
h264ref_x86.executable =  './h264ref_base.X86_64'
h264ref_x86.cmd = [h264ref_x86.executable] + ['-d', 'foreman_ref_encoder_baseline.cfg']

lbm_x86 = LiveProcess()
lbm_x86.executable =  './lbm_base.X86_64'
lbm_x86.cmd = [lbm_x86.executable] + ['3000', 'reference.dat', '0', '0', '100_100_130_ldc.of']

sphinx3_x86 = LiveProcess()
sphinx3_x86.executable =  './sphinx_livepretend_base.X86_64'
sphinx3_x86.cmd = [sphinx3_x86.executable] + ['ctlfile', '.', 'args.an4']

specrand_x86 = LiveProcess()
specrand_x86.executable =  './specrand_base.X86_64'
specrand_x86.cmd = [specrand_x86.executable] + ['1255432124', '234923']

# FORTRAN BENCHMARKS
bwaves_x86 = LiveProcess()
bwaves_x86.executable = './bwaves_base.X86_64'
bwaves_x86.cmd = [bwaves_x86.executable]

gamess_x86 = LiveProcess()
gamess_x86.executable = './gamess_base.X86_64'
stdin='triazolium.config'
gamess_x86.cmd = [gamess_x86.executable]
gamess.input=stdin

zeusmp_x86 = LiveProcess()
zeusmp_x86.executable = './zeusmp_base.X86_64'
zeusmp_x86.cmd = [zeusmp_x86.executable]

leslie3d_x86 = LiveProcess()
leslie3d_x86.executable = './leslie3d_base.X86_64'
stdin='leslie3d.in'
leslie3d_x86.cmd = [gamess_x86.executable]
leslie3d.input=stdin

GemsFDTD_x86 = LiveProcess()
GemsFDTD_x86.executable = './GemsFDTD_base.X86_64'
GemsFDTD_x86.cmd = [GemsFDTD_x86.executable]

tonto_x86 = LiveProcess()
tonto_x86.executable = './tonto_base.X86_64'
tonto_x86.cmd = [tonto_x86.executable]

# CPP BENCHMARKS
namd_x86 = LiveProcess()
namd_x86.executable =  './namd_base.X86_64'
namd_x86.cmd = [namd_x86.executable] + ['--input', 'namd.input', '--iterations', '38', '--output', 'namd.out']

dealII_x86 = LiveProcess()
dealII_x86.executable =  './dealII_base.X86_64'
dealII_x86.cmd = [dealII_x86.executable] + ['23']

soplex_x86 = LiveProcess()
soplex_x86.executable =  './soplex_base.X86_64'
soplex_x86.cmd = [soplex_x86.executable] + ['-s1', '-e', '-m45000', 'pds-50.mps']

povray_x86 = LiveProcess()
povray_x86.executable =  './povray_base.X86_64'
povray_x86.cmd = [povray_x86.executable] + ['SPEC-benchmark-ref.ini']

omnetpp_x86 = LiveProcess()
omnetpp_x86.executable =  './omnetpp_base.X86_64'
omnetpp_x86.cmd = [omnetpp_x86.executable] + ['omnetpp.ini']

astar_x86 = LiveProcess()
astar_x86.executable =  './astar_base.X86_64'
astar_x86.cmd = [astar_x86.executable] + ['BigLakes2048.cfg']

xalancbmk_x86 = LiveProcess()
xalancbmk_x86.executable =  './Xalan_base.X86_64'
xalancbmk_x86.cmd = [xalancbmk_x86.executable] + ['-v', 't5.xml', 'xalanc.xsl']

# MIXED BENCHMARKS
gromacs_x86 = LiveProcess()
gromacs_x86.executable =  './gromacs_base.X86_64'
gromacs_x86.cmd = [gromacs_x86.executable] + ['-silent', '-deffnm', 'gromacs', '-nice', '0']

cactusADM_x86 = LiveProcess()
cactusADM_x86.executable =  './cactusADM_base.X86_64'
cactusADM_x86.cmd = [cactusADM_x86.executable] + ['benchADM.par']

calculix_x86 = LiveProcess()
calculix_x86.executable =  './calculix_base.X86_64'
calculix_x86.cmd = [calculix_x86.executable] + ['-i', 'hyperviscoplastic']

wrf_x86 = LiveProcess()
wrf_x86.executable = './wrf_base.X86_64'
wrf_x86.cmd = [wrf_x86.executable]















