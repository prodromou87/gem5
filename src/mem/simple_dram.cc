/*
 * Copyright (c) 2010-2013 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2013 Amin Farmahini-Farahani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Andreas Hansson
 *          Ani Udipi
 */

#include "base/trace.hh"
#include "debug/Drain.hh"
#include "debug/DRAM.hh"
#include "debug/DRAMWR.hh"
#include "debug/MTHREADS.hh"
#include "mem/simple_dram.hh"
#include "sim/system.hh"

#include <algorithm>
#include <set>
#include <sstream>

using namespace std;

SimpleDRAM::SimpleDRAM(const SimpleDRAMParams* p) :
    AbstractMemory(p),
    port(name() + ".port", *this),
    retryRdReq(false), retryWrReq(false),
    rowHitFlag(false), stopReads(false), actTicks(p->activation_limit, 0),
    writeEvent(this), respondEvent(this),
    refreshEvent(this), nextReqEvent(this), drainManager(NULL),
    deviceBusWidth(p->device_bus_width), burstLength(p->burst_length),
    deviceRowBufferSize(p->device_rowbuffer_size),
    devicesPerRank(p->devices_per_rank),
    burstSize((devicesPerRank * burstLength * deviceBusWidth) / 8),
    rowBufferSize(devicesPerRank * deviceRowBufferSize),
    ranksPerChannel(p->ranks_per_channel),
    banksPerRank(p->banks_per_rank), channels(p->channels), rowsPerBank(0),
    readBufferSize(p->read_buffer_size),
    writeBufferSize(p->write_buffer_size),
    writeThresholdPerc(p->write_thresh_perc),
    tWTR(p->tWTR), tBURST(p->tBURST),
    tRCD(p->tRCD), tCL(p->tCL), tRP(p->tRP),
    tRFC(p->tRFC), tREFI(p->tREFI),
    tXAW(p->tXAW), activationLimit(p->activation_limit),
    memSchedPolicy(p->mem_sched_policy), addrMapping(p->addr_mapping),
    pageMgmt(p->page_policy),
    frontendLatency(p->static_frontend_latency),
    backendLatency(p->static_backend_latency),
    busBusyUntil(0), writeStartTime(0),
    prevArrival(0), numReqs(0),
    // Prodromou
    slowdownAccesses(p->slowdown_accesses),
    perAccessSlowdown(p->per_access_slowdown)
{
    // create the bank states based on the dimensions of the ranks and
    // banks
    banks.resize(ranksPerChannel);
    for (size_t c = 0; c < ranksPerChannel; ++c) {
        banks[c].resize(banksPerRank);
    }

    // round the write threshold percent to a whole number of entries
    // in the buffer
    writeThreshold = writeBufferSize * writeThresholdPerc / 100.0;

    //Prodromou
    batchedInsts = 0;
    numOfThreads = p->procs;
/*
    if (memSchedPolicy == Enums::tcm) {
	//Initialize shadow rows
	for (int i=0; i<banks.size(); i++) {
	    for (int j=0; j<banks[i].size(); j++) {
		banks[i][j].initShadowBanks(numOfThreads);
	    }
	}
    }
*/
    cout<<"Created simple dram with "<<numOfThreads<<" threads"<<endl;
    if (memSchedPolicy == Enums::fcfs) cout<<"Policy: FCFS"<<endl;
    if (memSchedPolicy == Enums::frfcfs) cout<<"Policy: FRFCFS"<<endl;
    if (memSchedPolicy == Enums::parbs) cout<<"Policy: PARBS"<<endl;
    if (memSchedPolicy == Enums::tcm) cout<<"Policy: TCM"<<endl;
    if (memSchedPolicy == Enums::mthreads) cout<<"Policy: mThreads"<<endl;
    if (memSchedPolicy == Enums::mstatic) cout<<"Policy: Mstatic"<<endl;
}

void
SimpleDRAM::init()
{
    if (!port.isConnected()) {
        fatal("SimpleDRAM %s is unconnected!\n", name());
    } else {
        port.sendRangeChange();
    }

    // we could deal with plenty options here, but for now do a quick
    // sanity check
    DPRINTF(DRAM, "Burst size %d bytes\n", burstSize);

    // determine the rows per bank by looking at the total capacity
    uint64_t capacity = ULL(1) << ceilLog2(AbstractMemory::size());

    DPRINTF(DRAM, "Memory capacity %lld (%lld) bytes\n", capacity,
            AbstractMemory::size());

    columnsPerRowBuffer = rowBufferSize / burstSize;

    DPRINTF(DRAM, "Row buffer size %d bytes with %d columns per row buffer\n",
            rowBufferSize, columnsPerRowBuffer);

    rowsPerBank = capacity / (rowBufferSize * banksPerRank * ranksPerChannel);

    if (range.interleaved()) {
        if (channels != range.stripes())
            panic("%s has %d interleaved address stripes but %d channel(s)\n",
                  name(), range.stripes(), channels);

        if (addrMapping == Enums::RaBaChCo) {
            if (rowBufferSize != range.granularity()) {
                panic("Interleaving of %s doesn't match RaBaChCo address map\n",
                      name());
            }
        } else if (addrMapping == Enums::RaBaCoCh) {
            if (burstSize != range.granularity()) {
                panic("Interleaving of %s doesn't match RaBaCoCh address map\n",
                      name());
            }
        } else if (addrMapping == Enums::CoRaBaCh) {
            if (burstSize != range.granularity())
                panic("Interleaving of %s doesn't match CoRaBaCh address map\n",
                      name());
        }
    }

    //Prodromou: Initialize TCM
    if (memSchedPolicy == Enums::tcm) {
	threadCluster = vector<bool>(numOfThreads, false);
	threadNiceness = vector<int>(numOfThreads, 0);
	shadowRBHitCount = vector<int>(numOfThreads, 0);
	mpkiPerThread = vector<float>(numOfThreads, 0);
	BLP = vector<int>(numOfThreads, 0);
	reqPerThread = vector<int>(numOfThreads, 0);

	sortedNiceness = vector<int>(numOfThreads, 0);
	for (int i=0; i<numOfThreads; i++) sortedNiceness[i] = i;

	shuffleState = numOfThreads-1;
	lastSampleTime = 0;
	samplingThreshold = 2500000 * 500; //curTick() in increments of 500
	samplesTaken = 0;
	//LL at the end converts the number to long. 
	//Otherwise compilation fails.
	quantumThreshold = 10000000LL * 500LL; //curTick() in increments of 500
	lastQuantumTime = 0;
	clusterThresh = 4.0 / numOfThreads; //Paper suggests 2/N - 6/N

    }

    //if (memSchedPolicy == Enums::mthreads) {
	mthreadsReqsServiced = vector<int>(numOfThreads, 0);
    //}
}

void
SimpleDRAM::startup()
{
    // print the configuration of the controller
    printParams();

    // kick off the refresh
    schedule(refreshEvent, curTick() + tREFI);
}

Tick
SimpleDRAM::recvAtomic(PacketPtr pkt)
{
    DPRINTF(DRAM, "recvAtomic: %s 0x%x\n", pkt->cmdString(), pkt->getAddr());

    // do the actual memory access and turn the packet into a response
    access(pkt);

    Tick latency = 0;
    if (!pkt->memInhibitAsserted() && pkt->hasData()) {
        // this value is not supposed to be accurate, just enough to
        // keep things going, mimic a closed page
        latency = tRP + tRCD + tCL;
    }
    return latency;
}

bool
SimpleDRAM::readQueueFull(unsigned int neededEntries) const
{
    DPRINTF(DRAM, "Read queue limit %d, current size %d, entries needed %d\n",
            readBufferSize, readQueue.size() + respQueue.size(),
            neededEntries);

    return
        (readQueue.size() + respQueue.size() + neededEntries) > readBufferSize;
}

bool
SimpleDRAM::writeQueueFull(unsigned int neededEntries) const
{
    DPRINTF(DRAM, "Write queue limit %d, current size %d, entries needed %d\n",
            writeBufferSize, writeQueue.size(), neededEntries);
    return (writeQueue.size() + neededEntries) > writeBufferSize;
}

SimpleDRAM::DRAMPacket*
SimpleDRAM::decodeAddr(PacketPtr pkt, Addr dramPktAddr, unsigned size)
{
    // decode the address based on the address mapping scheme, with
    // Ra, Co, Ba and Ch denoting rank, column, bank and channel,
    // respectively
    uint8_t rank;
    uint16_t bank;
    uint16_t row;

    // truncate the address to the access granularity
    Addr addr = dramPktAddr / burstSize;

    // we have removed the lowest order address bits that denote the
    // position within the column
    if (addrMapping == Enums::RaBaChCo) {
        // the lowest order bits denote the column to ensure that
        // sequential cache lines occupy the same row
        addr = addr / columnsPerRowBuffer;

        // take out the channel part of the address
        addr = addr / channels;

        // after the channel bits, get the bank bits to interleave
        // over the banks
        bank = addr % banksPerRank;
        addr = addr / banksPerRank;

        // after the bank, we get the rank bits which thus interleaves
        // over the ranks
        rank = addr % ranksPerChannel;
        addr = addr / ranksPerChannel;

        // lastly, get the row bits
        row = addr % rowsPerBank;
        addr = addr / rowsPerBank;
    } else if (addrMapping == Enums::RaBaCoCh) {
        // take out the channel part of the address
        addr = addr / channels;

        // next, the column
        addr = addr / columnsPerRowBuffer;

        // after the column bits, we get the bank bits to interleave
        // over the banks
        bank = addr % banksPerRank;
        addr = addr / banksPerRank;

        // after the bank, we get the rank bits which thus interleaves
        // over the ranks
        rank = addr % ranksPerChannel;
        addr = addr / ranksPerChannel;

        // lastly, get the row bits
        row = addr % rowsPerBank;
        addr = addr / rowsPerBank;
    } else if (addrMapping == Enums::CoRaBaCh) {
        // optimise for closed page mode and utilise maximum
        // parallelism of the DRAM (at the cost of power)

        // take out the channel part of the address, not that this has
        // to match with how accesses are interleaved between the
        // controllers in the address mapping
        addr = addr / channels;

        // start with the bank bits, as this provides the maximum
        // opportunity for parallelism between requests
        bank = addr % banksPerRank;
        addr = addr / banksPerRank;

        // next get the rank bits
        rank = addr % ranksPerChannel;
        addr = addr / ranksPerChannel;

        // next the column bits which we do not need to keep track of
        // and simply skip past
        addr = addr / columnsPerRowBuffer;

        // lastly, get the row bits
        row = addr % rowsPerBank;
        addr = addr / rowsPerBank;
    } else
        panic("Unknown address mapping policy chosen!");

    assert(rank < ranksPerChannel);
    assert(bank < banksPerRank);
    assert(row < rowsPerBank);

    DPRINTF(DRAM, "Address: %lld Rank %d Bank %d Row %d\n",
            dramPktAddr, rank, bank, row);

    // create the corresponding DRAM packet with the entry time and
    // ready time set to the current tick, the latter will be updated
    // later
    return new DRAMPacket(pkt, rank, bank, row, dramPktAddr, size,
                          banks[rank][bank]);
}

