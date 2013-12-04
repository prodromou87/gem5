#ifndef __DEAD_INST_ANALYZER_HH__
#define __DEAD_INST_ANALYZER_HH__

template<class Impl>
class DeadInstAnalyzer {
    private:
    
    public:
	typedef typename Impl::DynInstPtr DynInstPtr;
	DeadInstAnalyzer();

	//The connecting function between the Analyzer and the 
	//commit stage. Commited instructions are sent to this 
	//function for deadness analyzis
	void analyze(DynInstPtr newInst);
};
#endif // __DEAD_INST_ANALYZER_HH__
