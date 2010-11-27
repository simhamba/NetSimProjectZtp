#include <cstdio>

#define HEADER_SIZE (sizeof(Packet) + sizeof(ZtpHeader))
#define SEGMENT_OFFSET (sizeof(Packet))
#define PAYLOAD_SIZE (MTU - HEADER_SIZE)

// ZTP segment header
class ZtpHeader {
    public:
        void print_ZtpHeader();
        unsigned int    sourcePort;
        unsigned int    destPort;
        unsigned int    seqNumber;
        unsigned int    ackNumber;
        bool            SYN;
        bool            FIN;
        bool            ACK;
        bool            PSH; // To indicate end of data
};

enum PacketType {
    SYN_TYPE,
    SYN_ACK_TYPE,
    DATA_TYPE,
    LAST_DATA_TYPE,
    FIN_TYPE,
    FIN_ACK_TYPE 
};

// FZTP Packet made up of Packet Header , ZTP Header and Segment data
class ZtpPacket : public Packet, public ZtpHeader {
    public:
        void print();
        bool isFlowInSync(unsigned int);
        bool isLastPacket();
        bool isSynPacket();
        bool isSynAckPacket();
        bool isFinPacket();
        bool isFinAckPacket();
        bool isAckPacket();
        bool isDataPacket();
        bool isHostInSync(unsigned int );
        bool isDestinationInSync(unsigned int );
        bool operator< (ZtpPacket);

        vector<char> data;
        
};
class ZtpPacketInfo 
{
    public:
    PacketType type;
    unsigned int seqNo;
    unsigned int dataLength; //only used for data packets

    ZtpPacketInfo (ZtpPacket*);
    void fillType(ZtpPacket *);
    ~ZtpPacketInfo ();
};
