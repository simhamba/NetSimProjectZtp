#include "../netsim/common.h"
#include "../netsim/Node.h"
#include "../netsim/FIFONode.h"
#include "../netsim/Packet.h"
#include "ZtpPacket.h"
#include "../netsim/Timer.h"
#include "../netsim/PacketScheduler.h"
#include "../netsim/Scheduler.h"
#include "ZtpHost.h"

AddressTuple* 
AddressTuple::FormTuple(Address A, Address B, Port C, Port D) { 
    sourceNode = A;
    destNode = B;
    sourcePort = C;
    destPort = D;

    return this;
}

bool
AddressTuple::operator< (AddressTuple X)
{
    if(sourceNode < X.sourceNode) {
        return true;
    }
    else if (sourceNode == X.sourceNode) {
        if (destNode < X.destNode) {
            return true;
        }
        else if (destNode == X.destNode) {
            if(sourcePort < X.sourcePort) {
                return true;
            }
            else if (sourcePort == X.sourcePort) {
                if (destPort < X.destPort) {
                    return true;
                }
            }
        }
    }
    return false;
}
bool
AddressTuple::operator==(AddressTuple X)
{
    return ((sourceNode == X.sourceNode) &&
            (destNode == X.destNode) &&
            (sourcePort == X.sourcePort) &&
            (destPort == X.destPort));
}
Address AddressTuple::GetSourceAddr() {return sourceNode;}
Address AddressTuple::GetDestAddr() {return destNode;}
bool
PortMap::reg(Port newPort) {
    int i;
    if(search(newPort) == true)
        return false;
    port_map.insert(port_map.begin(), newPort);
    return true;
}
bool
PortMap::dereg(Port oldPort) {
    int i;
    bool result = false;
    vector<Port>::iterator it;
    for (it = port_map.begin();it < port_map.end(); it++)
        if (*it == oldPort)
        {
            port_map.erase(it);
            result = true;
        }
    return result;
}
bool
PortMap::search(Port entry) {
    vector<Port>::iterator it;
    for (it = port_map.begin();it < port_map.end(); it++)
        if(*it == entry)
            return true;
    return false;
}
Port
PortMap::allocate() {
    Port i = 100;
    do {
        i++;
    } while(search(i) == true);
    return i;
}

Port
PortMap::getFromPool(int X) {
    Port i;
    if(port_map.size()>0)
    {
        i = port_map[0];
    }
    else
        i = 0xFF;

    return i;
}
ZtpHost::ZtpHost(Address a) : FIFONode(a,1000)
{
    int max_tcb_number = 20;
    TRACE(TRL3, "Initialized host with address %d\n", a);
}

ZtpHost::~ZtpHost()
{
    // Empty
}