void
SimpleDRAM::addToReadQueue(PacketPtr pkt, unsigned int pktCount)
{
    // only add to the read queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(!pkt->isWrite());

    assert(pktCount != 0);

    // if the request size is larger than burst size, the pkt is split into
    // multiple DRAM packets
    // Note if the pkt starting address is not aligened to burst size, the
    // address of first DRAM packet is kept unaliged. Subsequent DRAM packets
    // are aligned to burst size boundaries. This is to ensure we accurately
    // check read packets against packets in write queue.
    Addr addr = pkt->getAddr();
    unsigned pktsServicedByWrQ = 0;
    BurstHelper* burst_helper = NULL;
    for (int cnt = 0; cnt < pktCount; ++cnt) {
        unsigned size = std::min((addr | (burstSize - 1)) + 1,
                        pkt->getAddr() + pkt->getSize()) - addr;
        readPktSize[ceilLog2(size)]++;
        readBursts++;

        // First check write buffer to see if the data is already at
        // the controller
        bool foundInWrQ = false;
        for (auto i = writeQueue.begin(); i != writeQueue.end(); ++i) {
            // check if the read is subsumed in the write entry we are
            // looking at
            if ((*i)->addr <= addr &&
                (addr + size) <= ((*i)->addr + (*i)->size)) {
                foundInWrQ = true;
                servicedByWrQ++;
                pktsServicedByWrQ++;
                DPRINTF(DRAM, "Read to addr %lld with size %d serviced by "
                        "write queue\n", addr, size);
                bytesRead += burstSize;
                bytesConsumedRd += size;
                break;
            }
        }

        // If not found in the write q, make a DRAM packet and
        // push it onto the read queue
        if (!foundInWrQ) {

            // Make the burst helper for split packets
            if (pktCount > 1 && burst_helper == NULL) {
                DPRINTF(DRAM, "Read to addr %lld translates to %d "
                        "dram requests\n", pkt->getAddr(), pktCount);
                burst_helper = new BurstHelper(pktCount);
            }

            DRAMPacket* dram_pkt = decodeAddr(pkt, addr, size);
            dram_pkt->burstHelper = burst_helper;

            assert(!readQueueFull(1));
            rdQLenPdf[readQueue.size() + respQueue.size()]++;

            DPRINTF(DRAM, "Adding to read queue\n");

            readQueue.push_back(dram_pkt);

	    //Prodromou: If policy is TCM I need to store the packet in a cluster
	    if (memSchedPolicy == Enums::tcm) {
		int threadId = dram_pkt->thread;
		if (threadId == -1) { //Just store it in the BW sensitive cluster
		    bwSensRead.push_back(dram_pkt);
		}
		else {
		    reqPerThread[threadId]++;
		    if (threadCluster[threadId]) {
			latSensRead.push_back(dram_pkt);
		    }
		    else bwSensRead.push_back(dram_pkt);
		}
	    }

	    //Prodromou: Update stats for mThreads
	    //Do it for every policy - not just mthreads - for statistics
	    //if (memSchedPolicy == Enums::mthreads) {
		int threadNum = dram_pkt->thread;
		if (threadNum != -1) {
		    mthreadsReqsPerThread[threadNum]++;
		    mthreadsAvgQLenPerThread[threadNum]++;
		}
	    //}

            // Update stats
            uint32_t bank_id = banksPerRank * dram_pkt->rank + dram_pkt->bank;
            assert(bank_id < ranksPerChannel * banksPerRank);
            perBankRdReqs[bank_id]++;

            avgRdQLen = readQueue.size() + respQueue.size();
        }

        // Starting address of next dram pkt (aligend to burstSize boundary)
        addr = (addr | (burstSize - 1)) + 1;
    }

    // If all packets are serviced by write queue, we send the repsonse back
    if (pktsServicedByWrQ == pktCount) {
        accessAndRespond(pkt, frontendLatency);
        return;
    }

    // Update how many split packets are serviced by write queue
    if (burst_helper != NULL)
        burst_helper->burstsServiced = pktsServicedByWrQ;

    // If we are not already scheduled to get the read request out of
    // the queue, do so now
    if (!nextReqEvent.scheduled() && !stopReads) {
        DPRINTF(DRAM, "Request scheduled immediately\n");
        schedule(nextReqEvent, curTick());
    }
}

void
SimpleDRAM::processWriteEvent()
{
    assert(!writeQueue.empty());
    uint32_t numWritesThisTime = 0;

    DPRINTF(DRAMWR, "Beginning DRAM Writes\n");
    Tick temp1 M5_VAR_USED = std::max(curTick(), busBusyUntil);
    Tick temp2 M5_VAR_USED = std::max(curTick(), maxBankFreeAt());

    // @todo: are there any dangers with the untimed while loop?
    while (!writeQueue.empty()) {
        if (numWritesThisTime > writeThreshold) {
            DPRINTF(DRAMWR, "Hit write threshold %d\n", writeThreshold);
            break;
        }

        chooseNextWrite();
        DRAMPacket* dram_pkt = writeQueue.front();

        // sanity check
        assert(dram_pkt->size <= burstSize);

        // What's the earliest the request can be put on the bus
        Tick schedTime = std::max(curTick(), busBusyUntil);

        DPRINTF(DRAMWR, "Asking for latency estimate at %lld\n",
                schedTime + tBURST);

        pair<Tick, Tick> lat = estimateLatency(dram_pkt, schedTime + tBURST);
        Tick accessLat = lat.second;

        // look at the rowHitFlag set by estimateLatency
        if (rowHitFlag)
            writeRowHits++;

        Bank& bank = dram_pkt->bank_ref;

        if (pageMgmt == Enums::open) {
            bank.openRow = dram_pkt->row;
            bank.freeAt = schedTime + tBURST + std::max(accessLat, tCL);
            busBusyUntil = bank.freeAt - tCL;
            bank.bytesAccessed += burstSize;

            if (!rowHitFlag) {
                bank.tRASDoneAt = bank.freeAt + tRP;
                recordActivate(bank.freeAt - tCL - tRCD);
                busBusyUntil = bank.freeAt - tCL - tRCD;

                // sample the number of bytes accessed and reset it as
                // we are now closing this row
                bytesPerActivate.sample(bank.bytesAccessed);
                bank.bytesAccessed = 0;
            }
        } else if (pageMgmt == Enums::close) {
            bank.freeAt = schedTime + tBURST + accessLat + tRP + tRP;
            // Work backwards from bank.freeAt to determine activate time
            recordActivate(bank.freeAt - tRP - tRP - tCL - tRCD);
            busBusyUntil = bank.freeAt - tRP - tRP - tCL - tRCD;
            DPRINTF(DRAMWR, "processWriteEvent::bank.freeAt for "
                    "banks_id %d is %lld\n",
                    dram_pkt->rank * banksPerRank + dram_pkt->bank,
                    bank.freeAt);
            bytesPerActivate.sample(burstSize);
        } else
            panic("Unknown page management policy chosen\n");

        DPRINTF(DRAMWR, "Done writing to address %lld\n", dram_pkt->addr);

        DPRINTF(DRAMWR, "schedtime is %lld, tBURST is %lld, "
                "busbusyuntil is %lld\n",
                schedTime, tBURST, busBusyUntil);

        writeQueue.pop_front();
/*
	//Prodromou: Also remove from tcm cluster
	int threadId = dram_pkt->thread;
	DRAMPacket* temp = NULL;
	if (memSchedPolicy == Enums::tcm) {
	    if (threadId != -1) {
		if (threadCluster[threadId]) {
		    temp = latSensRead.front();
		    latSensRead.pop_front();
		}
		else {
		    temp = bwSensRead.front();
		    bwSensRead.pop_front();
		}
		if (temp != dram_pkt) panic ("Something went wrong in removing from write cluster");
	    }	
	}
*/
        delete dram_pkt;

        numWritesThisTime++;
    }

    DPRINTF(DRAMWR, "Completed %d writes, bus busy for %lld ticks,"\
            "banks busy for %lld ticks\n", numWritesThisTime,
            busBusyUntil - temp1, maxBankFreeAt() - temp2);

    // Update stats
    avgWrQLen = writeQueue.size();

    // turn the bus back around for reads again
    busBusyUntil += tWTR;
    stopReads = false;

    if (retryWrReq) {
        retryWrReq = false;
        port.sendRetry();
    }

    // if there is nothing left in any queue, signal a drain
    if (writeQueue.empty() && readQueue.empty() &&
        respQueue.empty () && drainManager) {
        drainManager->signalDrainDone();
        drainManager = NULL;
    }

    // Once you're done emptying the write queue, check if there's
    // anything in the read queue, and call schedule if required. The
    // retry above could already have caused it to be scheduled, so
    // first check
    if (!nextReqEvent.scheduled())
        schedule(nextReqEvent, busBusyUntil);
}

void
SimpleDRAM::triggerWrites()
{
    DPRINTF(DRAM, "Writes triggered at %lld\n", curTick());
    // Flag variable to stop any more read scheduling
    stopReads = true;

    writeStartTime = std::max(busBusyUntil, curTick()) + tWTR;

    DPRINTF(DRAM, "Writes scheduled at %lld\n", writeStartTime);

    assert(writeStartTime >= curTick());
    assert(!writeEvent.scheduled());
    schedule(writeEvent, writeStartTime);
}

