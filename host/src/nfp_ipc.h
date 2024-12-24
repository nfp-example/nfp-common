/** Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file          nfp_ipc.h
 * @brief         NFP IPC library
 *
 * The NFP IPC library provides a single server, multiple client
 * interprocess communication system using shared memory. It does not
 * provide allocation of the shared memory - nfp_support provides
 * wrappers an abstraction for that, if required. It does provide
 * system-optimized mechanisms to reduce CPU cycles and cache traffic
 * to support IPC.
 *
 */

/*a Includes
 */
#include <stdint.h> 

/*a Defines
 */
#define NFP_IPC_MAX_CLIENTS 64
#define MSGS_PER_QUEUE 8

/*a Enumerations
 */
/** NFP_IPC_STATE, state used for client and server
 */
enum {
    /** For clients, indicates client is inactive **/
    NFP_IPC_STATE_INIT,
    /** For clients & servers, indicates alive **/
    NFP_IPC_STATE_ALIVE,
    /** For clients & servers, indicates shutting down **/
    NFP_IPC_STATE_SHUTTING_DOWN,
    /** For the server, once all clients are shut down, needs
     * reinitialization */
    NFP_IPC_STATE_DEAD
};

/** NFP_IPC_EVENT, return type for server and client poll
 */
enum {
    /** Indicates client/server has shutdown **/
    NFP_IPC_EVENT_SHUTDOWN=-1,
    /** No event ready within timeout **/
    NFP_IPC_EVENT_TIMEOUT,
    /** Message event; event field will be filled out and valid */
    NFP_IPC_EVENT_MESSAGE,
};

/*a Structures
 */
/*f struct _nfp_ipc_msg_data_hdr */ /**
 *
 *  @brief Internal structure for a message queue
 *
 * Internal structure for the header of the message heap
 *
 */
struct _nfp_ipc_msg_data_hdr {
    /** Atomically accessed lock to permit any client or the server to
     * allocate or free messages **/
    int  locked;
    /** Free list for the message heap, which must only be accessed when the lock is held. **/
    int  free_list;
};

/*f struct _nfp_ipc_msg_hdr */ /**
 *
 * @brief Internal structure for the header of a message heap entry
 *
 * The message heap is implemented as a chain of message heap blocks, each with this header.
 *
 */
struct _nfp_ipc_msg_hdr {
    /** Next message heap block in the heap - whether it contains a
     * message or is free; zero for the last block in the heap. **/
    int  next_in_list;
    /** Previous message heap block in the heap - whether it contains
     * a message or is free; zero for the first block in the heap. **/
    int  prev_in_list;
    /** Next free message heap block in the heap; forms the next
     * pointer in the free list chain **/
    int  next_free;
    /** Size in bytes of the message heap block, including this
     * header **/
    int  byte_size;
};

/*f struct _nfp_ipc_msg_data */ /**
 *
 *  @brief Internal structure for message heap
 *
 * Structure for the whole of the message heap.
 */
struct _nfp_ipc_msg_data {
    /** First message heap block header, chaining to the next
     * internally with its @next_in_list element **/
    struct _nfp_ipc_msg_data_hdr hdr;
    /** Data for the contents of the heap
     **/
    char   data[8192-sizeof(struct _nfp_ipc_msg_data_hdr)];
};

/*f struct nfp_ipc_msg */ /**
 *
 * @brief Message as seen by clients and servers
 *
 * Externally visible structure as seen by clients and servers; the @p
 * hdr must not be touched by these, but the @p data field should be
 * used to contain the message data for the message.
 */
struct nfp_ipc_msg {
    /** Message header that the server and client must not
     * touch. Included to make the software simpler, hence faster. **/
    struct _nfp_ipc_msg_hdr hdr;
    /** Data for the payload; an actual message instance has a
     * server/client-defined payload size, from the @p
     * nfp_ipc_msg_alloc call. **/
    char data[4];
};

