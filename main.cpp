// YOU SHOULD NOT CHANGE THIS FILE
#include <cstdio>
#include "../netsim/common.h"
#include "../netsim/Config.h"
#include "../netsim/Packet.h"
#include "../netsim/Timer.h"
#include "../netsim/PacketScheduler.h"
#include "../netsim/Scheduler.h"
#include "../netsim/Topology.h"

extern "C" {
#include "unistd.h"
};

// Global defaults
char* config_file = "./netsim_config";
const char* options = "hf:t:";

// Global variables
unsigned int trace = 0;

int
usage()
{
    printf("./netsim_app -h -f <filename> -d\n\n");
    printf("\t-h: print this message\n");
    printf("\t-f <filename>: use the specified config file\n");
    printf("\t-t x: bitmap of trace levels\n");
    printf("\t\tgreater than or equal to x\n");
}

int
main(int argc,
     char** argv)
{
    char c;

    // Process command-line options
    opterr = 0;
    while ((c = getopt (argc, argv, options)) != -1)
        switch (c)
        {
        case 'h':
            usage();
            exit(0);

        case 'f':
            config_file = optarg;
            break;

        case 't':
            trace = (unsigned int) atoi(optarg);
            break;

        default:
            usage();
            exit(1);
        }

    // Start the scheduler and topology
    scheduler = new Scheduler;
    topology = new Topology;

    // Read the configuration file
    config = new Config;
    config->parse(config_file);

    scheduler->run();
    // Execution doesn't reach here
}
