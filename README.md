
# tcp-mitm-proxy
A multi-client man-in-the-middle (MITM) proxy for TCP connections over IPv4. It allows interception and optional modification of traffic between clients and any remote server, configurable to bind on any IP and port.

## Usage
#### In main.c file, there are fields to set in order to configure the proxy:
```C
#define IDLE_SLEEP_MS 10

#define MAX_CLIENTS 1
#define RECV_BUFFER_SIZE (2 * 1024 * 1024)

const char *server_ip = "x.x.x.x";
const unsigned short server_port = 80;

const char *listen_ip = "127.0.0.1";
const unsigned short listen_port = 80;
```
**IDLE_SLEEP_MS**: **The proxy server's polling interval if there are no clients.**
**MAX_CLIENTS**: **The proxy server's maximum client count.**
**RECV_BUFFER_SIZE**: **The proxy server's max recv size, you should set this to the maximum packet size that can be received from the client and the server for the best compatibility.**

**server_ip**: **The remote server's ip.**
**server_port**: **The remote server's port.**
**listen_ip**: **The ip where clients will connect.**
**listen_port**: **The port where clients will connect.**

## Packet manipulation
Use events.h file, if you desire to manipulate the packets.
The handle_packet_event function has five parameters:
```C
int handle_packet_event(SOCKET client, SOCKET server, int from_client, const char *packet, int packet_length)
```
**client**: **The client socket**
**server**: **The remove server socket**
**from_client**: **A boolean for checking if the packet is coming from the client or not**
**packet**: **The packet's pointer**
**packet_length**: **The packet's length**

**If the return value is 'true' then the packet is sent, if 'false' then the packet is dropped in the original loop.**

## How it works
For every client that connects to the proxy server, the proxy create a new socket and connect to the remote server, any data received from the client will be sent to the remote server and every data received from the remote server will be sent to the remote server:

**1**: Client connects to the proxy server.
**2**: Proxy connects to the remote server with a new socket.
--- **2***: Individual connection for every new client.
**3**: If a client sends a data to the proxy server, it gets forwarded to the remote server.
**4**: If a server sends a data to a proxy client, it gets forwarded to the client corresponding to the socket received the data.

## How it handles multiple clients
It maps each client connected to the proxy server to a new socket and every data received and sent will be on that socket.
Uses no multi-threading and uses select() in order to have the best performance without using threading.

### Contributions

Im open to all contribution requests so if there is something needs to be added or fixed, let me know :)
