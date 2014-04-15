#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "debug/Prodromou.hh"
#include "arch/utility.hh"
#include "params/DerivO3CPU.hh"
#include <fstream>
#include "base/statistics.hh"
#include <cstdio>
#include <cstdlib>

#define SIZE 33

using namespace std;

template<class Impl>
DeadInstAnalyzer<Impl>::DeadInstAnalyzer(O3CPU *cpu_ptr, DerivO3CPUParams *params)
    : cpu (cpu_ptr),
      STREAM_WINDOW (params->InstWindow), 
      op_type (params->OpType)
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

    fromOverReg = 0;
    fromOverStore = 0;
    fromSilentReg = 0;
    fromSilentStore = 0;

    // Register this object's statistics
    regStats();

	cout<<"DIA Initialized with instruction window size of "<<STREAM_WINDOW<<endl;

    DPRINTF (DeadInstAnalyzer, "Constructor: Created window of size %lld. Operation Type: %d\n", STREAM_WINDOW, op_type);

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

    //Create new Instruction node
    INS_STRUCT *node = new INS_STRUCT;
    node->ID = newInst->seqNum;
    node->address = newInst->instAddr();
    node->WRegCount = numW;
    node->isMemRef = newInst->isStore();
    node->Wregs = new int[numW];
    if (node->isMemRef) node->effAddr = newInst->effAddr;
    else node->effAddr = -1;
   

    int *WregNames = new int[numW];
    int *RregNames = new int[numR];   
    for (int i=0; i<numW; i++) {
	WregNames[i] = (int)(newInst->destRegIdx(i));
	node->Wregs[i] = WregNames[i];
    }
    for (int i=0; i<numR; i++) RregNames[i] = (int)(newInst->srcRegIdx(i));
    //Affected registers (if any) are here

    totalInstructions ++;
    

    //if (newInst->isStore()) node->WRegCount=1; //Fake One output register for stores -- UPDATE: Why did I do that?
    //Instruction node created 

    // Check for room in the instruction window
    // If there is no room remove oldest instruction node
    // Also update the (virtual) register file
    if (instructions.size() == STREAM_WINDOW) {
        INS_STRUCT *temp_ptr = instructions.front();
        //clearRegFile(temp_ptr); //NOTE This adds a significant overhead. Also it's not necessary since the deadness checks verify that an instruction is still in the window before they proceed. 
        instructions.pop_front();
        delete(temp_ptr);
    }

    //Print information regarding this instruction
    printNodeInfo(node, newInst, numW, numR, WregNames, RregNames);


// Prodromou: Checking happens here:

/*
    NOTE: op_type defines the checks
	op_type = 1 => Only OverReg check
	op_type = 2 => + OverStores check
	op_type = 3 => + SilentStores check
	op_type = 4 => + SilentRegs check (all checkers active)
*/

    //Check for overwrites
    //OverReg chec -- ALL INSTRUCTIONS MUST DO THIS
    UINT64 t = deadInstructions.value();
    analyzeDeadRegOverwrite (node,newInst,numW,numR,WregNames,RregNames);
    fromOverReg += (deadInstructions.value() - t);
    
    if ((newInst->isLoad()) || (newInst->isStore())) {
	//OverMem check
	UINT64 t = deadInstructions.value();
	analyzeDeadMemRef (node, newInst);
	fromOverStore += (deadInstructions.value() - t);
    }

    //Check for silence
    if (newInst->isStore()) {
	//SilentStore check
	UINT64 t = deadInstructions.value();
        checkForSilentStore(node, newInst);
	fromSilentStore += (deadInstructions.value() - t);
    }
    else {
	//SilentReg check
	UINT64 t = deadInstructions.value();
	analyzeRegSameValueOverwrite (node, newInst, numW);
	fromSilentReg += (deadInstructions.value() - t);
    }

    //cout<<"S: "<<regFile.size()<<endl;

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
    
    if (instruction->isMemRef) {
	long regName = instruction->effAddr;
	INS_STRUCT *temp = regFile[regName];
	//temp CANNOT BE NULL -- but regFile might be pointing to other instruction
	if ((temp != NULL) && (temp->ID == id)) regFile[regName] = NULL;
    }
    else {
	for (int i=0; i<instruction->WRegCount; i++) {
	    long regName = instruction->Wregs[i];
	    INS_STRUCT *temp = regFile[regName];
	    //temp CANNOT BE NULL -- but regFile might be pointing to other instruction
	    if (temp->ID == id) regFile[regName] = NULL;
	}
    }