void
SimpleDRAM::addToWriteQueue(PacketPtr pkt, unsigned int pktCount)
{
    // only add to the write queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(pkt->isWrite());

    // if the request size is larger than burst size, the pkt is split into
    // multiple DRAM packets
    Addr addr = pkt->getAddr();
    for (int cnt = 0; cnt < pktCount; ++cnt) {
        unsigned size = std::min((addr | (burstSize - 1)) + 1,
                        pkt->getAddr() + pkt->getSize()) - addr;
        writePktSize[ceilLog2(size)]++;
        writeBursts++;

        // see if we can merge with an existing item in the write
        // queue and keep track of whether we have merged or not so we
        // can stop at that point and also avoid enqueueing a new
        // request
        bool merged = false;
        auto w = writeQueue.begin();

        while(!merged && w != writeQueue.end()) {
            // either of the two could be first, if they are the same
            // it does not matter which way we go
            if ((*w)->addr >= addr) {
                // the existing one starts after the new one, figure
                // out where the new one ends with respect to the
                // existing one
                if ((addr + size) >= ((*w)->addr + (*w)->size)) {
                    // check if the existing one is completely
                    // subsumed in the new one
                    DPRINTF(DRAM, "Merging write covering existing burst\n");
                    merged = true;
                    // update both the address and the size
                    (*w)->addr = addr;
                    (*w)->size = size;
                } else if ((addr + size) >= (*w)->addr &&
                           ((*w)->addr + (*w)->size - addr) <= burstSize) {
                    // the new one is just before or partially
                    // overlapping with the existing one, and together
                    // they fit within a burst
                    DPRINTF(DRAM, "Merging write before existing burst\n");
                    merged = true;
                    // the existing queue item needs to be adjusted with
                    // respect to both address and size
                    (*w)->addr = addr;
                    (*w)->size = (*w)->addr + (*w)->size - addr;
                }
            } else {
                // the new one starts after the current one, figure
                // out where the existing one ends with respect to the
                // new one
                if (((*w)->addr + (*w)->size) >= (addr + size)) {
                    // check if the new one is completely subsumed in the
                    // existing one
                    DPRINTF(DRAM, "Merging write into existing burst\n");
                    merged = true;
                    // no adjustments necessary
                } else if (((*w)->addr + (*w)->size) >= addr &&
                           (addr + size - (*w)->addr) <= burstSize) {
                    // the existing one is just before or partially
                    // overlapping with the new one, and together
                    // they fit within a burst
                    DPRINTF(DRAM, "Merging write after existing burst\n");
                    merged = true;
                    // the address is right, and only the size has
                    // to be adjusted
                    (*w)->size = addr + size - (*w)->addr;
                }
            }
            ++w;
        }

        // if the item was not merged we need to create a new write
        // and enqueue it
        if (!merged) {
            DRAMPacket* dram_pkt = decodeAddr(pkt, addr, size);

            assert(writeQueue.size() < writeBufferSize);
            wrQLenPdf[writeQueue.size()]++;

            DPRINTF(DRAM, "Adding to write queue\n");

            writeQueue.push_back(dram_pkt);

	    //Prodromou: If the policy is TCM store it in a cluster
	    if (memSchedPolicy == Enums::tcm) {
		int threadId = dram_pkt->thread;
		if (threadId == -1) { //Just store it in BW sensitive
		    bwSensWrite.push_back(dram_pkt);
		}
		else {
		    reqPerThread[threadId]++;
		    if (threadCluster[threadId]) latSensWrite.push_back(dram_pkt);
		    else bwSensWrite.push_back(dram_pkt);
		}
	    }

            // Update stats
            uint32_t bank_id = banksPerRank * dram_pkt->rank + dram_pkt->bank;
            assert(bank_id < ranksPerChannel * banksPerRank);
            perBankWrReqs[bank_id]++;

            avgWrQLen = writeQueue.size();
        }

        bytesConsumedWr += size;
        bytesWritten += burstSize;

        // Starting address of next dram pkt (aligend to burstSize boundary)
        addr = (addr | (burstSize - 1)) + 1;
    }

    // we do not wait for the writes to be send to the actual memory,
    // but instead take responsibility for the consistency here and
    // snoop the write queue for any upcoming reads
    // @todo, if a pkt size is larger than burst size, we might need a
    // different front end latency
    accessAndRespond(pkt, frontendLatency);

    // If your write buffer is starting to fill up, drain it!
    if (writeQueue.size() > writeThreshold && !stopReads){
        triggerWrites();
    }
}

void
SimpleDRAM::printParams() const
{
    // Sanity check print of important parameters
    DPRINTF(DRAM,
            "Memory controller %s physical organization\n"      \
            "Number of devices per rank   %d\n"                 \
            "Device bus width (in bits)   %d\n"                 \
            "DRAM data bus burst          %d\n"                 \
            "Row buffer size              %d\n"                 \
            "Columns per row buffer       %d\n"                 \
            "Rows    per bank             %d\n"                 \
            "Banks   per rank             %d\n"                 \
            "Ranks   per channel          %d\n"                 \
            "Total mem capacity           %u\n",
            name(), devicesPerRank, deviceBusWidth, burstSize, rowBufferSize,
            columnsPerRowBuffer, rowsPerBank, banksPerRank, ranksPerChannel,
            rowBufferSize * rowsPerBank * banksPerRank * ranksPerChannel);

    string scheduler =  memSchedPolicy == Enums::fcfs ? "FCFS" : "FR-FCFS";
    string address_mapping = addrMapping == Enums::RaBaChCo ? "RaBaChCo" :
        (addrMapping == Enums::RaBaCoCh ? "RaBaCoCh" : "CoRaBaCh");
    string page_policy = pageMgmt == Enums::open ? "OPEN" : "CLOSE";

    DPRINTF(DRAM,
            "Memory controller %s characteristics\n"    \
            "Read buffer size     %d\n"                 \
            "Write buffer size    %d\n"                 \
            "Write buffer thresh  %d\n"                 \
            "Scheduler            %s\n"                 \
            "Address mapping      %s\n"                 \
            "Page policy          %s\n",
            name(), readBufferSize, writeBufferSize, writeThreshold,
            scheduler, address_mapping, page_policy);

    DPRINTF(DRAM, "Memory controller %s timing specs\n" \
            "tRCD      %d ticks\n"                        \
            "tCL       %d ticks\n"                        \
            "tRP       %d ticks\n"                        \
            "tBURST    %d ticks\n"                        \
            "tRFC      %d ticks\n"                        \
            "tREFI     %d ticks\n"                        \
            "tWTR      %d ticks\n"                        \
            "tXAW (%d) %d ticks\n",
            name(), tRCD, tCL, tRP, tBURST, tRFC, tREFI, tWTR,
            activationLimit, tXAW);
}

void
SimpleDRAM::printQs() const {
    DPRINTF(DRAM, "===READ QUEUE===\n\n");
    for (auto i = readQueue.begin() ;  i != readQueue.end() ; ++i) {
        DPRINTF(DRAM, "Read %lu\n", (*i)->addr);
    }
    DPRINTF(DRAM, "\n===RESP QUEUE===\n\n");
    for (auto i = respQueue.begin() ;  i != respQueue.end() ; ++i) {
        DPRINTF(DRAM, "Response %lu\n", (*i)->addr);
    }
    DPRINTF(DRAM, "\n===WRITE QUEUE===\n\n");
    for (auto i = writeQueue.begin() ;  i != writeQueue.end() ; ++i) {
        DPRINTF(DRAM, "Write %lu\n", (*i)->addr);
    }
}

bool
SimpleDRAM::recvTimingReq(PacketPtr pkt)
{
    /// @todo temporary hack to deal with memory corruption issues until
    /// 4-phase transactions are complete
    for (int x = 0; x < pendingDelete.size(); x++) 
    	delete pendingDelete[x];
    
    pendingDelete.clear();

    // This is where we enter from the outside world
    DPRINTF(DRAM, "recvTimingReq: request %s addr %lld size %d\n",
            pkt->cmdString(), pkt->getAddr(), pkt->getSize());

    // simply drop inhibited packets for now
    if (pkt->memInhibitAsserted()) {
        DPRINTF(DRAM,"Inhibited packet -- Dropping it now\n");
        pendingDelete.push_back(pkt);
        return true;
    }

   // Every million accesses, print the state of the queues
   if (numReqs % 1000000 == 0)
       printQs();

    // Calc avg gap between requests
    if (prevArrival != 0) {
        totGap += curTick() - prevArrival;
    }
    prevArrival = curTick();


    // Find out how many dram packets a pkt translates to
    // If the burst size is equal or larger than the pkt size, then a pkt
    // translates to only one dram packet. Otherwise, a pkt translates to
    // multiple dram packets
    unsigned size = pkt->getSize();
    unsigned offset = pkt->getAddr() & (burstSize - 1);
    unsigned int dram_pkt_count = divCeil(offset + size, burstSize);

    // check local buffers and do not accept if full
    if (pkt->isRead()) {
        assert(size != 0);
        if (readQueueFull(dram_pkt_count)) {
            DPRINTF(DRAM, "Read queue full, not accepting\n");
            // remember that we have to retry this port
            retryRdReq = true;
            numRdRetry++;
            return false;
        } else {
            addToReadQueue(pkt, dram_pkt_count);
            readReqs++;
            numReqs++;
        }
    } else if (pkt->isWrite()) {
        assert(size != 0);
        if (writeQueueFull(dram_pkt_count)) {
            DPRINTF(DRAM, "Write queue full, not accepting\n");
            // remember that we have to retry this port
            retryWrReq = true;
            numWrRetry++;
            return false;
        } else {
            addToWriteQueue(pkt, dram_pkt_count);
            writeReqs++;
            numReqs++;
        }
    } else {
        DPRINTF(DRAM,"Neither read nor write, ignore timing\n");
        neitherReadNorWrite++;
        accessAndRespond(pkt, 1);
    }

    retryRdReq = false;
    retryWrReq = false;
    return true;
}

void
SimpleDRAM::processRespondEvent()
{
    DPRINTF(DRAM,
            "processRespondEvent(): Some req has reached its readyTime\n");

    DRAMPacket* dram_pkt = respQueue.front();

    // Actually responds to the requestor
    bytesConsumedRd += dram_pkt->size;
    bytesRead += burstSize;
    if (dram_pkt->burstHelper) {
        // it is a split packet
        dram_pkt->burstHelper->burstsServiced++;
        if (dram_pkt->burstHelper->burstsServiced ==
                                  dram_pkt->burstHelper->burstCount) {
            // we have now serviced all children packets of a system packet
            // so we can now respond to the requester
            // @todo we probably want to have a different front end and back
            // end latency for split packets
            accessAndRespond(dram_pkt->pkt, frontendLatency + backendLatency);
            delete dram_pkt->burstHelper;
            dram_pkt->burstHelper = NULL;
        }
    } else {
        // it is not a split packet
        accessAndRespond(dram_pkt->pkt, frontendLatency + backendLatency);
    }

    //Prodromou: Request Serviced. Update stats for mthreads
    // Do this for all policies - statistics
	int threadNum = dram_pkt->thread;
	if (threadNum != -1) {
	    mthreadsAvgMemAccTimePerThread[threadNum] += (dram_pkt->readyTime - dram_pkt->entryTime);
	    mthreadsReqsServiced[threadNum]++;
	    mthreadsAvgQLenPerThread[threadNum]--;
	    //cout<<"Added "<<dram_pkt->readyTime - dram_pkt->entryTime<<endl;
	    //cout<<"Reported average: "<<mthreadsAvgMemAccTimePerThread[threadNum].value() / mthreadsReqsServiced[threadNum]<<endl;
	    //cout<<"Avg Q Length: "<<mthreadsAvgQLenPerThread[threadNum].value()<<endl;
	}

    delete respQueue.front();
    respQueue.pop_front();

    // Update stats
    avgRdQLen = readQueue.size() + respQueue.size();

    if (!respQueue.empty()) {
        assert(respQueue.front()->readyTime >= curTick());
        assert(!respondEvent.scheduled());
        schedule(respondEvent, respQueue.front()->readyTime);
    } else {
        // if there is nothing left in any queue, signal a drain
        if (writeQueue.empty() && readQueue.empty() &&
            drainManager) {
            drainManager->signalDrainDone();
            drainManager = NULL;
        }
    }

    // We have made a location in the queue available at this point,
    // so if there is a read that was forced to wait, retry now
    if (retryRdReq) {
        retryRdReq = false;
        port.sendRetry();
    }
}

