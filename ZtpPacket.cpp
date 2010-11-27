#include "../netsim/common.h"
#include "../netsim/Node.h"
#include "../netsim/Packet.h"
#include "../netsim/Timer.h"
#include "../netsim/PacketScheduler.h"
#include "../netsim/FIFONode.h"
#include "../netsim/Scheduler.h"
#include "ZtpPacket.h"

void 
ZtpHeader::print_ZtpHeader()
{
    TRACE(TRL3, "Sequence Number: %d, Acknowledgement Number: %d,SYN flag: %d, FIN flag: %d, ACK flag: %d\n",\
            (int)seqNumber , (int)ackNumber ,(int)SYN, (int)FIN, (int)ACK );
}

void
ZtpPacket::print()
{
    Packet::print_header();
    ZtpHeader::print_ZtpHeader();
//    Packet::print_payload((char *) &data[0], length - DATA_OFFSET, false);
}

bool
ZtpPacket::isLastPacket()
{
    return (PSH == true);
}
bool
ZtpPacket::isSynPacket()
{
    return ((SYN == true) && (ACK == false) && (FIN == false));
}

bool
ZtpPacket::isSynAckPacket()
{
    return ((SYN == true) && (ACK == true) && (FIN == false));
}

bool
ZtpPacket::isAckPacket()
{
    return ((SYN == false) && (ACK == true) && (FIN == false));
}
bool
ZtpPacket::isFinPacket()
{
    return ((SYN == false) && (ACK == false) && (FIN == true));
}

bool
ZtpPacket::isFinAckPacket()
{
    return ((SYN == false) && (ACK == true) && (FIN == true));
}
bool
ZtpPacket::isFlowInSync(unsigned int destinationSeqNum)
{
    return (seqNumber == destinationSeqNum);
}
bool
ZtpPacket::isHostInSync(unsigned int hostSeqNum)
{
    return (ackNumber == hostSeqNum + 1);
}
bool
ZtpPacket::isDestinationInSync(unsigned int destinationSeqNum)
{
    return (seqNumber == destinationSeqNum + 1);
}
bool
ZtpPacket::operator< (ZtpPacket A)
{
    return (seqNumber > A.seqNumber);
}
bool
ZtpPacket::isDataPacket()
{
    return ((SYN == false) && (ACK == false) && (FIN == false));
}

ZtpPacketInfo::ZtpPacketInfo(ZtpPacket* pkt) {
    seqNo = pkt->seqNumber;
    dataLength = (pkt->data).size();
    if (pkt->isSynAckPacket() ) {
        type = SYN_ACK_TYPE;
    } else if (pkt->isFinAckPacket() ) {
        type = FIN_ACK_TYPE;
    } else if (pkt->isSynPacket() ) {
        type = SYN_TYPE;
    } else if (pkt->isFinPacket() ) {
        type = FIN_TYPE;
    } else if (pkt->PSH == true) {
        type = LAST_DATA_TYPE;
    } else if (pkt->isDataPacket() ) {
        type = DATA_TYPE;
    }

}

ZtpPacketInfo::~ZtpPacketInfo() {
}


void
ZtpPacketInfo::fillType(ZtpPacket* pkt) {
    switch (type) {
        case SYN_TYPE: {
            pkt->SYN = true;
            break; 
        }
        case SYN_ACK_TYPE: {

            pkt->ACK = true;
            pkt->SYN = true;
            break; 
        }
        case FIN_TYPE:{
            pkt->FIN = true;
            break; 
        }
        case FIN_ACK_TYPE:{
            pkt->FIN = true;
            pkt->ACK = true;
            break; 
        }
        case LAST_DATA_TYPE:{
            pkt->PSH = true;
            break; 
        }
        case DATA_TYPE:{
            break; 
        }
        
    }
}