/*
    for (typename map<long,INS_STRUCT*>::iterator it=regFile.begin(); it!=regFile.end(); ++it) {
        if (it->second == NULL) continue;
        if (it->second->ID == id) {
            if (!(it->second->isMemRef)) it->second = NULL;
	    else {
		regFile.erase(it); //NOTE: VERIFY CORRECTNESS
	    }
        }
    }
*/
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
    /*
    NOTE: if instruction's read count is not 0 but it's overwritten, 
    is there a chance that the overwrite was silent (Could this 
    happen when this is a recursive call -- backlog?) If it's possible, 
    we are missing some opportunity here
    */
    return false;
}

template<class Impl>
void DeadInstAnalyzer<Impl>::declareDead (INS_STRUCT *instruction) {
    DPRINTF(DeadInstAnalyzer, "Instruction Dead: %lld\n", instruction->ID);

    deadInsCounter ++;

    //For Simulator's statistics
    ++deadInstructions;

    long long int currentHead = (*(instructions.begin()))->ID;

    //IMPORTANT: Before starting reducing the instruction counts I should update all the WAW list overwrite counts. Otherwise it might lead to errors. (Instruction thinks it's dead, but the instruction that overwritten it is dead already). 
    // FIXED: There is no need to do this IF THERE IS ONLY ONE output register. Otherwise I need to take care of it. REASON: The instruction that triggered backlog is dead, thus overwritten. Consequently all WAW instructions are still overwritten.

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

    fromOverReg
        .name(name()+".fromOverReg")
        .desc("Number of dead instructions where backlog was triggered by OverReg");

    fromOverStore
        .name(name()+".fromOverStore")
        .desc("Number of dead instructions where backlog was triggered by OverStore");

    fromSilentReg
        .name(name()+".fromSilentReg")
        .desc("Number of dead instructions where backlog was triggered by SilentReg");

    fromSilentStore
        .name(name()+".fromSilentStore")
        .desc("Number of dead instructions where backlog was triggered by SilentStore");
}


template<class Impl>
void DeadInstAnalyzer<Impl>::checkForSilentStore (INS_STRUCT *node, DynInstPtr newInst) {

     //check operation type and return if checker is not activated
    if (op_type < 3) {
        return;
    }
 
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

    //check operation type and return if checker is not activated
    if (op_type < 2) {
        return;
    }

    // Reading Memory References => Load Instructions
    // Handling memory address as a register.
    if (newInst->isLoad()) {
	//Option 0
	long regName = newInst->effAddr; //Adds a TON of overhead. I don't know why
	//Option 2
	//string regName = static_cast<ostringstream*>( &(ostringstream() << newInst->effAddr) )->str();
	//cout<<regName<<endl;

        DPRINTF(Prodromou, "Load Instruction. Reading From: %#08s\n", regName);
        INS_STRUCT *conflictingIns = regFile[regName];
        if (conflictingIns != NULL) {
            node->RAW.push_back(conflictingIns);
            conflictingIns->readCounter++;
        }
    }
    // Writing Memory References => Store Instructions
    else if (newInst->isStore()) {
        // Use the address of the store/load and use that as a register's name
	//Option 0
	long regName = newInst->effAddr; //Adds a TON of overhead. I don't know why
        //Option 2
        //regName = static_cast<ostringstream*>( &(ostringstream() << newInst->effAddr) )->str();
	//cout<<regName<<endl;
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

    //check operation type and return if checker is not activated
    if (op_type < 1) {
	return; 
    }

    for (int i=0; i<numR; i++) {
	//Option 0
	long regName = RregNames[i];
        //Option 2
        //regName = static_cast<ostringstream*>( &(ostringstream() << RregNames[i]) )->str();

	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    node->RAW.push_back(conflictingIns);
	    conflictingIns->readCounter++;
	}
    }
    for (int i=0; i<numW; i++) {
	//Option 0
        long regName = WregNames[i];
        //Option 2
        //regName = static_cast<ostringstream*>( &(ostringstream() << WregNames[i]) )->str();

	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    node->WAW.push_back(conflictingIns);
	    conflictingIns->OWCount ++;
 
	    //Marks the end of a dead code stream
	    if (checkDeadness(conflictingIns)) {
		assert (newInst->seqNum != conflictingIns->ID);
		deadStreamCounter ++;
	    }
	    
	}
	regFile[regName] = node;
    }
}

template<class Impl>
void DeadInstAnalyzer<Impl>::analyzeRegSameValueOverwrite (INS_STRUCT *node, DynInstPtr newInst, int numW){

     //check operation type and return if checker is not activated
    if (op_type < 4) {
        return;
    }

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