void
SimpleDRAM::chooseNextWrite()
{
    // This method does the arbitration between write requests. The
    // chosen packet is simply moved to the head of the write
    // queue. The other methods know that this is the place to
    // look. For example, with FCFS, this method does nothing
    assert(!writeQueue.empty());

    if ((writeQueue.size() == 1) && (memSchedPolicy != Enums::parbs)) {
        DPRINTF(DRAMWR, "Single write request, nothing to do\n");
	if (memSchedPolicy == Enums::tcm) {
            int threadId = writeQueue[0]->thread;
            if (threadId == -1) {
		bwSensWrite.pop_front();
	    }
	    else if (threadCluster[threadId]) {
                latSensWrite.pop_front();
            }
            else {
                bwSensWrite.pop_front();
            }
        }

        return;
    }

    if (memSchedPolicy == Enums::fcfs) {
        // Do nothing, since the correct request is already head
    } else if (memSchedPolicy == Enums::frfcfs) {
        auto i = writeQueue.begin();
        bool foundRowHit = false;
        while (!foundRowHit && i != writeQueue.end()) {
            DRAMPacket* dram_pkt = *i;
            const Bank& bank = dram_pkt->bank_ref;
            if (bank.openRow == dram_pkt->row) { //FR part
                DPRINTF(DRAMWR, "Write row buffer hit\n");
                writeQueue.erase(i);
                writeQueue.push_front(dram_pkt);
                foundRowHit = true;
            } else { //FCFS part
                ;
            }
            ++i;
        }
    } else if (memSchedPolicy == Enums::parbs) { 
	//Prodromou: Try to implement parbs logic here to speed up simulations
	parbsNextWrite();
    } else if (memSchedPolicy == Enums::tcm) {
	tcmNextWrite();
    } else if (memSchedPolicy == Enums::mthreads) {
	mthreadsNextWrite();
    } else if (memSchedPolicy == Enums::mstatic) {
	mStaticNextWrite();
    }
    else
        panic("No scheduling policy chosen\n");

    DPRINTF(DRAMWR, "Selected next write request\n");
}

bool
SimpleDRAM::chooseNextRead()
{


    //Prodromou: Calculate N for every mechanism so it's displayed 
    //in the statistics
    int tableDimension = 20; // CAREFUL: This is hardcoded at two places
    // For each thread
    for (int threadNum=0; threadNum<numOfThreads; threadNum++) {
	//Generate the table based on current RTT
	//Should RTT be per thread?
	float RTT = mthreadsAvgMemAccTimePerThread[threadNum].value() / mthreadsReqsServiced[threadNum];
	mthreadsLookupTable = new LookupTable(tableDimension, RTT);
	
	DPRINTF (MTHREADS, mthreadsLookupTable->printTable().c_str());

	//Get the thread's search terms -- reads/time & avgQLen (X & Q)
	float X = mthreadsReqsPerThread[threadNum].value() / (curTick() / 1000000000);
	float Q = mthreadsAvgQLenPerThread[threadNum].value();

	DPRINTF(MTHREADS, "RTT: %.3f, X: %.3f, Q: %.3f\n", RTT, X, Q);

	//Query for N and Z
	pair<int,int> nAndZ = mthreadsLookupTable->searchFor(X,Q);
	perThreadNPdf[threadNum][nAndZ.first]++;
	//cout <<", N: "<<nAndZ.first<<endl;
	DPRINTF (MTHREADS, "Returned: %d, %d\n", nAndZ.first, nAndZ.second);

	//Delete LookupTable
	delete(mthreadsLookupTable);
    }

    // this method does the arbitration between read requests. the
    // chosen packet is simply moved to the head of the queue. the
    // other methods know that this is the place to look. for example,
    // with fcfs, this method does nothing
    if (readQueue.empty()) {
        DPRINTF(DRAM, "no read request to select\n");
        return false;
    }

    // if there is only one request then there is nothing left to do
    // prodromou: for par-bs i need to follow the entire procedure 
    // in order to take care of batching and book-keeping
    if ((readQueue.size() == 1) && (memSchedPolicy != Enums::parbs)){
	//Prodromou: In case of TCM remove the packet from the cluster
	if (memSchedPolicy == Enums::tcm) {
	    int threadId = readQueue[0]->thread;
	    if (threadId == -1) {
		bwSensRead.pop_front();
	    }
	    else if (threadCluster[threadId]) {
		latSensRead.pop_front();
	    }
	    else {
		bwSensRead.pop_front();
	    }
	}
        return true;
    }

    if (memSchedPolicy == Enums::fcfs) {
        // do nothing, since the request to serve is already the first
        // one in the read queue
    } else if (memSchedPolicy == Enums::frfcfs) {
        for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
            DRAMPacket* dram_pkt = *i;
            const Bank& bank = dram_pkt->bank_ref;
            // check if it is a row hit
            if (bank.openRow == dram_pkt->row) { //fr part
                DPRINTF(DRAM, "row buffer hit\n");
                readQueue.erase(i);
                readQueue.push_front(dram_pkt);
                break;
            } else { //fcfs part
                ;
            }
        }
    } else if (memSchedPolicy == Enums::parbs) {
	parbsNextRead();
    } else if (memSchedPolicy == Enums::tcm) {
	tcmNextRead();
    } else if (memSchedPolicy == Enums::mthreads) {
	mthreadsNextRead();
    } else if (memSchedPolicy == Enums::mstatic) {
	mStaticNextRead();
    } else
        panic("no scheduling policy chosen!\n");

	DPRINTF(DRAM, "selected next read request\n");
    return true;
}

void
SimpleDRAM::parbsNextRead() {
    //first create new batch if necessary
    parbsCheckForBatch();

    //the threads are ranked. we need to find a row hit or the oldest 
    //request of the highest ranked thread. requests are stored in 
    //chronological order so we don't need to worry about finding the 
    //oldest one

    // this will store the request with the highest priority in case 
    // we don't find a row buffer hit
    auto prioPkt = readQueue.begin();

    for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
	DRAMPacket* dram_pkt = *i;
	const Bank& bank = dram_pkt->bank_ref;

	int prioRank = threadRank[(*prioPkt)->thread];
	int competingRank = threadRank[dram_pkt->thread];
	if (competingRank < prioRank) {
	    prioPkt = i;
	}

	// check if it is a row hit
	if (bank.openRow == dram_pkt->row) { //fr part
	    DPRINTF(DRAM, "row buffer hit\n");
	    readQueue.erase(i);
	    readQueue.push_front(dram_pkt);
	    batchedInsts--;
	    return;
	} 
    }

    // no row buffer hit found. we are going with the priopkt option
    DRAMPacket* prio_pkt = *prioPkt;
    readQueue.erase(prioPkt);
    readQueue.push_front(prio_pkt);

    //done. decrement the number of batched instructions
    batchedInsts--;
}

void
SimpleDRAM::mStaticNextWrite() {
    // Thread order for mcf,libquantum,GemsFDTD,xalancbmk: 1,3,2,0
   
    int threadPriority[4] = {1,3,2,0};
 
    for (auto i = writeQueue.begin(); i != writeQueue.end() ; ++i) {
	DRAMPacket* dram_pkt = *i;
	const Bank& bank = dram_pkt->bank_ref;

	// check if it is a row hit
	if (bank.openRow == dram_pkt->row) { //fr part
	    DPRINTF(DRAM, "row buffer hit\n");
	    writeQueue.erase(i);
	    writeQueue.push_front(dram_pkt);
	    return;
	}
    }

    //No RBHs found. Choose the first packet of the thread 
    //with the highest priority
    for (int j=0; j<numOfThreads; j++) {
        int threadNum = threadPriority[j];
        for (auto i = writeQueue.begin(); i != writeQueue.end() ; ++i) {
            DRAMPacket* dram_pkt = *i;
            int currentThread = dram_pkt->thread;

            if (currentThread == threadNum) {
                    writeQueue.erase(i);
                    writeQueue.push_front(dram_pkt);
                    return;
            }
        }
    } 
}

void
SimpleDRAM::mStaticNextRead() {
    // Thread order for mcf,libquantum,GemsFDTD,xalancbmk: 1,3,2,0
   
    int threadPriority[4] = {1,3,2,0};
 
    for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
        DRAMPacket* dram_pkt = *i;
        const Bank& bank = dram_pkt->bank_ref;

        // check if it is a row hit
        if (bank.openRow == dram_pkt->row) { //fr part
            DPRINTF(DRAM, "row buffer hit\n");
            readQueue.erase(i);
            readQueue.push_front(dram_pkt);
            return;
        }
    }

    //No RBHs found. Choose the first packet of the thread 
    //with the highest priority
    for (int j=0; j<numOfThreads; j++) {
        int threadNum = threadPriority[j];
        for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
            DRAMPacket* dram_pkt = *i;
            int currentThread = dram_pkt->thread;

            if (currentThread == threadNum) {
                    readQueue.erase(i);
                    readQueue.push_front(dram_pkt);
                    return;
            }
        }
    } 
}

void 
SimpleDRAM::parbsNextWrite() {
    //first create new batch if necessary
    parbsCheckForBatch();

    //the threads are ranked. we need to find a row hit or the oldest
    //request of the highest ranked thread. requests are stored in
    //chronological order so we don't need to worry about finding the
    //oldest one

    // this will store the request with the highest priority in case
    // we don't find a row buffer hit
    auto prioPkt = writeQueue.begin();

    for (auto i = writeQueue.begin(); i != writeQueue.end() ; ++i) {
        DRAMPacket* dram_pkt = *i;
        const Bank& bank = dram_pkt->bank_ref;

        int prioRank = threadRank[(*prioPkt)->thread];
        int competingRank = threadRank[dram_pkt->thread];
        if (competingRank < prioRank) {
            prioPkt = i;
        }

        // check if it is a row hit
        if (bank.openRow == dram_pkt->row) { //fr part
            DPRINTF(DRAM, "row buffer hit\n");
            writeQueue.erase(i);
            writeQueue.push_front(dram_pkt);
	    batchedInsts--;
            return;
        }
    }

    // no row buffer hit found. we are going with the prioPkt option
    DRAMPacket* prio_pkt = *prioPkt;
    writeQueue.erase(prioPkt);
    writeQueue.push_front(prio_pkt);

    //done. decrement the number of batched instructions
    batchedInsts--;
}

