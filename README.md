# stop-and-wait_client-server
Client transmits a text file of any size, reliably, to the Server

Packets can be sent through a network emulator (emulates a router that drops packets at a specified probability)

Emulator:
    --local port
    --client port
    --server port
    --client address
    --server address
    --seed
    --dropProb 
    --verbose-mode <1, 0>
    
Server:
    --dest address to send acks to (emulator address)
    --local port
    --dest port(emulator port)
    --name of file to output transferred data to
    
Client:
    --dest address (possibly emulator address)
    --dest port (emulator port)
    --local port
    --name of file to be transferred

    
Run like: 
./emulator 6000 6001 6002 localhost localhost 0 0.2 1
./server localhost 6002 6000 output.txt
./client localhost 6000 6001 file.txt
