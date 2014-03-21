#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "debug/Prodromou.hh"
#include "arch/utility.hh"
#include <fstream>

#define SIZE 33

using namespace std;

template<class Impl>
DeadInstAnalyzer<Impl>::DeadInstAnalyzer(O3CPU *cpu_ptr, UINT64 InstWindow)
    : cpu (cpu_ptr),
      STREAM_WINDOW (InstWindow) 
{

    //Initialize Variables
    globalInsCount = 0;
    deadInsCounter=0;
    deadStreamCounter = 0;
    totalInsCounter = 0;

    totalInstructions = 0;
    deadInstructions = 0;
    silentStores = 0;
    silentRegs = 0;
    overStores = 0;
    overRegs = 0;
    
    // Register this object's statistics
    regStats();

    DPRINTF (DeadInstAnalyzer, "Constructor: Created window of size %lld\n", STREAM_WINDOW);

/*
    deadInstructionsList = new long long int [SIZE];
    
    ifstream input;
    input.open("/tmp/sorted_dead.txt");

    for (int i=0; i<SIZE; i++) {
	input >> deadInstructionsList[i];
    }

    for (int i=0; i<SIZE; i++) 
	DPRINTF(DeadInstAnalyzer,"LIST: %lli\n", deadInstructionsList[i]);
*/

    nextDeadIns = 0;
}

template<class Impl>
long long int DeadInstAnalyzer<Impl>::nextDead() {
    //Should be ok for now
    return -1;
    return deadInstructionsList[nextDeadIns];
}

template<class Impl>
void DeadInstAnalyzer<Impl>::deadInstMet() {
    nextDeadIns++;
    nextDeadIns = nextDeadIns % SIZE;
}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyze (DynInstPtr newInst) {
    
    //Get the affected registers
    int numW = newInst->numDestRegs();
    int numR = newInst->numSrcRegs();

    int *WregNames = new int[numW];
    int *RregNames = new int[numR];   
    for (int i=0; i<numW; i++) WregNames[i] = (int)(newInst->destRegIdx(i));
    for (int i=0; i<numR; i++) RregNames[i] = (int)(newInst->srcRegIdx(i));
    //Affected registers (if any) are here

    totalInstructions ++;
    
    //Create new Instruction node
    INS_STRUCT *node = new INS_STRUCT;
    node->ID = newInst->seqNum;
    node->address = newInst->instAddr();
    node->WRegCount = numW;
    node->isMemRef = newInst->isMemRef();
    if (newInst->isStore()) node->WRegCount=1; //Fake One output register for stores
    //Instruction node created 

    // Check for room in the instruction window
    // If there is no room remove oldest instruction node
    // Also update the (virtual) register file
    if (instructions.size() == STREAM_WINDOW) {
        INS_STRUCT *temp_ptr = instructions.front();
        clearRegFile(temp_ptr);
        instructions.pop_front();
        delete(temp_ptr);
    }

    //Print information regarding this instruction
    printNodeInfo(node, newInst, numW, numR, WregNames, RregNames);


// Prodromou: Checking happens here:
    if ((newInst->isLoad()) || (newInst->isStore())) {
	analyzeDeadMemRef (node, newInst);
        checkForSilentStore(node, newInst);
    }
    else {

	analyzeDeadRegOverwrite (node, newInst, numW, numR, WregNames, RregNames);
    }
  
    analyzeRegSameValueOverwrite (node, newInst, numW);
//Prodromou: Checks completed


    // Push the instruction in the stream window
    instructions.push_back(node);

    //Memory Management
    delete[] WregNames;
    delete[] RregNames;
}


/*
When an instruction is removed from the stream window, 
I need to update the (virtual) reg file in order to 
remove any trace of that instruction
*/
template<class Impl>
void DeadInstAnalyzer<Impl>::clearRegFile(INS_STRUCT *instruction) {
    long long int id = instruction->ID;

    for (typename map<long,INS_STRUCT*>::iterator it=regFile.begin(); it!=regFile.end(); ++it) {
        if (it->second == NULL) continue;
        if (it->second->ID == id) {
            if (!(it->second->isMemRef)) it->second = NULL;
	    else {
		regFile.erase(it);
	    }
        }
    }
}

template<class Impl>
bool DeadInstAnalyzer<Impl>::checkDeadness (INS_STRUCT *instruction) {
    long long int currentHead = (*(instructions.begin()))->ID;
    if (instruction->ID < currentHead) return false;

    if ( (instruction->readCounter == 0) &&
         (instruction->OWCount == instruction->WRegCount) )
    {
        //Instruction is Dead
	if (instruction->isMemRef) overStores++;
	else overRegs++;
	
	declareDead(instruction);
        return true;
    }
    return false;
}

