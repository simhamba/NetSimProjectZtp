#include <string.h>
#include "../netsim/common.h"
#include "../netsim/Node.h"
#include "../netsim/Packet.h"
#include "../netsim/Timer.h"
#include "../netsim/PacketScheduler.h"
#include "../netsim/FIFONode.h"
#include "../netsim/Scheduler.h"
#include "../netsim/Config.h"
#include "ZtpHost.h"

void
Config::process_app_command(char* id)
{
    // Insert app-level commands here
    if (strcmp(id, "Router") == 0) {
        if (config_argnum != 2) {
            FATAL("Incorrect number of args for: %s", id);
        }
        FIFONode* router = new FIFONode((Address) config_args[0].numval,
                                      (int) config_args[1].numval);
        TRACE(TRL3, "Initialized router with address %d \n", router->address());
    } else if (strcmp(id, "Host") == 0) {
        if (config_argnum != 1) {
            FATAL("Incorrect number of args for: %s", id);
        }
        ZtpHost* host = new ZtpHost((Address) config_args[0].numval);
    } else if (strcmp(id, "FZTPFlow") == 0) {
        if (config_argnum != 4) {
            FATAL("Incorrect number of args for: %s", id);
        }
        Address source = (Address)config_args[0].numval;
        Address destination = (Address)config_args[1].numval;
        Time txStartTime = (Time)config_args[2].numval;
        char* filePath = config_args[3].strval; 

        ZtpHost* server = (ZtpHost*)scheduler->get_node(destination);
        ZtpHost* client = (ZtpHost*)scheduler->get_node(source);
        server->initialize_receive(destination,txStartTime, filePath);
        client->initialize_send (source, destination, txStartTime, filePath);
        // Make initial TCB's for sender and receiver - initialize memory,
        //      set destination (and senders) address, RTT, RTO, state as
        //      CLOSED, initial seq values
        // Set timer event ACTIVE_OPEN on sender host
        // Set timer event PASSIVE_OPEN on destination host
    }
    config_argnum = 0;
    return;
}