/*f struct nfp_ipc_client_desc */ /**
 *
 * @brief Client descriptor, to enable a client to register with a server
 *
 * This structure is used in @nfp_ipc_client_start calls, to register
 * a new client with the server.
 *
 */
struct nfp_ipc_client_desc {
    /** Version number; really this is over-the-top, and is currently
     * not used **/
    int version;
    /** Name of the client, for debugging purposes. Not used
     * currently. **/
    const char *name;
};

/*f struct nfp_ipc_server_desc */ /**
 *
 * @brief Server descriptor, for initializing the server
 *
 * This structure is used in @nfp_ipc_server_init calls, to register
 * the server.
 *
 */
struct nfp_ipc_server_desc {
    /** Version number; really this is over-the-top, and is currently
     * not used **/
    int version;
    /** Maximum number of clients that are permited to connect to the
     * server. Cannot exceed 64, or @p NFP_IPC_MAX_CLIENTS **/
    int max_clients;
    /** Name of the server, for debugging purposes. Not used
     * currently. **/
    const char *name;
};

/*f struct nfp_ipc_event */ /**
 *
 *  @brief Structure of an event returned by the @p nfp_ipc_..._poll
 *  functions
 *
 * A structure of this type should be allocated (statically, on the
 * stack, etc) for passing to the client or server poll functions; it
 * is filled out if an event occurs.
 *
 */
struct nfp_ipc_event {
    /** Filled out by poll routine with the NFP IPC structure **/
    struct nfp_ipc *nfp_ipc;
    /** Type of event that occured (e.g. @p NFP_IPC_EVENT_MESSGE **/
    int event_type;
    /** Client that the event is from (for server polling) **/
    int client;
    /** Message that was passed to the client/server **/
    struct nfp_ipc_msg *msg;
};

/*a External functions
 */
/*f nfp_ipc_size */ /**
 *
 * @brief Provide the size of the basic server/client shared memory structure
 *
 * @returns The size of the structure required for the client/server
 * system, so that it may be allocated in shared memory
 *
 * Find the size of shared memory required for the server/client
 * system; this memory has to be accessible by the server and the
 * clients, and it should be cache line aligned. Ideally this is
 * allocated with malloc, for a pthreads approach, or with shm shared
 * memory for separate processes. For the latter, the @p nfp_support
 * functions may be useful. Note that malloc will not necessarily
 * provide cache-line aligned memory regions.
 *
 */
int nfp_ipc_size(void);

/*f nfp_ipc_server_init */ /**
 *
 * @brief Initialize an NFP IPC server
 *
 * @param nfp_ipc Storage for nfp_ipc structure in memory visible to
 * clients and server, of at least @p nfp_ipc_size() bytes
 *
 * @param desc Structure filled out with maximum number of clients, server name, etc
 *
 * Initializes a server with support for up to the supplied maximum number of clients
 *
 * Clients cannot connect to a server until it has been initialized
 */
void nfp_ipc_server_init(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_server_desc *desc);

/*f nfp_ipc_server_shutdown */ /**
 *
 * @brief Shut down the server and inform clients
 *
 * @returns Something indicating how it shut down
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param timeout Time to wait for clients to shutdown; use 0 to
 * return immediately, <0 for indefinitely
 *
 * Wait for clients to stop before quitting the server. This will timeout after the
 * given timeout, if that is non-zero.
 *
 */
int nfp_ipc_server_shutdown(struct nfp_ipc *nfp_ipc, int timeout);

/*f nfp_ipc_server_poll */ /**
 *
 * @brief Server call to poll for messages, or other events
 *
 * @returns NFP_IPC_EVENT_* enumeration indicating that an event is
 * valid (and @p event is filled out), or that the timeout occurred
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param timeout Time to wait for events in microseconds; use 0 to
 * return immediately, <0 for indefinitely
 *
 * @param event Structure to be filled out if a valid event has
 * occurred
 *
 * Poll for events from clients on behalf of for the specified amount
 * of time. If a valid event occurs then @p event will be filled out;
 * otherwise (e.g. on timeouts) an appropriate return value is
 * used and @p event is not touched.
 *
 */