template<class Impl>
void DeadInstAnalyzer<Impl>::declareDead (INS_STRUCT *instruction) {
    DPRINTF(DeadInstAnalyzer, "Instruction Dead: %lld\n", instruction->ID);

    deadInsCounter ++;

    //For Simulator's statistics
    ++deadInstructions;

    long long int currentHead = (*(instructions.begin()))->ID;
    //DECREASE THE COUNTER IN ALL RAWs
    for (typename deque<INS_STRUCT*>::iterator it=(instruction->RAW).begin(); it != (instruction->RAW).end(); ++it) {
        if ((*it)->ID < currentHead) continue;
        else {
            (*it)->readCounter --;
            checkDeadness (*it);
        }
    }
}

template<class Impl>
string DeadInstAnalyzer<Impl>::name() const
{
    return "DeadInstAnalyzer";
}

template<class Impl>
void DeadInstAnalyzer<Impl>::regStats() 
{
    totalInstructions
	.name(name() + ".TotalInstructions")
	.desc("Number of instructions analyzed");

    deadInstructions
	.name(name() + ".DeadInstructions")
	.desc("Number of dead instructions");

    silentStores
	.name(name()+".silentStores")
	.desc("Number of silent store instructions");

    overStores
	.name(name()+".overStores")
	.desc("Number of store instructions that overwrite mem. address without anybody reading it since the last time it was written");

    overRegs
	.name(name()+".overRegs")
	.desc("Number of instructions that overwrite register without anybody reading it since the last time it was written");

    silentRegs
	.name(name()+".silentRegs")
	.desc("Number of register writes with the same value as the previous write");

    
}


template<class Impl>
void DeadInstAnalyzer<Impl>::checkForSilentStore (INS_STRUCT *node, DynInstPtr newInst) {
 
    DPRINTF (DeadInstAnalyzer, "Analyzing instruction [sn:%lld] for silent store\n", newInst->seqNum);

    // Prodromou: Get the information
	Addr effAddr = newInst->physEffAddr;
	int size = newInst->effSize;   
	uint64_t *temp = new uint64_t; 
	Request::Flags flags;
    //Required information found

    //Create a request and a packet to send
	//Always ask for size 8 and mask later
	Request *req = new Request(effAddr, 8, flags, newInst->masterId());
	PacketPtr data_pkt = new Packet(req, MemCmd::ReadReq);
	data_pkt->dataStatic(temp);
    //Request and packet created

    //send the new packet to memory 
	cpu->getDataPort().sendFunctional(data_pkt);
    //Packet sent. Functional send means that the information is here now

    // Mask the memory result and get all necessary info
	uint64_t mask = pow(2,size*4)-1; // *4 to convert size in bits (YES it's not 8 it's 4)
	uint64_t *mem_data = data_pkt->getPtr<uint64_t>();
	uint64_t useful_data = *(mem_data) & mask;
	uint64_t reg_data = -1234;
	int reg_id = (int)(newInst->srcRegIdx(0));

	//Read the register's value (need to differentiate between ints and floats
	    if ( reg_id < TheISA::NumIntRegs) {
		reg_data = cpu->readArchIntReg(reg_id, newInst->threadNumber);
	    }
	    else { //Float
		reg_data = cpu->readArchFloatRegInt(reg_id, newInst->threadNumber);
	    }

	//reg_data was initialized with -1234. This is a hack and in case
	//-1234 is the actual memory value it will crash. Then I will read 
	//this comment and understand what happened.
	assert (reg_data != -1234); //This could actually happen within a program

	uint64_t reg_useful_data = reg_data & mask;
	DPRINTF (DeadInstAnalyzer, "[sn:%lld] Functional Access returned: address:%#x size:%d mem_data:%#x useful_data:%#x reg_data:%#x reg_useful_data:%lf\n", newInst->seqNum, req->getPaddr(), size, mem_data, useful_data, reg_data, reg_useful_data);
    //Everything we need for the check is here now.
	
    if (useful_data == reg_useful_data) {
	DPRINTF (DeadInstAnalyzer, "[sn:%lli] Silent store. Instruction Dead\n", newInst->seqNum);
	silentStores++;
	declareDead(node);
    }
    
    delete[] temp;
}