void
SimpleDRAM::parbsCheckForBatch() {
    if (batchedInsts > 0) return;
 
 
    //initialize book-keeping arrays
    int totalBanks = ranksPerChannel * banksPerRank;
    int totalBatchedRequests[numOfThreads];
    int maxPerBank[numOfThreads];
    int perBankRequests[numOfThreads][totalBanks];

    for (int i=0; i<numOfThreads; i++) {
	totalBatchedRequests[i] = 0;
	maxPerBank[i]=0;
	for (int j=0; j<totalBanks; j++) perBankRequests[i][j] = 0;
    }
 
    //create batch and collect info
    //all outstanding requests will be batched
    for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
	DRAMPacket* dram_pkt = *i;
	dram_pkt->batched = true;
	batchedInsts ++;

	int bankId = dram_pkt->bank;
	int threadId = dram_pkt->thread; // some are -1
	if (threadId == -1) continue;

	totalBatchedRequests[threadId]++;
	perBankRequests[threadId][bankId]++;
	if (perBankRequests[threadId][bankId] > maxPerBank[threadId]) {
	    maxPerBank[threadId] = perBankRequests[threadId][bankId];
	}
    }
    for (auto i = writeQueue.begin(); i != writeQueue.end() ; ++i) {
        DRAMPacket* dram_pkt = *i;
	dram_pkt->batched = true;
	batchedInsts ++;

	int bankId = dram_pkt->bank;
        int threadId = dram_pkt->thread; // some are -1
        if (threadId == -1) continue;

        totalBatchedRequests[threadId]++;
        perBankRequests[threadId][bankId]++;
        if (perBankRequests[threadId][bankId] > maxPerBank[threadId]) {
            maxPerBank[threadId] = perBankRequests[threadId][bankId];
        }
    }
    //batch created and info collected

    //generate thread ranking
    threadRank.clear();
    while (true) {
	int min = 1000000; //hack but should be fine
	int minId = -1;

	for (int i=0; i<numOfThreads; i++) {
	    if (maxPerBank[i] == 0) continue;
	    if (maxPerBank[i] < min) {
		min = maxPerBank[i];
		minId = i;
	    }
	    else if (maxPerBank[i] == min) { // we need the tie-breaker
		if (totalBatchedRequests[i] < totalBatchedRequests[minId]) {
		    minId = i; // no need to change min since they are equal
		}
	    }
	}
    
	if (minId != -1) { //found min
	    threadRank.push_back(minId);
	    maxPerBank[minId] = 0;
	}
	else {
	    break;
	}
    }
    //done! 
}

void
SimpleDRAM::tcmNextWrite() {
    int chosen;
    DRAMPacket* dram_pkt;
    bool foundInLat = false;

    //Check if sampling is needed
    if (curTick() - lastSampleTime >= samplingThreshold) {
        tcmSampling();
        tcmShuffle(); //Let's assume that the sampling threshold is also the shuffling threshold
        lastSampleTime = curTick();
    }

    //Check if a quantum ended
    if (curTick() - lastQuantumTime >= quantumThreshold) {
        tcmQuantum();
	lastQuantumTime = curTick();
    }

    //prioritize lat sensitive cluster
    if (latSensWrite.size() != 0) {
        chosen = tcmChooseFromLatCluster(false);
        dram_pkt = latSensWrite[chosen];
        foundInLat = true;
    }
    else { //if nothing in the lat sensitive, schedule something from bw
        chosen = tcmChooseFromBwCluster(false); //True -> Looking for reads
        dram_pkt = bwSensWrite[chosen];
        foundInLat = false;
    }

    //Keep per-access statistics
    tcmPerAccessStats(dram_pkt);

    //find in writeQueue
    auto i = std::find (writeQueue.begin(), writeQueue.end(), dram_pkt);
    if (i != writeQueue.end()) {
        //Found our next packet. Move to the front of the queue
        writeQueue.erase(i);
        writeQueue.push_front(dram_pkt);
        //Delete from latSensRead/Write
        if (foundInLat) latSensWrite.erase(latSensWrite.begin() + chosen);
        else bwSensWrite.erase (bwSensWrite.begin() + chosen);
    }
    else {
        panic("Error: transaction found in lat sensitive but not found in transaction Queue");
    }
}

void 
SimpleDRAM::tcmNextRead() {
    int chosen;
    DRAMPacket* dram_pkt;
    bool foundInLat = false;

    //Check if sampling is needed
    if (curTick() - lastSampleTime >= samplingThreshold) {
	tcmSampling();
	tcmShuffle(); //Let's assume that the sampling threshold is also the shuffling threshold
	lastSampleTime = curTick();
    }

    //Check if a quantum ended
    if (curTick() - lastQuantumTime >= quantumThreshold) {
	tcmQuantum();
	lastQuantumTime = curTick();
    }

    //Select the actual request
    //prioritize lat sensitive cluster
    if (latSensRead.size() != 0) {
	chosen = tcmChooseFromLatCluster(true); //True -> Looking for reads
	dram_pkt = latSensRead[chosen];
	foundInLat = true;
    }
    else { //if nothing in the lat sensitive, schedule something from bw
	chosen = tcmChooseFromBwCluster(true); //True -> Looking for reads
	dram_pkt = bwSensRead[chosen];
	foundInLat = false;
    }

    //Keep per-access statistics
    tcmPerAccessStats(dram_pkt);

    //find in readQueue
    auto i = std::find (readQueue.begin(), readQueue.end(), dram_pkt);
    if (i != readQueue.end()) {
	//Found our next packet. Move to the front of the queue
	readQueue.erase(i);
	readQueue.push_front(dram_pkt);
	//Delete from latSensRead/Write
	if (foundInLat) {
		latSensRead.erase(latSensRead.begin() + chosen);
	}
	else bwSensRead.erase (bwSensRead.begin() + chosen);
    }
    else {
	panic("Error: transaction found in lat sensitive but not found in transaction Queue");
    }


}

void 
SimpleDRAM::tcmPerAccessStats(DRAMPacket* dram_pkt) {
    int threadId = dram_pkt->thread;
    if (threadId == -1) return;

    int row = dram_pkt->row;
    Bank& bank = dram_pkt->bank_ref;

    if (bank.shadowRow[threadId] == row) { //Shadow row buffer hit
	shadowRBHitCount[threadId] ++;
    }

    //Update ghost bank status
    bank.shadowRow[threadId] = row;
    //Prodromou: I don't see any pre-charging activity in the code 
    //so I guess I don't have to worry about closing ghost pages
}

void 
SimpleDRAM::tcmSampling() {

    samplesTaken++;

    //update blp
    //create a set per thread that holds the ids of the banks accessed
    vector<set<int> > bankRequests (numOfThreads, set<int>());
    //start with the read queue
    for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
	DRAMPacket* dram_pkt = *i;
	int bankId = dram_pkt->bank;
        int threadId = dram_pkt->thread; // some are -1
        if (threadId == -1) continue;

	bankRequests[threadId].insert(bankId);
    }
    //repeat for the write queue
    for (auto i = writeQueue.begin(); i != writeQueue.end() ; ++i) {
        DRAMPacket* dram_pkt = *i;
        int bankId = dram_pkt->bank;
        int threadId = dram_pkt->thread; // some are -1
        if (threadId == -1) continue;

        bankRequests[threadId].insert(bankId);
    }
    for (int i=0; i<numOfThreads; i++) {
	BLP[i] = bankRequests[i].size();
    }
    //blp updated
}

void 
SimpleDRAM::tcmShuffle() {
    //Make this better
    if (shuffleState >= 0) {
	int i= shuffleState; // READABILITY PURPOSES ONLY
	//left shift (i, N-1)
	//Create temp vector with the indices of interest
	vector<int> temp (sortedNiceness.begin()+i, sortedNiceness.end());
	//delete indices of interest from original vector
	sortedNiceness.erase(sortedNiceness.begin()+i, sortedNiceness.end());
	//shift temp to the left
	int t1 = temp[0];
	temp.erase(temp.begin());
	temp.push_back(t1);
	//Add temp back into the original vector
	sortedNiceness.insert( sortedNiceness.end(), temp.begin(), temp.end() );
    }
    else if (shuffleState == -1) {
	int i=numOfThreads-1;
	//left shift (N-1, N-1)
	//Create temp vector with the indices of interest
	vector<int> temp (sortedNiceness.begin()+i, sortedNiceness.end());
	//delete indices of interest from original vector
	sortedNiceness.erase(sortedNiceness.begin()+i, sortedNiceness.end());
	//shift temp to the left
	int t1 = temp[0];
	temp.erase(temp.begin());
	temp.push_back(t1);
	//Add temp back into the original vector
	sortedNiceness.insert( sortedNiceness.end(), temp.begin(), temp.end() );
    }
    else {
	int i = (shuffleState * (-1)) - 2; //ABS HACK
	//right shift (i, N-1)
	vector<int> temp (sortedNiceness.begin()+i, sortedNiceness.end());
	//delete indices of interest from original vector
	sortedNiceness.erase(sortedNiceness.begin()+i, sortedNiceness.end()-1);
	//shift temp to the left
	int t1 = temp.back();
	temp.erase(temp.end()-1);
	temp.insert(temp.begin(), t1);
	//Add temp back into the original vector
	sortedNiceness.insert( sortedNiceness.end(), temp.begin(), temp.end() );
    }

    shuffleState --;
    if (shuffleState == (-1-numOfThreads)) shuffleState = numOfThreads-1; //NEG HACK
}

void 
SimpleDRAM::tcmQuantum() {
    //Calculate avg BW of all threads
    float totalBW = 0;
    for (int i=0; i<numOfThreads; i++) totalBW += BLP[i];
    totalBW = totalBW / samplesTaken;

    //Get MPKI per thread
    for (int i=0; i<numOfThreads; i++) {
	long long instCount;
	instCount = ((system()->getThreadContext(i))->getCpuPtr())->getCommitedInsts(i);
	mpkiPerThread[i] = (float)(reqPerThread[i]*1000) / instCount;
    }

    //Categorize Threads
    float sumBW = 0;
    for (int i=0; i<numOfThreads; i++) {
	//Find smallest MPKI
	int minMPKI = 100000; //HACK
	int minMpkiIndex = -1;
	for (int j=0; j<mpkiPerThread.size(); j++) {
	    if (mpkiPerThread[j] < minMPKI) {
		minMPKI = mpkiPerThread[j];
		minMpkiIndex = j;
	    }
	}

	// Reset the thread's mpki
	mpkiPerThread[minMpkiIndex] = 10000; //HACK

	sumBW = sumBW + ((float)(BLP[minMpkiIndex]) / samplesTaken);
	if (sumBW <= clusterThresh * totalBW) {
	    threadCluster[minMpkiIndex] = true;
	}
	else {
	    break;
	}
    }

    //Calculate thread niceless
    for (int i=0; i<numOfThreads; i++) {
	int Bi = 1;
	int Ri = 1;
	for (int j=0; j<numOfThreads; j++) {
	    //Only compare against threads in the BW sensitive cluster
	    if ( ! threadCluster[j]) {
		if (BLP[j] > BLP[i]) Bi++;
		if (shadowRBHitCount[j] > shadowRBHitCount[i]) Ri++;
	    }
	}
	threadNiceness[i] = Bi - Ri;
    }

    //Sort threads based on niceness
    while(true) {
	if (threadNiceness.size() == 0) break;
	int minIndex = -1;
	int min_temp = 0;
	for (int i=0; i<threadNiceness.size(); i++) {
	    if ((threadNiceness[i] < min_temp) || (i == 0)) {
		min_temp = threadNiceness[i];
		minIndex = i;
	    }
	}
	sortedNiceness.push_back(threadNiceness[minIndex]);
	threadNiceness.erase(threadNiceness.begin() + minIndex);
    }

    //Prodromou: I need to clear the Clusters and re-input all the 
    //requests according to the new ranking

    //Some metricks to verify correct reassignment
    int readSize = latSensRead.size() + bwSensRead.size();
    int writeSize = latSensWrite.size() + bwSensWrite.size();
 
    if ((readSize != readQueue.size()) || (writeSize != writeQueue.size())) {
	panic ("Queue size mismatch at the beginning of tcmQuantum ");
    }

    latSensRead.clear();
    bwSensRead.clear();
    latSensWrite.clear();
    bwSensWrite.clear();

    for (auto i = readQueue.begin(); i!= readQueue.end(); ++i) { 
	DRAMPacket* dram_pkt = *i;
	int threadId = dram_pkt->thread;
	if (threadId == -1) { //Just store it in the BW sensitive cluster
	    bwSensRead.push_back(dram_pkt);
	}
	else {
	    if (threadCluster[threadId]) {
		latSensRead.push_back(dram_pkt);
	    }
	    else bwSensRead.push_back(dram_pkt);
	} 
    }
    for (auto i = writeQueue.begin(); i!= writeQueue.end(); ++i) { 
	DRAMPacket* dram_pkt = *i;
	int threadId = dram_pkt->thread;
	if (threadId == -1) { //Just store it in the BW sensitive cluster
	    bwSensWrite.push_back(dram_pkt);
	}
	else {
	    if (threadCluster[threadId]) {
		latSensWrite.push_back(dram_pkt);
	    }
	    else bwSensWrite.push_back(dram_pkt);
	} 
    }

    readSize = latSensRead.size() + bwSensRead.size();
    writeSize = latSensWrite.size() + bwSensWrite.size();

    if ((readSize != readQueue.size()) || (writeSize != writeQueue.size())) {
        panic ("Queue size mismatch at the end of tcmQuantum ");
    }

    //Reset stats counters
    for (int i=0; i<numOfThreads; i++) {
	shadowRBHitCount[i] = 0;
	BLP[i] = 0;
    }
    samplesTaken = 0;
}

