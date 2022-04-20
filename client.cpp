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
#include <netdb.h> 
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "packet.h"

using std::cout;
using std::endl;
using std::stringstream;
using std::noskipws;
using std::ifstream;
using std::ofstream;
using std::ios_base;


/** Predefining Helper functions*/
void setupDataSockets(sockaddr_in &router, sockaddr_in &client, int &rcvSocket_fd, int & sndSocket_fd,
                    int routerPort, int clientPort, hostent * hostname);


int main(int argc, char *argv[]) {

    /** Initialize router and client local-socket addr objs */  
    struct sockaddr_in router;
    struct sockaddr_in client;

    /** Initialize default values for ports and fd's and save router obj memory size in var*/
    /** data Socket used by client is NOT the same as the one used by router*/
    int rcvSocket_fd = 0, sndSocket_fd = 0, routerPort, clientPort;
    socklen_t rlen = sizeof(router);
    socklen_t clen = sizeof(client);

    /**
     * Getting hostname to generate sockaddr 
     *
     * Can also find router with address, but we are assuming address is unknown
     */
    struct hostent *hostName;
    hostName = gethostbyname(argv[1]);
    
    /**atoi to convert the port args to an int*/
    routerPort = atoi(argv[2]);
    clientPort = atoi(argv[3]);
    
    /** Helper function to set up data socket for handshake to occur*/
    setupDataSockets(router, client, rcvSocket_fd, sndSocket_fd, routerPort, clientPort, hostName);
    
    /**opening the requested input_file. If input_file does not exist, remote router is closed*/
    ifstream input_file;
    input_file.open(argv[4]);

    ofstream sent_log;
    ofstream ack_log;
    sent_log.open("clientseqnum.log", ofstream::trunc);
    ack_log.open("clientack.log", ofstream::trunc);

    if (!input_file.is_open()) {

        /** Send termination control bits*/
        cout << "**input_file OPEN FAILURE**\nClient Terminated." << endl;
        close(rcvSocket_fd);
        return -1;

    }

    char byte = 0; // temporary storage for parsing input_file (holds each character)
    int index = 0; // starts at chunk location in packet
    char payload[30]; // storage for payload read from input_file (up to 30 characters)
    int packet_counter = 0; // seqnum for each packet
    int seqnum;
    char ack[40]; // ack from server
    memset((char *) &payload, 0, sizeof(payload));
    memset((char *) &ack, 0, sizeof(ack));
    
    while (input_file.get(byte)) {

        payload[index] = byte;
        index += 1; 

        /** Packet is full when index reaches 30*/
        if (index == 30){
            
            int type = 1; // packet type is 1 for data
            if (packet_counter % 2 == 0) seqnum = 0; else seqnum = 1; // keep seqnum binary
            int length = 30; // data length is 30
            char *data = payload; // pointer to payload in memory

            // create packet obj
            packet pk(type, seqnum, length, data);
            char spacket[40];
            memset((char *) &spacket, 0, sizeof(spacket));

            // serialize packet obj into spacket
            pk.serialize(spacket);

            /** Send packet and receive ack back*/
            sendto(sndSocket_fd, spacket, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
            sent_log << seqnum << "\n";
            pk.printContents();

            // Wait for ack, resends if time exceeds 2 seconds (repeatedly) 
            while (recvfrom(rcvSocket_fd, ack, 40, MSG_CONFIRM, (struct sockaddr*) &client, &clen) < 0){

                cout << "Wait time exceeded. Resending previous packet.\n" << endl;
                sendto(sndSocket_fd, spacket, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
                sent_log << seqnum << "\n";

            }

            /** Grab seqnum from response packet and log it*/
            pk.deserialize(ack);
            ack_log << pk.getSeqNum() << "\n";

            memset((char *) &payload, 0, sizeof(payload)); // clear out packet data
            memset((char *) &ack, 0, sizeof(ack)); // clear out ACK data

            packet_counter++; // inc sequence number
            index = 0; // reset index of payload

            /** If this full packet happens to be eof, handle*/
            if (input_file.peek() == EOF) {
                type = 3;
                if (packet_counter % 2 == 0) seqnum = 0; else seqnum = 1; // keep seqnum binary
                length = 0; // data length is 30
                data = NULL; // pointer to payload in memory
    
                /** Set up EOT packet*/
                packet EOT(type, seqnum, length, data);
                char sEOT[40];
                memset((char *) &sEOT, 0, sizeof(sEOT));
                EOT.serialize(sEOT);

                /** Send EOT packet*/
                sendto(sndSocket_fd, sEOT, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
                sent_log << seqnum << "\n";
                EOT.printContents();

                while (recvfrom(rcvSocket_fd, ack, 40, MSG_CONFIRM, (struct sockaddr*) &client, &clen) < 0){

                    cout << "Wait time exceeded. Resending previous packet.\n" << endl;
                    sendto(sndSocket_fd, sEOT, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
                    sent_log << seqnum << "\n";

                }

                EOT.deserialize(ack);
                ack_log << EOT.getSeqNum() << "\n";

            }

        } 
        /** EOF is met (will not run if eof happened to end on 30th byte of payload)*/
        else if (input_file.peek() == EOF){

            int type = 1; // packet type is 1 for data
            if (packet_counter % 2 == 0) seqnum = 0; else seqnum = 1; // keep seqnum binary
            int length = index; // data length is whatever is left in file
            char *data = payload; // pointer to payload in memory

            // create packet obj
            packet pk(type, seqnum, length, data);
            char spacket[40];
            memset((char *) &spacket, 0, sizeof(spacket));

            // serialize packet obj into spacket
            pk.serialize(spacket);
            pk.printContents();

            /** Send packet and receive ack back*/
            sendto(sndSocket_fd, spacket, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
            sent_log << seqnum << "\n";

            while (recvfrom(rcvSocket_fd, ack, 40, MSG_CONFIRM, (struct sockaddr*) &client, &clen) < 0){

                cout << "Wait time exceeded. Resending previous packet.\n" << endl;
                sendto(sndSocket_fd, spacket, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
                sent_log << seqnum << "\n";

            }

            /** Grab seqnum from response packet and log it*/
            pk.deserialize(ack);
            ack_log << pk.getSeqNum() << "\n";
            
            /** In between packet, updates*/
            packet_counter++; // inc sequence number
            memset((char *) &ack, 0, sizeof(ack)); // clear out ACK data

            type = 3;
            if (packet_counter % 2 == 0) seqnum = 0; else seqnum = 1; // keep seqnum binary
            length = 0; // data length is 30
            data = NULL; // pointer to payload in memory

            /** Set up EOT packet*/
            packet EOT(type, seqnum, length, data);
            char sEOT[40];
            memset((char *) &sEOT, 0, sizeof(sEOT));
            EOT.serialize(sEOT);

            /** Send EOT packet*/
            sendto(sndSocket_fd, sEOT, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
            sent_log << seqnum << "\n";
            EOT.printContents();

            while (recvfrom(rcvSocket_fd, ack, 40, MSG_CONFIRM, (struct sockaddr*) &client, &clen) < 0){

                cout << "Wait time exceeded. Resending previous packet.\n" << endl;
                sendto(sndSocket_fd, sEOT, 40, MSG_CONFIRM, (struct sockaddr*) &router, rlen);
                sent_log << seqnum << "\n";

            }

            EOT.deserialize(ack);
            ack_log << EOT.getSeqNum() << "\n";
            
        }
    }

    cout << "\n\n";
    
    close(rcvSocket_fd);
    input_file.close();
    ack_log.close();
    sent_log.close();
    return 0;
}


/**Initializing UDP Sockets for communication*/
void setupDataSockets(sockaddr_in &router, sockaddr_in &client, int &rcvSocket_fd, int & sndSocket_fd,
                    int routerPort, int clientPort, hostent * hostName) {
    
    /** Creates datagram sockets with IPv4*/
    if ((sndSocket_fd=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        cout << "Socket creation failed.\n Client Terminated." << endl;
        exit(1);
    }
    if ((rcvSocket_fd=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        cout << "Socket creation failed.\n Client Terminated." << endl;
        exit(1);
    }
    
    /** 
     * Provides specific structure for empty local-socket address objs 
     */
    memset((char *) &router, 0, sizeof(router));
    memset((char *) &client, 0, sizeof(client));

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

    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = htons(clientPort);

    /** Binds default input_file descriptor to local-socket address obj*/
    if (bind(rcvSocket_fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
        cout << "Error in binding data socket.\n";
        perror(0);
        close(rcvSocket_fd);
        exit(1);
    }

    /** Sets 2 second timeout needed by stop-and-wait protocol */
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(rcvSocket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror(0);
        close(rcvSocket_fd);
        exit(1);
    }

}