void
ZtpHost::receive(Packet* pkt)
{
    ZtpPacket* curPkt = (ZtpPacket*)pkt;
    Tcb *curTcb ;
    // first check if there is any Tcb which is at closed/ Listen state
    AddressTuple key;
    key.FormTuple(curPkt->destination, curPkt->source, curPkt->destPort, curPkt->sourcePort);
    TcbMapIterator nmi = tcb_map.find(key);

    if (nmi == tcb_map.end()) {
  
        AddressTuple key1,key2;
        TcbMapIterator nmi1,nmi2;
   
        key1.FormTuple(curPkt->destination, curPkt->source, curPkt->destPort, ZTP_KNOWN_PORT);
        nmi1 = tcb_map.find(key1);

        key2.FormTuple(curPkt->destination, 0xffff, getFromPool(ZTP_KNOWN_PORT), 0xff);
        nmi2 = tcb_map.find(key2);
        
        if (nmi1 != tcb_map.end()) {
            key = key1;
            nmi = nmi1;
        }
        else if (nmi2 != tcb_map.end()) {
            key = key2;
            nmi = nmi2;
        }
        else {
        FATAL("No TCB block for sender %d and receiver %d at Host %d\n", \
            (int)curPkt->source, \
            (int)curPkt->destination, \
            address());
        }
    }
    curTcb = (*nmi).second;
    switch(curTcb->state)
    {
        case LISTEN: {
            if ( curPkt->isSynPacket() ) {
            // Update the Tcb block
                tcb_map.erase(nmi);
                key.FormTuple(curPkt->destination, curPkt->source, curTcb->sourcePort, curPkt->sourcePort);

                dereg(getFromPool(ZTP_KNOWN_PORT));

                TcbMapPair entry(key, curTcb);
                nmi = tcb_map.find(key);
                if(nmi != tcb_map.end())
                {
                    tcb_map.erase(nmi);
                }
                tcb_map.insert(entry);

                curTcb->destination = curPkt->source;
                curTcb->destPort = curPkt->sourcePort;
                curTcb->rcvdSeqNo = curPkt->seqNumber;

            // Make a timer event for RCV_SYN for host
                HostTimerData* context = new HostTimerData(key, RCV_SYN);
                set_timer(scheduler->time(), (void*)context);

            // Tracer output
                TRACE(TRL4, "Recieved SYN  packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            }
            break;
        }
        case SYN_SENT: {
            if( curPkt->isSynAckPacket() && 
                (curPkt->isHostInSync(curTcb->seqNo)) ) {
            // Update Tcb context
                tcb_map.erase(nmi);
                key.FormTuple(curPkt->destination, curPkt->source, curTcb->sourcePort, curPkt->sourcePort);

                TcbMapPair entry(key, curTcb);
                nmi = tcb_map.find(key);
                if(nmi != tcb_map.end())
                {
                    tcb_map.erase(nmi);
                }
                tcb_map.insert(entry);


            // Update TCB block to generate ACK
                curTcb->destPort = curPkt->sourcePort;
                curTcb->rcvdSeqNo = curPkt->seqNumber;
                curTcb->seqNo = curPkt->ackNumber;
                curTcb->ackRcvd();
            
            // Remove timer event
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie);  

            // Make a timer event for RCV_SYNACK
                HostTimerData* context = new HostTimerData(key, RCV_SYNACK);
                set_timer(scheduler->time(), (void*)context);

            // Tracer output
                TRACE(TRL4, "Recieved SYN_ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            }

            break;
        }
        case SYN_RCVD: {
            if( curPkt->isAckPacket() && \
                (curPkt->isHostInSync(curTcb->seqNo)) && \
                (curPkt->isDestinationInSync(curTcb->rcvdSeqNo)) ) {
            // Update TCB block as per the ACK recieved
                curTcb->rcvdSeqNo = curPkt->seqNumber;
                curTcb->seqNo = curPkt->ackNumber;
                curTcb->ackRcvd();

            // Remove timer event
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie);  

            // Make a timer event for RCV_ACK
                HostTimerData* context = new HostTimerData(key, RCV_ACK);
                set_timer(scheduler->time(), (void*)context);
  
            // Tracer output
                TRACE(TRL4, "Recieved ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            //((ZtpPacket*) pkt)->print();
            }
            break;
        }
        case ESTABLISHED: {
            // If recieved a FIN message
            if ( curPkt->isFinPacket() ) {
            //   Update TCB as per the Fin msg
                curTcb->rcvdSeqNo = curPkt->seqNumber;
                curTcb->seqNo = curPkt->ackNumber;
            //   Make a timer event for RCV_FIN_1
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie); 
                curTcb->timeOutSet = false;
                HostTimerData* context = new HostTimerData(key, RCV_FIN_1);
                set_timer(scheduler->time(), (void*)context);
            // Tracer output
                TRACE(TRL4, "Recieved FIN packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            }
            // if data packet is recieved
            else if ( curPkt->isDataPacket() && \
                (curPkt->seqNumber >= curTcb->expectedSendSeqNo )) {
            //   Update TCB as per the Fin msg
                curTcb->rcvdSeqNo = curPkt->seqNumber;
            //  curTcb->seqNo = curPkt->ackNumber;

            //   Make a timer event for RCV_DATA
                HostTimerData* context = new HostTimerData(key, RCV_DATA);
                set_timer(scheduler->time(), (void*)context);
            // Tracer output
                TRACE(TRL4, "Recieved data packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
                
            //    curPkt->print_payload(&(curPkt->data[0]), (curPkt->data).size(),1);
            // Queue packet for application process action
                ZtpPacket* appForwardPkt = new ZtpPacket (*curPkt);
                (curTcb->recvPktQueue).push(appForwardPkt);
                
            }
            // update the TCB as per data packet
            // Make a timer event for RCV_DATA event
            // tracer output
            // if ack packet is received
            else if ( curPkt->isAckPacket() ) {
                // tracer output
                TRACE(TRL4, "Recieved ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            // update the TCB as per the ack packet received
                curTcb->rcvdSeqNo = curPkt->seqNumber;
            //    curTcb->seqNo = curPkt->ackNumber;
                curTcb->baseSeqNo = curPkt->ackNumber;
            // Remove timer event
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie); 
                bool setTimeout = curTcb->dataAckRcvd(curPkt->ackNumber);
                if (curTcb->duplicateAck) {
                    if (curTcb->duplicateAck == 1) {
                        ZtpPacketInfo* backupPktInfo = (curTcb->backupPktInfoQueue).front();
                        ZtpPacket* pkt = new ZtpPacket;
                        curTcb->rebuildPacket( pkt, backupPktInfo );
                        backupPktInfo->fillType( pkt);
                        if (backupPktInfo->type == DATA_TYPE || backupPktInfo->type == LAST_DATA_TYPE) 
                            curTcb->writeData(pkt, backupPktInfo->seqNo);

                        if(send(pkt)) {
                            TRACE(TRL4, "Duplicate Ack: Sent packet from Host %d to Host %d, seq number %d ack number %d\n", \
                            pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
                    // Update Tcb state info
                        }
                        curTcb->cngThresh = (curTcb->cngWind) / 2;
                        curTcb->cngWind = curTcb->cngThresh;
                        curTcb->cngState = CA;
                    }
                    curTcb->packetSent();
                // Start Timer
                    curTcb->timerCookie = new HostTimerData(key, TIMEOUT);
                    set_timer(curTcb->timer, (void*)curTcb->timerCookie);
                }
                else {

                    if(! curTcb->lastPacket) {
                        if (setTimeout == true){
                            curTcb->packetSent();
                            curTcb->timerCookie = new HostTimerData(key, TIMEOUT);
                            curTcb->timeOutSet = true;
                            set_timer(curTcb->timer, (void*)curTcb->timerCookie );
                        }
                // Make a timer event for SEND_DATA event
                        HostTimerData* context = new HostTimerData(key,SEND_DATA);
                        set_timer(scheduler->time(), (void*)context);
                    }
                    else
                    {
                        curTcb->packetSent();
                        curTcb->timerCookie = new HostTimerData(key, TIMEOUT);
                        curTcb->timeOutSet = true;
                        set_timer(curTcb->timer, (void*)curTcb->timerCookie );
                    }
                }
            }

            break;
        }
        case FIN_SENT: {
            // If received an FINACK msg and valid ack
            if ( curPkt->isFinAckPacket() &&\
                (curPkt->isHostInSync(curTcb->seqNo)) ) {
            //   Update TCB as per the finAck msh
                curTcb->rcvdSeqNo = curPkt->seqNumber;
                curTcb->seqNo = curPkt->ackNumber;
                curTcb->ackRcvd();
            // Remove timer event
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie);  

            //   Generate a timer event for RCV_FINACK 
                HostTimerData* context = new HostTimerData(key, RCV_FINACK);
                set_timer(scheduler->time(), (void*)context);
            // Tracer output
                TRACE(TRL4, "Recieved FINACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            }
            break;
        }
        case FIN_RCVD: {
            // If received an ACK msg and valid ack
            if ( curPkt->isAckPacket() && \
                (curPkt->isHostInSync(curTcb->seqNo)) ) {
            //   Update TCB as per the Ack msh
                curTcb->rcvdSeqNo = curPkt->seqNumber;
                curTcb->seqNo = curPkt->ackNumber;
                curTcb->ackRcvd();
            // Remove timer event
                cancel_timer(curTcb->timer,(void*)curTcb->timerCookie);  

            //   Generate a timer event for RCV_ACK_1
                HostTimerData* context = new HostTimerData(key, RCV_ACK_1);
                set_timer(scheduler->time(), (void*)context);
  
            // Tracer output
                TRACE(TRL4, "Recieved ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                curPkt->source,curPkt->destination,curPkt->seqNumber, curPkt->ackNumber );
            }
            break;

        }
        case TIME_WAIT: {
            // Do nothing
            break;
        }
        case CLOSED: {
            break;
        }
        default: {
            break;
        }
    }

    delete pkt;
}

void
ZtpHost::handle_timer(void* cookie)
{
    HostTimerData* curContext = (HostTimerData*)(cookie);
    Tcb* curTcb;

    TcbMapIterator nmi = tcb_map.find(curContext->key);
    if (nmi == tcb_map.end() && 
        !(curContext->currentEvent == CREATE_SOURCE || 
        curContext->currentEvent == CREATE_DESTINATION) )
    {
        FATAL("No TCB block for sender %d and receiver %d at Host %d\n", \
            (int)(curContext->key).GetSourceAddr(), \
            (int)(curContext->key).GetDestAddr(), \
            this->address());
        // Not Reached
    }
    else {
        curTcb= (*nmi).second;
    }
    switch(curContext->currentEvent) {
        case ACTIVE_OPEN: {
        // Make SYN packet as per configuration in TCB i.e source, destination, initial timeout value and Initial seq no.
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->rcvdSeqNo+1;
            pkt->SYN = true;
            pkt->FIN = false;
            pkt->ACK = false;
            curTcb->rto += curTcb->source*1000;

            ZtpPacketInfo* backupPktInfo = new ZtpPacketInfo(pkt);
        // Send packet
            if(send(pkt)) {
                TRACE(TRL4, "Sent SYN packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();

            }

        // Start timer and make a backup of the pkt and the state`
            (curTcb->backupPktInfoQueue).push(backupPktInfo);
            if (!(curTcb->timeOutSet))
            {
                curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
                curTcb->timeOutSet = true;
                set_timer(curTcb->timer, (void*)curTcb->timerCookie );
            }
        // Change state to SYN_SENT
            curTcb->state = SYN_SENT; 
            break;
        }
        case PASSIVE_OPEN: {
           
        // Configure initial TCB , destination address and initial sequence number
        // Change state to LISTEN
            curTcb->state = LISTEN;
            break;
        }
        case TIMEOUT: {
        // Resend the last packet on which timer was started
            ZtpPacketInfo* backupPktInfo = (curTcb->backupPktInfoQueue).front();
            ZtpPacket* pkt = new ZtpPacket;
            curTcb->rebuildPacket( pkt, backupPktInfo );
            backupPktInfo->fillType( pkt);
            if (backupPktInfo->type == DATA_TYPE ||backupPktInfo->type == LAST_DATA_TYPE)
                curTcb->writeData(pkt, backupPktInfo->seqNo);

            if(send(pkt)) {
                TRACE(TRL4, "Timeout: Sent packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }
        // Increase timout value *2  :exponential backoff
            curTcb->rto *= 2;
        // Update Tcb state info
            curTcb->retransmissionSent();

            curTcb->cngThresh = (curTcb->cngWind) / 2;
            curTcb->cngWind = PAYLOAD_SIZE;
            curTcb->cngState = SS;
        // Start Timer
            curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
            set_timer(curTcb->timer, (void*)curTcb->timerCookie);

        // Remain in same state
            break;
        }
        case RCV_SYN: {
        // Synchronize host TCB connection wrt recieved Source, destination and seq number, initial timer value
        // Generate SYNACK packet, by using TCB data
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->rcvdSeqNo+1;
            pkt->SYN = true;
            pkt->FIN = false;
            pkt->ACK = true;

            ZtpPacketInfo* backupPktInfo = new ZtpPacketInfo(pkt);
        // Send packet
            if(send(pkt)) {
                TRACE(TRL4, "Sent SYN_ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }
        // Start timer and make a backup of the pkt and the state
            (curTcb->backupPktInfoQueue).push(backupPktInfo);
            if (!(curTcb->timeOutSet))
            {
                curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
                curTcb->timeOutSet = true;
                set_timer(curTcb->timer, (void*)curTcb->timerCookie );
            }
        // Change state to SYN_RCVD
            curTcb->state = SYN_RCVD; 
            break;
        }
        case RCV_SYNACK: {
        // Stop timer 
        // Calculate RTT, Timout and modify in TCB
        // Synchronize host TCB wrt recieved seq number.
        // Make ACK Packet
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->rcvdSeqNo+1;
            pkt->SYN = false;
            pkt->FIN = false;
            pkt->ACK = true;
        // Send pkt
            if(send(pkt)) {
                TRACE(TRL4, "Sent ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }
            curTcb->expectedSendSeqNo = curTcb->rcvdSeqNo+1;
            curTcb->baseSeqNo = curTcb->seqNo ;
            curTcb->nextDataSeqNo = curTcb->seqNo ;// next data to be sent
            curTcb->fileBase = curTcb->seqNo ;
            curTcb->cngThresh = 8*PAYLOAD_SIZE;
            curTcb->cngWind = PAYLOAD_SIZE;
            curTcb->duplicateAck = 0;
            curTcb->cngState = SS;

            curTcb->rtt = MAX_RTO;
            curTcb->rto = MAX_RTO;
            curTcb->numOfAckRcvd = 0;
        // Change state to ESTABLISHED
            curTcb->state = ESTABLISHED; 
            TRACE(TRL4, "Established FZTP flow from %d to %d (%d)\n",\
                    (int)curTcb->source, (int)curTcb->destination, (int)scheduler->time());

        // Initiate send by setting timer event on SEND_DATA
            HostTimerData* context2 = new HostTimerData(curContext->key, SEND_DATA);
            set_timer(scheduler->time(), (void*)context2);
            break;
        }
        case RCV_ACK: {
        // Stop timer
        // Calculate RTT, Timout and modify in TCB
        // Mdify sender and recieved seq numbers
            curTcb->expectedSendSeqNo = curTcb->rcvdSeqNo;
        // Change state to ESTABLISHED
            curTcb->state = ESTABLISHED; 
            TRACE(TRL3, "Established FZTP flow from %d to %d (%d)\n",\
                    (int)curTcb->destination, (int)curTcb->source, (int)scheduler->time());

            break;
        }
        case SEND_DATA: {

            //Check if sender window saturated
            if (!curTcb->txWindowSaturated())
            {
                ZtpPacket *pkt = new ZtpPacket;
                curTcb->seqNo = curTcb->nextDataSeqNo;
                curTcb->nextDataSeqNo = curTcb->writeData(pkt, curTcb->nextDataSeqNo);
                curTcb->buildPacket(pkt);
                pkt->ackNumber = curTcb->rcvdSeqNo+1;
                pkt->SYN = false;
                pkt->FIN = false;
                pkt->ACK = false;
                pkt->PSH = curTcb->lastPacket;
                
                ZtpPacketInfo* backupPktInfo = new ZtpPacketInfo(pkt);
                
            // send data packet
                if(send(pkt)) {
                    TRACE(TRL4, "Sent data packet from Host %d to Host %d, seq number %d, ack number %d\n", \
                    pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
            // Update Tcb state info
                    curTcb->packetSent();
                }
            // Start timer and make a backup of the pkt and the state
                (curTcb->backupPktInfoQueue).push(backupPktInfo);
                if (!(curTcb->timeOutSet))
                {
                    curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
                    curTcb->timeOutSet = true;
                    set_timer(curTcb->timer, (void*)curTcb->timerCookie );
                }
                if (! (curTcb->lastPacket)) {
                // Initiate send by setting timer event on SEND_DATA
                    HostTimerData* context2 = new HostTimerData(curContext->key, SEND_DATA);
                    set_timer(scheduler->time(), (void*)context2);
                }
            }
            // Do nothing wait for an ack
            break;
        }
        case RCV_DATA: {
        // Do application layer processing 
            ZtpPacket* appForwardPkt = (curTcb->recvPktQueue).top();
            while (curTcb->expectedSendSeqNo == appForwardPkt->seqNumber) {
                curTcb->expectedSendSeqNo += (appForwardPkt->data).size() ;
                (curTcb->rxStream).write(&(appForwardPkt->data[0]), (appForwardPkt->data).size() );
                if ( appForwardPkt->isLastPacket() )  {
                    (curTcb->rxStream).close();
        // Initiate close
        // Generate immediate teardown request by setting timer event on CLOSE_INITIATE
                    HostTimerData* context2 = new HostTimerData(curContext->key, CLOSE_INITIATE);
                    set_timer(scheduler->time(), (void*)context2);

                }

        // Remove the packet from the receive queue
                delete appForwardPkt;
                (curTcb->recvPktQueue).pop();

                if ((curTcb->recvPktQueue).size() == 0)
                    break;
                appForwardPkt = (curTcb->recvPktQueue).top();
            }
        // Process the incoming packet
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->expectedSendSeqNo;
            pkt->SYN = false;
            pkt->FIN = false;
            pkt->ACK = true;
        // Send pkt
            if(send(pkt)) {
                TRACE(TRL4, "Sent ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
            }
            break;
        }
        case CLOSE_INITIATE: {
        // make FIN packet
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->expectedSendSeqNo;
            pkt->SYN = false;
            pkt->FIN = true;
            pkt->ACK = false;

            ZtpPacketInfo* backupPktInfo = new ZtpPacketInfo(pkt);
        // send FIN packet
            if(send(pkt)) {
                TRACE(TRL4, "Sent FIN packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }
        // Start timer and make a backup of the pkt and the state
            (curTcb->backupPktInfoQueue).push(backupPktInfo);
            if (!(curTcb->timeOutSet))
            {
                curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
                curTcb->timeOutSet = true;
                set_timer(curTcb->timer, (void*)curTcb->timerCookie );
            }

        // change state to FIN_SENT
            curTcb->state = FIN_SENT; 
            break;
        }
        case RCV_FIN_1: {
        // make FINACK packet
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->rcvdSeqNo+1;
            pkt->SYN = false;
            pkt->FIN = true;
            pkt->ACK = true;

            ZtpPacketInfo* backupPktInfo = new ZtpPacketInfo(pkt);
        // send FINACK packet
            if(send(pkt)) {
                TRACE(TRL4, "Sent FINACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }
        // Start timer and make a backup of the pkt and the state
            (curTcb->backupPktInfoQueue).push(backupPktInfo);
            if (!(curTcb->timeOutSet))
            {
                curTcb->timerCookie = new HostTimerData(curContext->key, TIMEOUT);
                curTcb->timeOutSet = true;
                set_timer(curTcb->timer, (void*)curTcb->timerCookie );
            }
        // change state to FIN_RCVD
            curTcb->state = FIN_RCVD; 
        
            break;
        }
        case RCV_FINACK: {
        // make ACK packet
            ZtpPacket *pkt = new ZtpPacket;
            curTcb->buildPacket(pkt);
            pkt->ackNumber = curTcb->rcvdSeqNo+1;
            pkt->SYN = false;
            pkt->FIN = false;
            pkt->ACK = true;

        // send ACK packet
            if(send(pkt)) {
                TRACE(TRL4, "Sent ACK packet from Host %d to Host %d, seq number %d ack number %d\n", \
                pkt->source,pkt->destination,pkt->seqNumber, pkt->ackNumber );
        // Update Tcb state info
                curTcb->packetSent();
            }

        // change state to TIME_WAIT
            curTcb->state = TIME_WAIT; 

        // set timer for TIME_WAIT_FINISH
            HostTimerData* context2 = new HostTimerData(curContext->key, TIMER_WAIT_FINISH);
            set_timer(scheduler->time(), (void*)context2);
            break;
        }
        case RCV_ACK_1: {
        // change state to CLOSED
            curTcb->state = CLOSED; 
            TRACE(TRL3, "Tore down FZTP flow from %d to %d (%d)\n",\
                       (int)curTcb->source, (int)curTcb->destination, (int)scheduler->time());
        // NOTE: Due to hardcoding assumption is this is source.
            tcb_map.erase(nmi);
        //delete the closed TCB
            delete curTcb;
            break;
        }
        case TIMER_WAIT_FINISH: {
            tcb_map.erase(nmi);

            TRACE(TRL4, "Tore down FZTP flow from %d to %d (%d)\n",\
                       (int)curTcb->destination, (int)curTcb->source, (int)scheduler->time());
        //delete the closed TCB
            delete curTcb;
        // NOTE: Due to hardcoding assumption is this is destination.
            break;
        }
        case CREATE_SOURCE: 
        case CREATE_DESTINATION: {
            AddressTuple addrTuple = curContext->key;
            Time curTime = scheduler->time();
            TcbMapPair* currentTcbPair;
            TcbTimeMapIterator it; 

            // Search for the current context and update
            if(tcb_time_map.count(curTime) == 1) {
                it = tcb_time_map.find(curTime);
                currentTcbPair = (*it).second;
                tcb_time_map.erase(it);
            }
            else {
                pair <TcbTimeMapIterator, TcbTimeMapIterator> it_pair = \
                    tcb_time_map.equal_range(curTime);
                for (it = it_pair.first; it != it_pair.second; ++it) {
                    TcbMapPair* tempContext = (*it).second;
                    if (tempContext->first == addrTuple) {
                        currentTcbPair = tempContext;
                        break;
                    }
                }
                tcb_time_map.erase(it);        
            }
            curTcb = currentTcbPair->second;
        //Register the source port
            if(!reg(curTcb->sourcePort)) {
                curTcb->sourcePort = allocate();
                reg(curTcb->sourcePort);
                addrTuple.FormTuple(curTcb->source,curTcb->destination, curTcb->sourcePort, curTcb->destPort);
                currentTcbPair->first = addrTuple;
            }
        //Overwrite if already present the entry in the tcb map 
            TcbMapIterator oldContext = tcb_map.find(addrTuple);
            if(oldContext != tcb_map.end()) {
                tcb_map.erase(oldContext);
            }
            tcb_map.insert(*currentTcbPair);
            
            HostTimerData* tcbData;
        // Make a timer event for ACTIVE_OPEN for host
            if (curContext->currentEvent == CREATE_SOURCE) {
                tcbData= new HostTimerData(addrTuple, ACTIVE_OPEN);
            }
            else {
        // Make a timer event for PASSIVE_OPEN for host
                tcbData= new HostTimerData(addrTuple, PASSIVE_OPEN);
            }

            set_timer(curTime, (void*)tcbData);
            break;
        }
        default: {
            break;
        }
    }

    // Delete the extra Host data after consuming the event
    delete curContext;
    return;

}

Tcb::Tcb(Address s, Address d, Port sp, Port dp)
{
    // Initialize state variables
    source = s;
    destination = d;
    sourcePort = sp;
    destPort = dp;
    state = CLOSED;
    timer = 0;
    startTimer = 0;
    rtt = MAX_RTO;
    rto = MAX_RTO;
    numOfPktSent =0;
    numOfAckRcvd =0;
    numOfRetransmissions = 0; 
    averageRtt = MAX_RTO;
    seqNo = ( d!= 0xffff)?SOURCE_SEQ_NO:DEST_SEQ_NO;
    rcvdSeqNo = 0;
    timerCookie = NULL;
    timeOutSet = false;
    lastPacket = false;
    expectedSendSeqNo = 0;
    cngThresh = 8*PAYLOAD_SIZE;
    duplicateAck = 0;
    cngState = SS;
    return;
}
Tcb::~Tcb()
{
}
void
Tcb::buildPacket (ZtpPacket* pkt)
{
    pkt->source = source;
    pkt->destination = destination;
    pkt->sourcePort = sourcePort;
    pkt->destPort = destPort;
    pkt->seqNumber = seqNo;
    pkt->length = HEADER_SIZE + (pkt->data).size();
    pkt->id = pkt->seqNumber; // dummy fill
    pkt->PSH = false;
}
void
Tcb::rebuildPacket (ZtpPacket* pkt, ZtpPacketInfo* backupPktInfo)
{
    pkt->source = source;
    pkt->destination = destination;
    pkt->sourcePort = sourcePort;
    pkt->destPort = destPort;
    pkt->seqNumber = backupPktInfo->seqNo;
    pkt->ackNumber = rcvdSeqNo+1;
    pkt->length = HEADER_SIZE + backupPktInfo->dataLength;
    pkt->id = pkt->seqNumber; // dummy fill
    pkt->PSH = false;
    pkt->ACK = false;
    pkt->FIN = false;
    pkt->SYN = false;
}
void
Tcb::packetSent() {
    startTimer = scheduler->time();
    timer = scheduler->time() + rto;
    numOfPktSent ++;
}
bool
Tcb::txWindowSaturated() {
    if ( (nextDataSeqNo - baseSeqNo) < cngWind)
        return false;
    else
        return true;              
}
void
Tcb::retransmissionSent() {
    if (numOfRetransmissions >= MAX_RETRANSMISSIONS) {
        FATAL("Maximum Timeout exceeded at Host %d\n", source);
    }
    numOfPktSent ++;
    ZtpPacketInfo* backupPktInfo = backupPktInfoQueue.front(); 
    seqNo = backupPktInfo->seqNo;
    //rcvdSeqNo = backupPkt->ackNumber - 1;

}
void
Tcb::ackRcvd() {
    if(numOfRetransmissions == 0) {
        rtt = scheduler->time() - startTimer;
        numOfAckRcvd ++;
        if( numOfAckRcvd == 1) {
            averageRtt = rtt;
            rto = BETA*averageRtt;
        }
        else {
            averageRtt = (Time)(ALPHA*averageRtt + (1.0 - ALPHA)*rtt);
            rto = BETA *averageRtt; 
        }
    }
    // Reset timer default values
    numOfRetransmissions = 0;
    ZtpPacketInfo* backupPktInfo = backupPktInfoQueue.front();
    delete backupPktInfo;
    backupPktInfoQueue.pop();
    timeOutSet = false;
    return;
}
void
ZtpHost::initialize_receive(Address s, Time t, char* filePath)
{
    Address d = 0xffff; //dummy address
    Port sourcePort = allocate();
    Port destPort = 0xff; // dummy destination port
    // Initialize a TCB block in CLOSED state
    Tcb* newTcb = new Tcb(s, d, sourcePort, destPort);
    string rxFilePath (filePath);
    string adnlString ("Rx");
    rxFilePath = adnlString + filePath;
    (newTcb->rxStream).open(rxFilePath.data(), ios::out | ios::binary);
    if ((newTcb->rxStream).is_open() == false)
    {
        (newTcb->rxStream).close();
        remove(rxFilePath.data());
        FATAL("Cannot find %s\n", rxFilePath.data()); 
    }
    
    AddressTuple addrTuple;
    addrTuple.FormTuple(s,d, sourcePort, destPort);
    TcbMapPair* context = new TcbMapPair(addrTuple, newTcb);

    // Save TCB context with Host
    TcbTimeMapPair entry(t, context);
    tcb_time_map.insert(entry);
    // Make a timer event for CREATE_DESTINATION for host
    HostTimerData* eventData = new HostTimerData(addrTuple, CREATE_DESTINATION);
    set_timer(t, (void*)eventData);
    return;
}
void
ZtpHost::initialize_send(Address s, Address d, Time t, char* filePath)
{
    Port sourcePort = allocate();
    Port destPort = ZTP_KNOWN_PORT;    
    // Initialize a TCB block in CLOSED state
    Tcb* newTcb = new Tcb(s, d, sourcePort, destPort);
    
    (newTcb->txStream).open(filePath, ios::in | ios::binary);
    if ((newTcb->txStream).is_open() == false)
    {
        FATAL("Cannot find %s\n", filePath); 

    }
    newTcb->cngWind = PAYLOAD_SIZE;   //setting to be size of 1 packet
    // Save TCB context with Host

    AddressTuple addrTuple;
    addrTuple.FormTuple(s,d, sourcePort, destPort);
    TcbMapPair* context = new TcbMapPair(addrTuple, newTcb);

    // Save TCB context with Host
    TcbTimeMapPair entry(t, context);
    tcb_time_map.insert(entry);
    
    // Make a timer event for CREATE_SOURCE for host
    HostTimerData* eventData = new HostTimerData(addrTuple, CREATE_SOURCE);
    set_timer(t, (void*)eventData);

    return;
}

HostTimerData::HostTimerData(AddressTuple x, ConnectionEvent curEvent)
{
    key = x;
    currentEvent = curEvent;
}

HostTimerData::~HostTimerData()
{
}
unsigned int
Tcb::writeData (ZtpPacket * pkt, unsigned int seqNo) {
    // find the data offset and length from where to begin the copy
    unsigned int toBeSentOffset = (seqNo - fileBase); 
    txStream.seekg(0, ios::end);
    unsigned int fileEnd = (unsigned int) txStream.tellg(); 
    unsigned int dataLength;
    if((fileEnd - toBeSentOffset) > PAYLOAD_SIZE) {
        dataLength = PAYLOAD_SIZE;
        lastPacket = false; 
    }
    else {
        dataLength = fileEnd - toBeSentOffset;
        lastPacket = true;
    }
    txStream.seekg(toBeSentOffset, ios::beg);
    
    //Make data packet
    (pkt->data).resize(dataLength);
    txStream.read(&(pkt->data[0]), dataLength);
    return (seqNo + dataLength);
}
bool 
ltZtpPacket::operator() (ZtpPacket* a1, ZtpPacket* a2) const {
        return (a1->seqNumber > a2->seqNumber);
}

bool
Tcb::dataAckRcvd(unsigned int rcvdAckNo) {
    bool setTimeout = false;
    ZtpPacketInfo* backupPktInfo = backupPktInfoQueue.front();
    if (backupPktInfo->seqNo == rcvdAckNo) {
        duplicateAck++;
    }
    else if (backupPktInfo->seqNo < rcvdAckNo) {
        duplicateAck = 0;
        while ( (backupPktInfo->seqNo < rcvdAckNo) && \
                (backupPktInfoQueue.size() !=0) ) {
            delete backupPktInfo;
            backupPktInfoQueue.pop();
            seqNo = backupPktInfo->seqNo;
            setTimeout = false;
            if (backupPktInfoQueue.size() != 0) {
                backupPktInfo = backupPktInfoQueue.front();
                setTimeout = true;
            }
        }
        if (cngState == SS) {
            cngWind += PAYLOAD_SIZE;
            if (cngWind > cngThresh)
                cngState = CA;
        }
        else {
            cngWind += (int)(PAYLOAD_SIZE * \
                             (float)(PAYLOAD_SIZE / cngWind));
        }
    }
    

    if(numOfRetransmissions == 0) {
        rtt = scheduler->time() - startTimer;
        numOfAckRcvd ++;
        if( numOfAckRcvd == 1) {
            averageRtt = rtt;
            rto = BETA*averageRtt;
        }
        else {
            averageRtt = (Time)(ALPHA*averageRtt + (1.0 - ALPHA)*rtt);
            rto = BETA *averageRtt; 
        }
    }
    // Reset timer default values
    numOfRetransmissions = 0;
    timeOutSet = false;
    return setTimeout;
}