int 
SimpleDRAM::tcmChooseFromBwCluster(bool isRead) {
    int chosenIndex = 0;

    //Choose which queue we'll work on
    std::deque<DRAMPacket*> queue;
    if (isRead) queue = bwSensRead;
    else queue = bwSensWrite;

    // i=1 is not a typo. i=0 is already chosen
    for (int i=1; i<queue.size(); i++) {
	int threadId = queue[i]->thread;
	if (threadId == queue[chosenIndex]->thread) { //Same thread. Tie breaker
	    //Row Buffer Hit
	    Bank& bank = queue[chosenIndex]->bank_ref;
	    int row = queue[chosenIndex]->row;
	    Bank& bank2 = queue[i]->bank_ref;
	    int row2 = queue[i]->row;
	    if (bank.openRow == row) ; //Nothing to do here
	    else if (bank2.openRow == row2) chosenIndex = i;
	    else ; //Age tie breaker. Chronologically sorted queues...
	}
	else { //Different threads. Check their priorities
	    vector<int>::iterator chosenIt, it2;
	    chosenIt = std::find (sortedNiceness.begin(), sortedNiceness.end(), queue[chosenIndex]->thread);
	    it2 = std::find (sortedNiceness.begin(), sortedNiceness.end(), threadId);
	    if (it2 == sortedNiceness.end()) ; // No match found. Do nothing
	    else if (it2 > chosenIt) { //thread has higher priority
		chosenIndex = i;
	    }
	}
    }
    return chosenIndex;
}

int
SimpleDRAM::tcmChooseFromLatCluster(bool isRead) {
    
    int chosenIndex = -1;

    //Choose which queue we'll work on
    std::deque<DRAMPacket*> queue;
    if (isRead) queue = latSensRead;
    else queue = latSensWrite;
   
    //Lower MPKI
    float minMpki = 0;
    for (auto i = queue.begin(); i<queue.end(); i++) {
	DRAMPacket* dram_pkt = *i;
	int threadId = dram_pkt->thread;
	if ((mpkiPerThread[threadId] < minMpki) || (i == queue.begin())) {
            minMpki = mpkiPerThread[threadId];
            chosenIndex = i - queue.begin(); //Returns the index
        }
	else if (mpkiPerThread[threadId] == minMpki) { //Tie breaker: RB Hit

	    Bank& bank = queue[chosenIndex]->bank_ref;
	    int row = queue[chosenIndex]->row;

	    Bank& bank2 = dram_pkt->bank_ref;
	    int row2 = dram_pkt->row;

	    if (bank.openRow == row) ; //No need to do anything else
	    else if (bank2.openRow == row2) chosenIndex = i - queue.begin();
	    else { //No Row hits found. Tie breaker is age
		//No need to do anything since the already-selected 
		//index is also first in time
		;
	    }
	}
    }

    return chosenIndex;
}

void SimpleDRAM::mthreadsNextRead() {
    //Prodromou: Find the next read request and place it 
    //in front of the queue
    int tableDimension = 20;
    
    std::vector<int> NPerThread(numOfThreads, 9999);
    std::vector<int> threadPriority;

    // For each thread
    for (int threadNum=0; threadNum<numOfThreads; threadNum++) {
	//Generate the table based on current RTT
	//Should RTT be per thread?
	float RTT = mthreadsAvgMemAccTimePerThread[threadNum].value();
	mthreadsLookupTable = new LookupTable(tableDimension, RTT);

	//Get the thread's search terms -- reads/time & avgQLen (X & Q)
	float X = mthreadsReqsPerThread[threadNum].value();
	float Q = mthreadsAvgQLenPerThread[threadNum].value();
	
	//Query for N and Z
	pair<int,int> nAndZ = mthreadsLookupTable->searchFor(X,Q);
	NPerThread[threadNum] = nAndZ.first;
	perThreadNPdf[threadNum][nAndZ.first]++;

	//Delete LookupTable
	delete(mthreadsLookupTable);
    }

    //Sort threads
    for (int prio=0; prio<numOfThreads; prio++) {
	int minN = 10000;
	int minthread = -1;
	for (int thread=0; thread<numOfThreads; thread++) {
	    if (NPerThread[thread] < minN) {
		minN = NPerThread[thread];
		minthread = thread;
	    }
	}
	threadPriority.push_back(minthread);
	NPerThread[minthread] = 100000; // Make sure it's not selected again
    }

    //Search for RBHs in prioritized fashion
    for (int j=0; j<numOfThreads; j++) {
	int threadNum = threadPriority[j];
	for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
	    DRAMPacket* dram_pkt = *i;
	    const Bank& bank = dram_pkt->bank_ref;
	    int currentThread = dram_pkt->thread;

	    if (currentThread == threadNum) {
		// check if it is a row hit
		if (bank.openRow == dram_pkt->row) { //fr part
		    DPRINTF(DRAM, "row buffer hit\n");
		    readQueue.erase(i);
		    readQueue.push_front(dram_pkt);
		    return;
		}
	    }
	}
    }

    //No RBHs found. Choose the first packet of the thread 
    //with the highest priority
    for (int j=0; j<numOfThreads; j++) {
	int threadNum = threadPriority[j];
	for (auto i = readQueue.begin(); i != readQueue.end() ; ++i) {
	    DRAMPacket* dram_pkt = *i;
	    int currentThread = dram_pkt->thread;

	    if (currentThread == threadNum) {
		    readQueue.erase(i);
		    readQueue.push_front(dram_pkt);
		    return;
	    }
	}
    }
}

void SimpleDRAM::mthreadsNextWrite() {
    //Prodromou: Find the next write request and place it 
    // in front of the queue

    // Do nothing for now?
}

void
SimpleDRAM::accessAndRespond(PacketPtr pkt, Tick static_latency)
{
    DPRINTF(DRAM, "Responding to Address %lld.. ",pkt->getAddr());

    bool needsResponse = pkt->needsResponse();
    // do the actual memory access which also turns the packet into a
    // response
    access(pkt);

    // turn packet around to go back to requester if response expected
    if (needsResponse) {
        // access already turned the packet into a response
        assert(pkt->isResponse());

        // @todo someone should pay for this
        pkt->busFirstWordDelay = pkt->busLastWordDelay = 0;

        // queue the packet in the response queue to be sent out after
        // the static latency has passed
        port.schedTimingResp(pkt, curTick() + static_latency);
    } else {
        // @todo the packet is going to be deleted, and the DRAMPacket
        // is still having a pointer to it
        pendingDelete.push_back(pkt);
    }

    DPRINTF(DRAM, "Done\n");

    return;
}

pair<Tick, Tick>
SimpleDRAM::estimateLatency(DRAMPacket* dram_pkt, Tick inTime)
{
    // If a request reaches a bank at tick 'inTime', how much time
    // *after* that does it take to finish the request, depending
    // on bank status and page open policy. Note that this method
    // considers only the time taken for the actual read or write
    // to complete, NOT any additional time thereafter for tRAS or
    // tRP.
    Tick accLat = 0;
    Tick bankLat = 0;
    rowHitFlag = false;

    const Bank& bank = dram_pkt->bank_ref;
    if (pageMgmt == Enums::open) { // open-page policy
        if (bank.openRow == dram_pkt->row) {
            // When we have a row-buffer hit,
            // we don't care about tRAS having expired or not,
            // but do care about bank being free for access
            rowHitFlag = true;

            if (bank.freeAt < inTime) {
               // CAS latency only
               accLat += tCL;
               bankLat += tCL;
            } else {
                accLat += 0;
                bankLat += 0;
            }

        } else {
            // Row-buffer miss, need to close existing row
            // once tRAS has expired, then open the new one,
            // then add cas latency.
            Tick freeTime = std::max(bank.tRASDoneAt, bank.freeAt);

            if (freeTime > inTime)
               accLat += freeTime - inTime;

            accLat += tRP + tRCD + tCL;
            bankLat += tRP + tRCD + tCL;
        }
    } else if (pageMgmt == Enums::close) {
        // With a close page policy, no notion of
        // bank.tRASDoneAt
        if (bank.freeAt > inTime)
            accLat += bank.freeAt - inTime;

        // page already closed, simply open the row, and
        // add cas latency
        accLat += tRCD + tCL;
        bankLat += tRCD + tCL;
    } else
        panic("No page management policy chosen\n");

    DPRINTF(DRAM, "Returning < %lld, %lld > from estimateLatency()\n",
            bankLat, accLat);

    return make_pair(bankLat, accLat);
}

void
SimpleDRAM::processNextReqEvent()
{
    scheduleNextReq();
}

