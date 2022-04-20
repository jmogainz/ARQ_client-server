/*
Jacob Moore
jlm1805

References:
https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
https://beej.us/guide/bgnet/html/
*/

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include <netdb.h>
#include "packet.h"

using std::cout;
using std::endl;
using std::ofstream;


/** Predefining Helper functions*/
void setupDataSockets(sockaddr_in &router, sockaddr_in &server, int &rcvSocket_fd, int & sndSocket_fd,
                    int routerPort, int serverPort, hostent * hostName);


int main(int argc, char *argv[]) {

    /** Initialize server and router local-socket addr obj*/
    struct sockaddr_in router;
    struct sockaddr_in server;

    /** Initialize default values for ports and fd's and save router obj memory size in var*/
    int rcvSocket_fd = 0, sndSocket_fd = 0, routerPort, serverPort;
    socklen_t rlen = sizeof(router);
    socklen_t slen = sizeof(server);

    /**
     * Getting hostname to generate sockaddr 
     *
     * Can also find router with address, but we are assuming address is unknown
     */
    struct hostent *hostName;
    hostName = gethostbyname(argv[1]);

    /** atoi to convert the port arg to an int*/
    serverPort = atoi(argv[2]);
    routerPort = atoi(argv[3]);

    /** Setting up the data socket*/
    setupDataSockets(router, server, rcvSocket_fd, sndSocket_fd, routerPort, serverPort, hostName);

    ofstream uploaded_data;
    uploaded_data.open(argv[4], ofstream::trunc); // trunc overwrites old file
    ofstream rcv_log;
    rcv_log.open("arrival.log", ofstream::trunc);

    if (!uploaded_data.is_open()) {
        cout << "Error opening output file.\n";
        return -1;
    }

    char spacket[40]; // packet size includes control: [0-1], chunk: [2-5]
    bool eof = false;
    int packet_count = 0;
    int expected_seqnum = 0;
    int previous_seqnum = 0;

    /** Receive data packets as long as router is sending them, send back ACKs, and record data to file*/
    while (true) {

        if (packet_count % 2 == 0) expected_seqnum = 0; else expected_seqnum = 1;
        if ((packet_count - 1) % 2 == 0) previous_seqnum = 0; else previous_seqnum = 1;

        memset((char *) &spacket, 0, sizeof(spacket)); // clear out packet data

        // construct packet to deserialize into
        char payload[40];
        memset((char *) &payload, 0, sizeof(payload));
        char *instantiation = payload; // pointer to payload in memory
        packet pk(0, 0, 0, instantiation);

        recvfrom(rcvSocket_fd, spacket, 40, MSG_CONFIRM, (struct sockaddr*)&server, &slen);

        pk.deserialize(spacket);
        pk.printContents();

        /** Create traditional ack packet variables*/
        int type = 0;
        int seqnum = pk.getSeqNum();
        int length = 0;
        char *data = NULL;
        char sACK[40];
        memset((char *) &sACK, 0, sizeof(sACK));

        rcv_log << seqnum << "\n";

        // unpack payload and ack seqnum if received matches expected
        if (pk.getSeqNum() == expected_seqnum && pk.getType() == 1){

            packet ACK(type, seqnum, length, data);
            ACK.serialize(sACK);

            sendto(sndSocket_fd, sACK, 40, MSG_CONFIRM, (struct sockaddr*)&router, rlen);

            packet_count++;

            int data_length = pk.getLength();
            char *data = pk.getData(); 

            for (int i = 0; i < data_length; i++)
                uploaded_data << data[i];

        }else if (pk.getSeqNum() == expected_seqnum && pk.getType() == 3){

            eof = true;
            type = 2;

            packet EOT(type, seqnum, length, data);
            EOT.serialize(sACK);

            sendto(sndSocket_fd, sACK, 40, MSG_CONFIRM, (struct sockaddr*)&router, rlen);

        }else if (pk.getSeqNum() != expected_seqnum){
            
            seqnum = previous_seqnum;

            packet dupACK(type, seqnum, length, data);
            dupACK.serialize(sACK);

            sendto(sndSocket_fd, sACK, 40, MSG_CONFIRM, (struct sockaddr*)&router, rlen);

        }

        if (eof) break; // close server socket, upload file, and shutdown

    }

    uploaded_data.close();
    rcv_log.close();
    close(rcvSocket_fd);
    return 0;
}


/**Initializing UDP Data sockets*/
void setupDataSockets(sockaddr_in &router, sockaddr_in &server, int &rcvSocket_fd, int & sndSocket_fd,
                    int routerPort, int serverPort, hostent * hostName) {
                        
    if ((rcvSocket_fd=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        cout << "Socket creation failed.\n Client Terminated." << endl;
        exit(1);
    }
    if ((sndSocket_fd=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        cout << "Socket creation failed.\n Client Terminated." << endl;
        exit(1);
    }

    /** 
     * Provides specific structure for empty local-socket address obj 
     */
    memset((char *) &router, 0, sizeof(router));
    memset((char *) &server, 0, sizeof(server));

    /** 
     * Initialize local name attributes of router and client
     *
     * sin_family: IPv4
     * sin_addr: IPv4 Address: copied from hostname DNS (local host address)
     * sin_port: Transport port number: routerPort (converted to network byte order)
     *
     * htons for 16 bit (short)
     * htonl for 32 bit (long)
     */
    router.sin_family = AF_INET;
    bcopy((char *)hostName->h_addr,
    (char *)&router.sin_addr.s_addr,
    hostName->h_length);
    router.sin_port = htons(routerPort);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(serverPort);

    /** Binds default file descriptor to local-socket address obj*/
    if (bind(rcvSocket_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        cout << "Error in binding data socket.\n";
        perror(0);
        close(rcvSocket_fd);
        exit(1);
    }

}