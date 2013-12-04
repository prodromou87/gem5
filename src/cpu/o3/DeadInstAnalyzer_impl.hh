#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "arch/utility.hh"
using namespace std;

template<class Impl>
DeadInstAnalyzer<Impl>::DeadInstAnalyzer() {}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyze (DynInstPtr newInst) {
    string s;
    newInst->dump(s);
    DPRINTF (DeadInstAnalyzer, "Inst: %s\n", s);
}