void
SimpleDRAM::recordActivate(Tick act_tick)
{
    assert(actTicks.size() == activationLimit);

    DPRINTF(DRAM, "Activate at tick %d\n", act_tick);

    // if the activation limit is disabled then we are done
    if (actTicks.empty())
        return;

    // sanity check
    if (actTicks.back() && (act_tick - actTicks.back()) < tXAW) {
        // @todo For now, stick with a warning
        warn("Got %d activates in window %d (%d - %d) which is smaller "
             "than %d\n", activationLimit, act_tick - actTicks.back(),
             act_tick, actTicks.back(), tXAW);
    }

    // shift the times used for the book keeping, the last element
    // (highest index) is the oldest one and hence the lowest value
    actTicks.pop_back();

    // record an new activation (in the future)
    actTicks.push_front(act_tick);

    // cannot activate more than X times in time window tXAW, push the
    // next one (the X + 1'st activate) to be tXAW away from the
    // oldest in our window of X
    if (actTicks.back() && (act_tick - actTicks.back()) < tXAW) {
        DPRINTF(DRAM, "Enforcing tXAW with X = %d, next activate no earlier "
                "than %d\n", activationLimit, actTicks.back() + tXAW);
        for(int i = 0; i < ranksPerChannel; i++)
            for(int j = 0; j < banksPerRank; j++)
                // next activate must not happen before end of window
                banks[i][j].freeAt = std::max(banks[i][j].freeAt,
                                              actTicks.back() + tXAW);
    }
}

void
SimpleDRAM::doDRAMAccess(DRAMPacket* dram_pkt)
{

    DPRINTF(DRAM, "Timing access to addr %lld, rank/bank/row %d %d %d\n",
            dram_pkt->addr, dram_pkt->rank, dram_pkt->bank, dram_pkt->row);

    // estimate the bank and access latency
    pair<Tick, Tick> lat = estimateLatency(dram_pkt, curTick());
    Tick bankLat = lat.first;
    Tick accessLat = lat.second;

    // This request was woken up at this time based on a prior call
    // to estimateLatency(). However, between then and now, both the
    // accessLatency and/or busBusyUntil may have changed. We need
    // to correct for that.

    Tick addDelay = (curTick() + accessLat < busBusyUntil) ?
        busBusyUntil - (curTick() + accessLat) : 0;

    Bank& bank = dram_pkt->bank_ref;

    // Update bank state
    if (pageMgmt == Enums::open) {
        bank.openRow = dram_pkt->row;
        bank.freeAt = curTick() + addDelay + accessLat;
        bank.bytesAccessed += burstSize;

        // If you activated a new row do to this access, the next access
        // will have to respect tRAS for this bank. Assume tRAS ~= 3 * tRP.
        // Also need to account for t_XAW
        if (!rowHitFlag) {
            bank.tRASDoneAt = bank.freeAt + tRP;
            recordActivate(bank.freeAt - tCL - tRCD); //since this is open page,
                                                      //no tRP by default
            // sample the number of bytes accessed and reset it as
            // we are now closing this row
            bytesPerActivate.sample(bank.bytesAccessed);
            bank.bytesAccessed = 0;
        }
    } else if (pageMgmt == Enums::close) { // accounting for tRAS also
        // assuming that tRAS ~= 3 * tRP, and tRC ~= 4 * tRP, as is common
        // (refer Jacob/Ng/Wang and Micron datasheets)
        bank.freeAt = curTick() + addDelay + accessLat + tRP + tRP;
        recordActivate(bank.freeAt - tRP - tRP - tCL - tRCD); //essentially (freeAt - tRC)
        DPRINTF(DRAM,"doDRAMAccess::bank.freeAt is %lld\n",bank.freeAt);
        bytesPerActivate.sample(burstSize);
    } else
        panic("No page management policy chosen\n");

    // Update request parameters
    dram_pkt->readyTime = curTick() + addDelay + accessLat + tBURST;

    // Prodromou: Slow down accesses when flag is set
    if (slowdownAccesses) dram_pkt->readyTime += perAccessSlowdown;

    DPRINTF(DRAM, "Req %lld: curtick is %lld accessLat is %d " \
                  "readytime is %lld busbusyuntil is %lld. " \
                  "Scheduling at readyTime\n", dram_pkt->addr,
                   curTick(), accessLat, dram_pkt->readyTime, busBusyUntil);

    // Make sure requests are not overlapping on the databus
    assert (dram_pkt->readyTime - busBusyUntil >= tBURST);

    // Update bus state
    busBusyUntil = dram_pkt->readyTime;

    DPRINTF(DRAM,"Access time is %lld\n",
            dram_pkt->readyTime - dram_pkt->entryTime);

    // Update stats
    totMemAccLat += dram_pkt->readyTime - dram_pkt->entryTime;
    totBankLat += bankLat;
    totBusLat += tBURST;
    totQLat += dram_pkt->readyTime - dram_pkt->entryTime - bankLat - tBURST;

    if (rowHitFlag)
        readRowHits++;

    // At this point we're done dealing with the request
    // It will be moved to a separate response queue with a
    // correct readyTime, and eventually be sent back at that
    //time
    moveToRespQ();

    // The absolute soonest you have to start thinking about the
    // next request is the longest access time that can occur before
    // busBusyUntil. Assuming you need to meet tRAS, then precharge,
    // open a new row, and access, it is ~4*tRCD.


    Tick newTime = (busBusyUntil > 4 * tRCD) ?
                   std::max(busBusyUntil - 4 * tRCD, curTick()) :
                   curTick();

    if (!nextReqEvent.scheduled() && !stopReads){
        schedule(nextReqEvent, newTime);
    } else {
        if (newTime < nextReqEvent.when())
            reschedule(nextReqEvent, newTime);
    }


}

void
SimpleDRAM::moveToRespQ()
{
    // Remove from read queue
    DRAMPacket* dram_pkt = readQueue.front();
    readQueue.pop_front();
/*
    //Prodromou: Remove from the cluster queue as well
    int threadId = dram_pkt->thread;
    DRAMPacket* temp = NULL;
    if (memSchedPolicy == Enums::tcm) {
	if (threadId != -1) {
	    if (threadCluster[threadId]) {
		temp = latSensRead.front();
		latSensRead.pop_front();
	    }
	    else {
		temp = bwSensRead.front();
		bwSensRead.pop_front();
	    }
	    if (temp != dram_pkt) panic ("Something went wrong in removing the packet from the cluster");
	}
    }
*/

    // sanity check
    assert(dram_pkt->size <= burstSize);

    // Insert into response queue sorted by readyTime
    // It will be sent back to the requestor at its
    // readyTime
    if (respQueue.empty()) {
        respQueue.push_front(dram_pkt);
        assert(!respondEvent.scheduled());
        assert(dram_pkt->readyTime >= curTick());
        schedule(respondEvent, dram_pkt->readyTime);
    } else {
        bool done = false;
        auto i = respQueue.begin();
        while (!done && i != respQueue.end()) {
            if ((*i)->readyTime > dram_pkt->readyTime) {
                respQueue.insert(i, dram_pkt);
                done = true;
            }
            ++i;
        }

        if (!done)
            respQueue.push_back(dram_pkt);

        assert(respondEvent.scheduled());

        if (respQueue.front()->readyTime < respondEvent.when()) {
            assert(respQueue.front()->readyTime >= curTick());
            reschedule(respondEvent, respQueue.front()->readyTime);
        }
    }
}

void
SimpleDRAM::scheduleNextReq()
{
    DPRINTF(DRAM, "Reached scheduleNextReq()\n");

    // Figure out which read request goes next, and move it to the
    // front of the read queue
    if (!chooseNextRead()) {
        // In the case there is no read request to go next, see if we
        // are asked to drain, and if so trigger writes, this also
        // ensures that if we hit the write limit we will do this
        // multiple times until we are completely drained
        if (drainManager && !writeQueue.empty() && !writeEvent.scheduled())
            triggerWrites();
    } else {
        doDRAMAccess(readQueue.front());
    }
}

Tick
SimpleDRAM::maxBankFreeAt() const
{
    Tick banksFree = 0;

    for(int i = 0; i < ranksPerChannel; i++)
        for(int j = 0; j < banksPerRank; j++)
            banksFree = std::max(banks[i][j].freeAt, banksFree);

    return banksFree;
}

void
SimpleDRAM::processRefreshEvent()
{
    DPRINTF(DRAM, "Refreshing at tick %ld\n", curTick());

    Tick banksFree = std::max(curTick(), maxBankFreeAt()) + tRFC;

    for(int i = 0; i < ranksPerChannel; i++)
        for(int j = 0; j < banksPerRank; j++)
            banks[i][j].freeAt = banksFree;

    schedule(refreshEvent, curTick() + tREFI);
}