template<class Impl>
void DeadInstAnalyzer<Impl>::printNodeInfo (INS_STRUCT *node, 
					    DynInstPtr newInst,
					    int numW, int numR,
					    int *WregNames,
					    int *RregNames) {

    //Print information regarding this instruction
    DPRINTF(Prodromou, "New Node: %lld\n", node->ID);
    string s;
    newInst->dump(s);
    DPRINTF(Prodromou, "Instruction: %s\n", s);
    std::ostringstream s1;
    std::ostringstream s2;
    for (int i=0; i<numW; i++) s1<<WregNames[i]<<" ";
    for (int i=0; i<numR; i++) s2<<RregNames[i]<<" ";
    
    DPRINTF(Prodromou, "Writes to: %s\n", s1.str());
    DPRINTF(Prodromou, "Reads from: %s\n", s2.str());
    DPRINTF(Prodromou, "1: %d 2: %d 3: %d 4: %d 5: %d\n", newInst->isMemRef(), newInst->isLoad(), newInst->isStore(), newInst->isStoreConditional(), newInst->doneEACalc());
    DPRINTF(Prodromou, "Addr: %#x, Eff. Addr: %#x, Phys. addr: %#x\n", newInst->instAddr(), newInst->effAddr, newInst->physEffAddr);
}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyzeDeadMemRef (INS_STRUCT *node, DynInstPtr newInst) {
    // Reading Memory References => Load Instructions
    // Handling memory address as a register.
    if (newInst->isLoad()) {
        long regName = newInst->effAddr;
        DPRINTF(Prodromou, "Load Instruction. Reading From: %#08d\n", regName);
        INS_STRUCT *conflictingIns = regFile[regName];
        if (conflictingIns != NULL) {
            node->RAW.push_back(conflictingIns);
            conflictingIns->readCounter++;
        }
    }
    // Writing Memory References => Store Instructions
    else if (newInst->isStore()) {
        // Use the address of the store/load and use that as a register's name
        long regName = newInst->effAddr;
        INS_STRUCT *conflictingIns = regFile[regName];
        if (conflictingIns != NULL) {
            node->WAW.push_back(conflictingIns);
            conflictingIns->OWCount ++;

            //Marks the end of a dead code stream
            if (checkDeadness(conflictingIns)) {
                deadStreamCounter ++;
            }
        }
        regFile[regName] = node;

    }
}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyzeDeadRegOverwrite (INS_STRUCT *node, 
						      DynInstPtr newInst,
						      int numW, int numR,
						      int *WregNames,
						      int *RregNames) {
    for (int i=0; i<numR; i++) {
	int regName = RregNames[i];

	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    node->RAW.push_back(conflictingIns);
	    conflictingIns->readCounter++;
	}
    }
    for (int i=0; i<numW; i++) {
	int regName = WregNames[i];

	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    node->WAW.push_back(conflictingIns);
	    conflictingIns->OWCount ++;

	    //Marks the end of a dead code stream
	    if (checkDeadness(conflictingIns)) {
		deadStreamCounter ++;
	    }
	}
	regFile[regName] = node;
    }
}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyzeRegSameValueOverwrite (INS_STRUCT *node, DynInstPtr newInst, int numW){

    int reg_id = (int)(newInst->destRegIdx(0));
    uint64_t res=-1234;
    double res_dbl=-1234;
    uint64_t reg_data;

    //Prodromou: Verified: readArchIntReg reads the 
    //Prodromou: RENAMED register, readIntReg reads 
    //Prodromou: the ISA register. Same applies for Float regs
    //Prodromou: readFloatRegInt returns the float number
    //Prodromou: in a uint64_t version (Just interpreting bits)
    if (numW > 0) {
        if (reg_id < TheISA::NumIntRegs) {
            newInst->readResult(res);
            reg_data = cpu->readIntReg(reg_id);
            DPRINTF(Prodromou, "Reading int reg %d and checking for deadness\n", reg_id);
	    DPRINTF(Prodromou, "[sn:%lli] Current Instruction Result:%#x, reg_data:%#x\n", newInst->seqNum, res, reg_data);
	    if (res == reg_data) {
		DPRINTF (Prodromou, "[sn:%lli] Silent register write (int). Instruction Dead\n", newInst->seqNum);
		silentRegs++;
		declareDead(node);
	    }
        }
        else if (reg_id < TheISA::NumIntRegs + TheISA::NumFloatRegs) {
            reg_data = cpu->readFloatRegBits(reg_id+cpu->numPhysIntRegs); //Need to offset the reg_id
            newInst->readResult(res_dbl);
            DPRINTF(Prodromou, "Reading float reg %d and checking for deadness\n", reg_id);
	    DPRINTF(Prodromou, "[sn:%lli] Current Instruction Float Result:%#x, reg_data:%#x\n", newInst->seqNum, res_dbl, reg_data);
	    if (res_dbl == reg_data) {
		DPRINTF (Prodromou, "[sn:%lli] Silent register write (float). Instruction Dead\n", newInst->seqNum);
		silentRegs++;
		declareDead(node);
	    }
        }
        else 
            DPRINTF (Prodromou, "Some sort of special register\n");
    }
}

