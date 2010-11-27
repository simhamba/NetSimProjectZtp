#include <fstream>
#include <queue>
using namespace std;
//#define DEBUG_MODE
#define MAX_RTO (540000)
#define MAX_RETRANSMISSIONS (8)
#define SOURCE_SEQ_NO (12431)
#define DEST_SEQ_NO (35234)
#define ALPHA (0.8)
#define BETA (2)
#define ZTP_KNOWN_PORT (0x41)
class FIFONode;
class ZtpPacket;
class ZtpPacketInfo; 

typedef unsigned char Port;

class AddressTuple
{
    public:

    AddressTuple* FormTuple(Address, Address , Port , Port ); 
    bool operator<(AddressTuple);
    bool operator==(AddressTuple);
    Address GetSourceAddr();
    Address GetDestAddr();
    private:

    Address sourceNode;
    Address destNode;
    Port sourcePort;
    Port destPort;
};
enum ConnectionEvent {
    INVALID_EVENT = -1,
    TIMEOUT           ,
    ACTIVE_OPEN       ,
    PASSIVE_OPEN      ,
    RCV_SYN           ,
    RCV_SYNACK        ,
    RCV_ACK           ,
    CLOSE_INITIATE    ,
    SEND_DATA         ,
    RCV_DATA          ,
    RCV_ACK_DATA      ,
    RCV_FIN_1         ,
    RCV_FINACK        ,
    RCV_ACK_1         ,
    TIMER_WAIT_FINISH ,
    CREATE_SOURCE     ,
    CREATE_DESTINATION,
    MAX_EVENTS
};
enum CongestionState {
    SS,
    CA,
};
enum ConnectionState {
    INVALID_STATE = -1,
    CLOSED        =  0,
    LISTEN        =  1,
    SYN_SENT      =  2,
    SYN_RCVD      =  3,
    ESTABLISHED   =  4,
    FIN_SENT      =  5,
    FIN_RCVD      =  6,
    TIME_WAIT     =  7,

    MAX_STATE

};

class HostTimerData
{
    friend class ZtpHost;
    private:
    AddressTuple key;
    ConnectionEvent currentEvent;

    public:
    HostTimerData(AddressTuple, ConnectionEvent);
    ~HostTimerData();
};

struct ltZtpPacket {
    bool operator() (ZtpPacket* , ZtpPacket* ) const; 
};

typedef priority_queue<ZtpPacket*, vector<ZtpPacket*>, ltZtpPacket> PriorityPktQueue; 
class Tcb
{
    friend class ZtpHost;
    private:
    Address source;
    Address destination;
    Port sourcePort;
    Port destPort;
    ConnectionState state;
    Time timer,startTimer;
    Time rtt, rto;
    Time averageRtt;
    unsigned int seqNo;
    unsigned int rcvdSeqNo;
    unsigned int numOfPktSent;
    unsigned int numOfRetransmissions;
    unsigned int numOfAckRcvd;
    queue<ZtpPacketInfo*> backupPktInfoQueue;
    HostTimerData* timerCookie;
    ifstream txStream;
#ifdef DEBUG_MODE 
    ofstream rxStream;
#endif
    unsigned int cngWind; // sender window
    unsigned int baseSeqNo; // last sent data waiting to be acked
    unsigned int fileBase; 
    bool timeOutSet;
    bool lastPacket;
    PriorityPktQueue recvPktQueue;
    unsigned int expectedSendSeqNo;
    unsigned int nextDataSeqNo;
    
    unsigned int duplicateAck;
    unsigned int cngThresh;
    CongestionState cngState;
    
    public:
    Tcb(Address, Address, Port, Port);
    ~Tcb();
    void buildPacket (ZtpPacket *);
    void packetSent();
    bool txWindowSaturated();
    void retransmissionSent();
    void ackRcvd();
    bool dataAckRcvd(unsigned int);
    void rebuildPacket(ZtpPacket *, ZtpPacketInfo *);
    unsigned int writeData (ZtpPacket*, unsigned int);
};



struct ltAddressTuple {
    bool operator() (AddressTuple a1, AddressTuple a2) const {
        return (a1 < a2);
    }
};
typedef map<AddressTuple, Tcb*, ltAddressTuple> TcbMap;
typedef map<AddressTuple, Tcb*, ltAddressTuple>::iterator TcbMapIterator;
typedef pair<AddressTuple, Tcb*> TcbMapPair;

typedef multimap<Time, TcbMapPair*, ltTime> TcbTimeMap;
typedef multimap<Time, TcbMapPair*, ltTime>::iterator TcbTimeMapIterator;
typedef pair<Time, TcbMapPair*> TcbTimeMapPair;

class PortMap
{
    public:
    bool dereg(Port);
    Port allocate();
    Port getFromPool(int);
    bool reg(Port);
    private: 
    bool search(Port);
    vector<Port> port_map;
};

class ZtpHost: public FIFONode, public PortMap {
    public:

    ZtpHost(Address a);
    ~ZtpHost();
    void initialize_receive(Address, Time, char*);
    void initialize_send(Address, Address, Time, char*);
    void receive(Packet* packet);
    void handle_timer(void* cookie);

    private:
    TcbMap      tcb_map;
    TcbTimeMap  tcb_time_map;
};

