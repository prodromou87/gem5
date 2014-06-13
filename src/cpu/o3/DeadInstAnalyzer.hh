#ifndef __DEAD_INST_ANALYZER_HH__
#define __DEAD_INST_ANALYZER_HH__

#include <deque>
#include <map>
#include <utility>
#include <string>
#include "base/statistics.hh"
#include "mem/request.hh"
#include "mem/physical.hh"
#include "mem/packet.hh"
#include "config/the_isa.hh"
#include "arch/utility.hh"
using namespace std;

struct DerivO3CPUParams;

template<class Impl>
class DeadInstAnalyzer {
    private:

	typedef typename Impl::O3CPU O3CPU;
	typedef typename Impl::DynInstPtr DynInstPtr;
	O3CPU *cpu;
	
	typedef long long int UINT64;
	
	struct INS_STRUCT {
	    INS_STRUCT() : ID(0), readCounter(0), OWCount(0) {};
	    deque <pair<UINT64, INS_STRUCT*> > RAW; // Need to store the ID as well as the INS_STRUCT node itself 
						    // so I can check for the memory aliasing bug during execution
	    deque <INS_STRUCT*> WAW;

	    long long int ID;
	    int WRegCount; //Number of output registers for each command
	    int readCounter; // Number of times the output registers were read by other instructions
	    int OWCount; // Note: Each output register (Wreg) can be overwritten by at most one instruction
	    long long int address;
	    bool isMemRef; //Set to true if the instruction is Memory Reference (Load/Store)
	    bool isLoad;
	    int *Wregs;
	    UINT64 effAddr;
	};

	map <long, INS_STRUCT*> regFile; // Holds the last writer instruction for each reg
	deque <INS_STRUCT*> instructions; // The instruction window

	UINT64 globalInsCount;
	UINT64 deadInsCounter, deadStreamCounter, totalInsCounter;

	bool checkDeadness (INS_STRUCT *instruction);
	void declareDead (INS_STRUCT *instruction);
	void clearRegFile (INS_STRUCT *instruction);
	void printNodeInfo (INS_STRUCT *node, 
			    DynInstPtr newInst,
			    int numW, int numR,
			    int *WregNames,
			    int * Rregnames);

	void analyzeDeadMemRef (INS_STRUCT *node, DynInstPtr newInst);
	void analyzeDeadRegOverwrite (INS_STRUCT *node, 
				      DynInstPtr newInst,
				      int numW, int numR,
				      int *WregNames,
				      int *RregNames);

	void analyzeRegSameValueOverwrite (INS_STRUCT *node, DynInstPtr newInst, int numW);
	void checkForSilentStore (INS_STRUCT *node, DynInstPtr newInst);

	UINT64 STREAM_WINDOW;
	int op_type;

	//Functions needed to create new entries in 
	//the list of simulation statistics
	string name() const;
	void regStats();

	//Declare the new statistics variables and their type 
	//from the Stats namespace. Type descriptions exist in 
	//src/base/stats/types.hh and online at www.m5sim.org/Statistics
	Stats::Scalar totalInstructions;
	Stats::Scalar deadInstructions;
	Stats::Scalar silentStores;
	Stats::Scalar silentRegs;
	Stats::Scalar overStores;
	Stats::Scalar overRegs;

	Stats::Scalar fromOverReg;
	Stats::Scalar fromOverStore;
	Stats::Scalar fromSilentReg;
	Stats::Scalar fromSilentStore;

	int nextDeadIns;
	
	//Prodromou: List to hold all dead Instructions
	long long int *deadInstructionsList;

	bool recursiveLoadOrigins (INS_STRUCT *node);
        int loadOrigins;
	UINT64 totalStores;
	map <int, int> loadOriginsMap;
	map <int, int> deadLoadOriginsMap;
	void printLoadOrigins();

	//bool trigger;
	int recursiveCalls;

    public:
	DeadInstAnalyzer(O3CPU *cpu_ptr, DerivO3CPUParams *params);

	//The connecting function between the Analyzer and the 
	//commit stage. Commited instructions are sent to this 
	//function for deadness analyzis
	void analyze(DynInstPtr newInst);
	
	long long int nextDead();
	void deadInstMet();

};
#endif // __DEAD_INST_ANALYZER_HH__