int nfp_ipc_server_poll(struct nfp_ipc *nfp_ipc, int timeout, struct nfp_ipc_event *event);

/*f nfp_ipc_server_send_msg */ /**
 *
 * @brief Send a message from the server to a client
 *
 * @returns Zero on success, non-zero on failure
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to send the message to
 *
 * @param msg Message to send
 *
 * Send a message from the server to a client. The message has to have been allocated with
 * @p nfp_ipc_alloc_msg or received from @p nfp_ipc_client_poll
 *
 */
int nfp_ipc_server_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg);

/*f nfp_ipc_msg_alloc */ /**
 *
 * @brief Allocate a messsage from the server message heap
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param size Size in bytes of the message payload
 *
 * Allocates a message from the server message heap.
 *
 * The allocate/free process is not cheap, and it is preferable to
 * allocate messages at start of day (for the client, usually) and
 * reuse the messages, rather than allocate and free dynamically.
 * 
 */
struct nfp_ipc_msg *nfp_ipc_msg_alloc(struct nfp_ipc *nfp_ipc, int size);

/*f nfp_ipc_msg_free */ /**
 *
 * @brief Free a messsage back to the server message heap
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param nfp_ipc_msg Message returned by either an @p
 * nfp_ipc_msg_alloc or a poll function call
 *
 * Frees the message back to the server message heap, for later
 * allocation.
 *
 * The allocate/free process is not cheap, and it is preferable to
 * allocate messages at start of day (for the client, usually) and
 * reuse the messages, rather than allocate and free dynamically.
 * 
 */
void nfp_ipc_msg_free(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg);

/*f nfp_ipc_client_start */ /**
 *
 * @brief Start the client
 *
 * @returns Client number allocated, or less than zero on failure
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param desc Filled-out client descriptor
 *
 * Finds a legal, inactive client number within the server, and
 * allocates it and returns that client number.
 *
 * Internally, if the call added client 'n', then the server @p
 * active_client_mask will now have bit 'n' set, and @p total_clients
 * will be incremented
 *
 * The approach is to select a client that is not active, and try to
 * claim that. If that fails, try again.
 *
 * If all clients are busy, or the server is shutting down, then the call will fail.
 *
 */
int nfp_ipc_client_start(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_client_desc *desc);

/*f nfp_ipc_client_stop */ /**
 *
 * @brief Stop the client
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client  Client number to send the message to
 *
 * Stop a client that has previously been started
 *
 * Sets the state to be shutting down, and alerts server.
 *
 */
void nfp_ipc_client_stop(struct nfp_ipc *nfp_ipc, int client);

/*f nfp_ipc_client_send_msg */ /**
 *
 * @brief Send a message from a client to the server
 *
 * @returns Zero on success, non-zero on failure
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number sending the message
 *
 * @param msg Message to send
 *
 * Send a message from the client to the server. The message has to have been allocated with
 * nfp_ipc_alloc_msg or received from nfp_ipc_client_poll
 *
 */
int nfp_ipc_client_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg);

/*f nfp_ipc_client_poll */ /**
 *
 * @brief Client call to poll for messages, server shutdown, or other
 * events
 *
 * @returns NFP_IPC_EVENT_* enumeration indicating that an event is
 * valid (and @p event is filled out), or that the timeout occurred,
 * or possibly that the server has shut down and so the client should
 * also do so
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to send the message to
 *
 * @param timeout Time to wait for events in microseconds; use 0 to
 * return immediately, <0 for indefinitely
 *
 * @param event Structure to be filled out if a valid event has
 * occurred
 *
 * Poll for events on behalf of the @p client for the specified amount
 * of time. If a valid event occurs then @p event will be filled out;
 * otherwise (e.g. on timeouts, or if the server has shut down, or if
 * the client is not correctly active) an appropriate return value is
 * used and @p event is not touched.
 *
 */
int nfp_ipc_client_poll(struct nfp_ipc *nfp_ipc, int client, int timeout, struct nfp_ipc_event *event);
