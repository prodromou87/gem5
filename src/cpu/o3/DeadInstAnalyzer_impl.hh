#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "debug/Prodromou.hh"
#include "arch/utility.hh"
#include <fstream>

#define SIZE 33

using namespace std;

template<class Impl>
DeadInstAnalyzer<Impl>::DeadInstAnalyzer() {

    //Initialize Variables
    globalInsCount = 0;
    deadInsCounter=0;
    deadStreamCounter = 0;
    totalInsCounter = 0;

    STREAM_WINDOW = 100000;

    deadInstructions = 0;
    
    // Register this object's statistics
    regStats();

    deadInstructionsList = new long long int [SIZE];
    
    ifstream input;
    input.open("/tmp/sorted_dead.txt");

    for (int i=0; i<SIZE; i++) {
	input >> deadInstructionsList[i];
    }

    for (int i=0; i<SIZE; i++) 
	DPRINTF(DeadInstAnalyzer,"LIST: %lli\n", deadInstructionsList[i]);

    //Prodromou: Delete it later
    t = 0;
}

template<class Impl>
long long int DeadInstAnalyzer<Impl>::nextDead() {
    //Should be ok for now
    return -1;
    return deadInstructionsList[t];
}

template<class Impl>
void DeadInstAnalyzer<Impl>::deadInstMet() {
    t++;
    t = t % SIZE;
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

    //Increase Total Instruction Count
    ++totalInsCounter;
    
    //Create new Instruction node
    INS_STRUCT *node = new INS_STRUCT;

    //Prodromou: Just a test. Make sure you verify this
    //node->ID = ++globalInsCount;
    node->ID = newInst->seqNum;

    node->address = newInst->instAddr();
    node->WRegCount = numW;
    node->isMemRef = newInst->isMemRef();
    if (newInst->isStore()) node->WRegCount=1; //Fake One output register for stores

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
    if (numR>0) DPRINTF(Prodromou, "Reads from: %s, %d\n", s2.str(), newInst->cpu->readIntReg(RregNames[0]));
    else DPRINTF(Prodromou, "Reads from: %s\n", s2.str());
    DPRINTF(Prodromou, "1: %d 2: %d 3: %d 4: %d 5: %d\n", newInst->isMemRef(), newInst->isLoad(), newInst->isStore(), newInst->isStoreConditional(), newInst->doneEACalc());
    DPRINTF(Prodromou, "Addr: %#08d, Eff. Addr: %#08d, Phys. addr: %#08d\n", newInst->instAddr(), newInst->effAddr, newInst->physEffAddr);


    // Check for room in the instruction window
    // If there is no room remove oldest instruction node
    // Also update the (virtual) register file
    if (instructions.size() == STREAM_WINDOW) {
        INS_STRUCT *temp_ptr = instructions.front();
        clearRegFile(temp_ptr);
        instructions.pop_front();
        delete(temp_ptr);
    }

    // First Step: Handle Read registers 
    // Reading Memory References => Load Instructions
    if (newInst->isLoad()) {
	long regName = newInst->effAddr;
	DPRINTF(Prodromou, "Load Instruction. Reading From: %#08d\n", newInst->effAddr);
	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    DPRINTF(Prodromou, "Aaaaaand... Conflict found!\n");
	    node->RAW.push_back(conflictingIns);
	    conflictingIns->readCounter++;
	}
    }
    // Second Step: Handle Dest Registers
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

	checkForSilent(newInst);

    }
    else { //Not Load Instructions (Could be stores)
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
	    // If the leaving instruction is a memory reference, 
	    // I need to delete the registers instead of just nullifying
	    // them. This will relax the memory requirements a little
	    // (Not all mem references kept in the regFile structure).
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
    deadInstructions
	.name(name() + ".DeadInstructions")
	.desc("Number of Dead Instructions");
	//.prereq(deadInstructions);
}


template<class Impl>
void DeadInstAnalyzer<Impl>::checkForSilent (DynInstPtr newInst) {
    DPRINTF (DeadInstAnalyzer, "Analyzing instruction [sn:%lld] for silent store\n", newInst->seqNum);

    //Request *req = newInst->createRequest();
    //req->setPaddr(newInst->physEffAddr);

    Addr effAddr = newInst->physEffAddr; ///effAddr
    int size = newInst->effSize;   
    uint8_t *temp = new uint8_t[size]; 
    Request::Flags flags;

    Request *req = new Request(effAddr, size, flags, newInst->masterId());
    DPRINTF (DeadInstAnalyzer, "Request Created. Creating Packet\n");

    PacketPtr data_pkt = new Packet(req, MemCmd::ReadReq);
    data_pkt->dataStatic(temp); //newInst->memData);
    
    DPRINTF (DeadInstAnalyzer, "Packet Created. Performing functional Access\n");
    (cpu->system->getPhysMem()).functionalAccess(data_pkt);

    delete[] temp;

    //uint64_t temp2 = data_pkt->get<uint64_t>();
    ostringstream o;
    o<<data_pkt->get<uint8_t>();
    string str = o.str();

    ostringstream t;
    t<<newInst->memData;
    string str2 = t.str();

    DPRINTF (DeadInstAnalyzer, "Functional Access returned: %s 2:%s\n", str, str2);
    

}


































