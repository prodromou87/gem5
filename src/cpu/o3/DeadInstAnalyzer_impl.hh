#include "cpu/o3/DeadInstAnalyzer.hh"
#include "debug/DeadInstAnalyzer.hh"
#include "debug/Prodromou.hh"
#include "arch/utility.hh"
using namespace std;

template<class Impl>
DeadInstAnalyzer<Impl>::DeadInstAnalyzer() {
    //Initialize Variables
    globalInsCount = 0;
    deadInsCounter=0;
    deadStreamCounter = 0;
    totalInsCounter = 0;

    STREAM_WINDOW = 10;

    deadInstructions = 0;
    
    // Register this object's statistics
    regStats();
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
    node->ID = ++globalInsCount;
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
	DPRINTF(Prodromou, "Load Instruction. Reading From: %#08d", newInst->effAddr);
	INS_STRUCT *conflictingIns = regFile[regName];
	if (conflictingIns != NULL) {
	    DPRINTF(Prodromou, "Aaaaaand... Conflict found!");
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



















