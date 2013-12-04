#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "arch/utility.hh"
using namespace std;

DeadInstAnalyzer::DeadInstAnalyzer() {
    test_int = 13;
    DPRINTF (DeadInstAnalyzer, "Object Instantiated with test int %d\n", test_int);
}