void
SimpleDRAM::regStats()
{
    using namespace Stats;

    AbstractMemory::regStats();

    readReqs
        .name(name() + ".readReqs")
        .desc("Total number of read requests accepted by DRAM controller");

    writeReqs
        .name(name() + ".writeReqs")
        .desc("Total number of write requests accepted by DRAM controller");

    readBursts
        .name(name() + ".readBursts")
        .desc("Total number of DRAM read bursts. "
              "Each DRAM read request translates to either one or multiple "
              "DRAM read bursts");

    writeBursts
        .name(name() + ".writeBursts")
        .desc("Total number of DRAM write bursts. "
              "Each DRAM write request translates to either one or multiple "
              "DRAM write bursts");

    servicedByWrQ
        .name(name() + ".servicedByWrQ")
        .desc("Number of DRAM read bursts serviced by write Q");

    neitherReadNorWrite
        .name(name() + ".neitherReadNorWrite")
        .desc("Reqs where no action is needed");

    perBankRdReqs
        .init(banksPerRank * ranksPerChannel)
        .name(name() + ".perBankRdReqs")
        .desc("Track reads on a per bank basis");

    perBankWrReqs
        .init(banksPerRank * ranksPerChannel)
        .name(name() + ".perBankWrReqs")
        .desc("Track writes on a per bank basis");

    avgRdQLen
        .name(name() + ".avgRdQLen")
        .desc("Average read queue length over time")
        .precision(2);

    avgWrQLen
        .name(name() + ".avgWrQLen")
        .desc("Average write queue length over time")
        .precision(2);

    totQLat
        .name(name() + ".totQLat")
        .desc("Total cycles spent in queuing delays");

    totBankLat
        .name(name() + ".totBankLat")
        .desc("Total cycles spent in bank access");

    totBusLat
        .name(name() + ".totBusLat")
        .desc("Total cycles spent in databus access");

    totMemAccLat
        .name(name() + ".totMemAccLat")
        .desc("Sum of mem lat for all requests");

    avgQLat
        .name(name() + ".avgQLat")
        .desc("Average queueing delay per request")
        .precision(2);

    avgQLat = totQLat / (readBursts - servicedByWrQ);

    avgBankLat
        .name(name() + ".avgBankLat")
        .desc("Average bank access latency per request")
        .precision(2);

    avgBankLat = totBankLat / (readBursts - servicedByWrQ);

    avgBusLat
        .name(name() + ".avgBusLat")
        .desc("Average bus latency per request")
        .precision(2);

    avgBusLat = totBusLat / (readBursts - servicedByWrQ);

    avgMemAccLat
        .name(name() + ".avgMemAccLat")
        .desc("Average memory access latency")
        .precision(2);

    avgMemAccLat = totMemAccLat / (readBursts - servicedByWrQ);

    numRdRetry
        .name(name() + ".numRdRetry")
        .desc("Number of times rd buffer was full causing retry");

    numWrRetry
        .name(name() + ".numWrRetry")
        .desc("Number of times wr buffer was full causing retry");

    readRowHits
        .name(name() + ".readRowHits")
        .desc("Number of row buffer hits during reads");

    writeRowHits
        .name(name() + ".writeRowHits")
        .desc("Number of row buffer hits during writes");

    readRowHitRate
        .name(name() + ".readRowHitRate")
        .desc("Row buffer hit rate for reads")
        .precision(2);

    readRowHitRate = (readRowHits / (readBursts - servicedByWrQ)) * 100;

    writeRowHitRate
        .name(name() + ".writeRowHitRate")
        .desc("Row buffer hit rate for writes")
        .precision(2);

    writeRowHitRate = (writeRowHits / writeBursts) * 100;

    readPktSize
        .init(ceilLog2(burstSize) + 1)
        .name(name() + ".readPktSize")
        .desc("Categorize read packet sizes");

     writePktSize
        .init(ceilLog2(burstSize) + 1)
        .name(name() + ".writePktSize")
        .desc("Categorize write packet sizes");

     rdQLenPdf
        .init(readBufferSize)
        .name(name() + ".rdQLenPdf")
        .desc("What read queue length does an incoming req see");

     wrQLenPdf
        .init(writeBufferSize)
        .name(name() + ".wrQLenPdf")
        .desc("What write queue length does an incoming req see");

     bytesPerActivate
         .init(rowBufferSize)
         .name(name() + ".bytesPerActivate")
         .desc("Bytes accessed per row activation")
         .flags(nozero);

    bytesRead
        .name(name() + ".bytesRead")
        .desc("Total number of bytes read from memory");

    bytesWritten
        .name(name() + ".bytesWritten")
        .desc("Total number of bytes written to memory");

    bytesConsumedRd
        .name(name() + ".bytesConsumedRd")
        .desc("bytesRead derated as per pkt->getSize()");

    bytesConsumedWr
        .name(name() + ".bytesConsumedWr")
        .desc("bytesWritten derated as per pkt->getSize()");

    avgRdBW
        .name(name() + ".avgRdBW")
        .desc("Average achieved read bandwidth in MB/s")
        .precision(2);

    avgRdBW = (bytesRead / 1000000) / simSeconds;

    avgWrBW
        .name(name() + ".avgWrBW")
        .desc("Average achieved write bandwidth in MB/s")
        .precision(2);

    avgWrBW = (bytesWritten / 1000000) / simSeconds;

    avgConsumedRdBW
        .name(name() + ".avgConsumedRdBW")
        .desc("Average consumed read bandwidth in MB/s")
        .precision(2);

    avgConsumedRdBW = (bytesConsumedRd / 1000000) / simSeconds;

    avgConsumedWrBW
        .name(name() + ".avgConsumedWrBW")
        .desc("Average consumed write bandwidth in MB/s")
        .precision(2);

    avgConsumedWrBW = (bytesConsumedWr / 1000000) / simSeconds;

    peakBW
        .name(name() + ".peakBW")
        .desc("Theoretical peak bandwidth in MB/s")
        .precision(2);

    peakBW = (SimClock::Frequency / tBURST) * burstSize / 1000000;

    busUtil
        .name(name() + ".busUtil")
        .desc("Data bus utilization in percentage")
        .precision(2);

    busUtil = (avgRdBW + avgWrBW) / peakBW * 100;

    totGap
        .name(name() + ".totGap")
        .desc("Total gap between requests");

    avgGap
        .name(name() + ".avgGap")
        .desc("Average gap between requests")
        .precision(2);

    avgGap = totGap / (readReqs + writeReqs);

    Lamda
	.name(name() + ".Lamda")
	.desc("The observed arrival ratio at the MC")
	.precision(2);

    //Prodromou: HACK: Hardcoded value
    Lamda = (readReqs + writeReqs) / (simTicks / 1000);

    Mi
	.name(name() + ".Mi")
	.desc("The observes serviced rate at the MC")
	.precision(2);

    //Prodromou: HACK: Hardcoded value
    Mi = (readReqs + writeReqs - constant(readQueue.size()) - constant(respQueue.size()) - constant(writeQueue.size())) / (simTicks / 1000);

    thinkingTime
	.name(name() + ".thinkingTime")
	.desc("Thinking time of clients when the MC is modeled as a closed Queuing network")
	.precision(2);
    
    //Prodromou: Can't use the word throughput (very common). Using Mi
    //Prodromou: / 1000 to get ns. HACK: Hardcoded value
    thinkingTime = ((avgRdQLen + avgWrQLen) - (Lamda * (avgMemAccLat / 1000))) / (Lamda + Mi);

    microThreads
	.name(name() + ".microThreads")
	.desc("The estimated number of microThreads")
	.precision (2);

    microThreads = (avgRdQLen + avgWrQLen) - (Mi * thinkingTime);

    mthreadsAvgQLenPerThread
	.init(numOfThreads)
	.name(name() + ".avgRdQLenPerThread")
	.precision(6);

    mthreadsReqsPerThread
	.init (numOfThreads)
	.name(name() + ".avgNumOfReqsPerThread")
	.precision(6);

    mthreadsAvgMemAccTimePerThread
	.init (numOfThreads)
	.name (name() + ".avgMemAccLatPerThread")
	.precision(2);

    perThreadNPdf
	.init(numOfThreads, 50)
	.name(name() + ".perThreadNPdf")
	.desc("Pdf distribution of the number of customers per thread");

}

void
SimpleDRAM::recvFunctional(PacketPtr pkt)
{
    // rely on the abstract memory
    functionalAccess(pkt);
}

BaseSlavePort&
SimpleDRAM::getSlavePort(const string &if_name, PortID idx)
{
    if (if_name != "port") {
        return MemObject::getSlavePort(if_name, idx);
    } else {
        return port;
    }
}

unsigned int
SimpleDRAM::drain(DrainManager *dm)
{
    unsigned int count = port.drain(dm);

    // if there is anything in any of our internal queues, keep track
    // of that as well
    if (!(writeQueue.empty() && readQueue.empty() &&
          respQueue.empty())) {
        DPRINTF(Drain, "DRAM controller not drained, write: %d, read: %d,"
                " resp: %d\n", writeQueue.size(), readQueue.size(),
                respQueue.size());
        ++count;
        drainManager = dm;
        // the only part that is not drained automatically over time
        // is the write queue, thus trigger writes if there are any
        // waiting and no reads waiting, otherwise wait until the
        // reads are done
        if (readQueue.empty() && !writeQueue.empty() &&
            !writeEvent.scheduled())
            triggerWrites();
    }

    if (count)
        setDrainState(Drainable::Draining);
    else
        setDrainState(Drainable::Drained);
    return count;
}

//Prodromou: Implementation of LookupTable class
SimpleDRAM::LookupTable::LookupTable(int dim_limit, float rtt) {
    Rtt = rtt; //For mcf_10ns at lat_tolerance/individual_analysis
    dimension_limit = dim_limit;

    //table.reserve(dimension_limit);
    table = vector<vector<LookupTableEntry> >(dimension_limit);

    for (int i=0; i<dimension_limit; i++) {
        //table[i].reserve(dimension_limit);
	table[i] = vector<LookupTableEntry>(dimension_limit);
    }

    //Initialize the lookup table
    for (int i=1; i<dimension_limit; i++) {
        for (int j=1; j<dimension_limit; j++ ) {
            exactMVA(i,j);
        }
    }
}

float
SimpleDRAM::LookupTable::getNStep() {
    return 1;
}

float
SimpleDRAM::LookupTable::getZStep() {
    return 1000;
}

void
SimpleDRAM::LookupTable::exactMVA(int N_dim, int Z_dim) {

    //Translated table dimensions
    table[N_dim][Z_dim].valid = true;

    float X=0; //throughput
    float L=0; // Queue Length

    float N = N_dim * getNStep();
    float Z = Z_dim * getZStep();

    for (int n = 0; n<N; n++) {

        //Set all station delays
        //We only have one so we are using Rtt

        //Set throughput
        //cout <<"X = "<<n<<" / ("<<Z<<" + "<<Rtt<<")"<<endl;
        X = ((float)(n)) / (Z + Rtt);

        //Set Queue length
        L = X * Rtt;
    }

    table[N_dim][Z_dim].throughput = X;
    table[N_dim][Z_dim].Qlen = L;
}

string
SimpleDRAM::LookupTable::printTable() {
    stringstream ss;
    ss.precision(3);
    ss << endl;
    for (int i=0; i<dimension_limit; i++) {
        for (int j=0; j<dimension_limit; j++) {
            ss << fixed << "("<<table[i][j].throughput<<","<<table[i][j].Qlen<<")\t";
        }
	ss << endl;
    }
    string s = ss.str();
    return s;
}

pair<int,int>
SimpleDRAM::LookupTable::searchFor (float X, float Q) {
    float min_error = 1000000;
    float error;
    float N=0;
    float Z=0;

    //Brute-force through array and find the tuple that minimizes the error
    for (int i=0; i<dimension_limit; i++) {
        for (int j=0; j<dimension_limit; j++) {
	    if (!table[i][j].valid) continue;
            float Xdiff = (X - table[i][j].throughput);
            float Qdiff = (Q - table[i][j].Qlen);
            error = (Xdiff * Xdiff) + (Qdiff * Qdiff);

            if (error < min_error) {
                min_error = error;
                N = i;
                Z = j;
            }
        }
    }

    //cout << "Min error: "<<min_error<<endl;
    pair <int, int> p (N,Z);
    return p;

}

void
SimpleDRAM::LookupTable::printEntry(int N, int Z) {
    cout <<"("<<table[N][Z].throughput<<","<<table[N][Z].Qlen<<")"<<endl;
}

//Prodromou: End of LookupTable class implementation

SimpleDRAM::MemoryPort::MemoryPort(const std::string& name, SimpleDRAM& _memory)
    : QueuedSlavePort(name, &_memory, queue), queue(_memory, *this),
      memory(_memory)
{ }

AddrRangeList
SimpleDRAM::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}

void
SimpleDRAM::MemoryPort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(memory.name());

    if (!queue.checkFunctional(pkt)) {
        // Default implementation of SimpleTimingPort::recvFunctional()
        // calls recvAtomic() and throws away the latency; we can save a
        // little here by just not calculating the latency.
        memory.recvFunctional(pkt);
    }

    pkt->popLabel();
}

Tick
SimpleDRAM::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return memory.recvAtomic(pkt);
}

bool
SimpleDRAM::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    // pass it to the memory controller
    return memory.recvTimingReq(pkt);
}

SimpleDRAM*
SimpleDRAMParams::create()
{
    return new SimpleDRAM(this);
}
