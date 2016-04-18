/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 */

/** Includes
 */
#include <stdint.h> 

/** Defines
 */
#define NFP_IPC_MAX_CLIENTS 64
#define MSGS_PER_QUEUE 8

/** NFP_IPC_STATE
 */
enum {
    NFP_IPC_STATE_INIT,
    NFP_IPC_STATE_ALIVE,
    NFP_IPC_STATE_SHUTTING_DOWN, /* To get clients to shut down */
    NFP_IPC_STATE_DEAD /* Once all clients are inactive */
};

/** NFP_IPC_EVENT
 */
enum {
    NFP_IPC_EVENT_SHUTDOWN=-1,
    NFP_IPC_EVENT_TIMEOUT,
    NFP_IPC_EVENT_MESSAGE,
};

/** struct nfp_ipc_server_data
 */
struct nfp_ipc_server_data {
    int      state;
    int      max_clients;
    int      total_clients;
    int      pad2;
    uint64_t client_mask;
    uint64_t active_client_mask;
    uint64_t doorbell_mask; /* Set bit in here if client has something for server */
    uint64_t pending_mask; /* Server mask indicating which clients to service */
    char pad[16];
};

/** struct nfp_ipc_msg_queue
 */
struct nfp_ipc_msg_queue
{
    int write_ptr;
    int read_ptr;
    int msg_ofs[MSGS_PER_QUEUE];
};

/** struct nfp_ipc_client_data
 */
struct nfp_ipc_client_data {
    int     state;
    int     doorbell_mask; /* Server sets bit in here to wake client */
    struct  nfp_ipc_msg_queue to_serverq;
    struct  nfp_ipc_msg_queue to_clientq;
};

/** struct nfp_ipc_msg_data_hdr
 */
struct nfp_ipc_msg_data_hdr {
    int  locked;
    int  free_list;
};

/** struct nfp_ipc_msg_hdr
 */
struct nfp_ipc_msg_hdr {
    int  next_in_list;
    int  prev_in_list;
    int  next_free;
    int  byte_size;
};

/** struct nfp_ipc_msg
 */
struct nfp_ipc_msg {
    struct nfp_ipc_msg_hdr hdr;
    char data[4];
};

/** struct nfp_ipc_msg_data
 */
struct nfp_ipc_msg_data {
    struct nfp_ipc_msg_data_hdr hdr;
    char   data[8192-sizeof(struct nfp_ipc_msg_data_hdr)];
};

/** struct nfp_ipc_client_desc
 */
struct nfp_ipc_client_desc {
    int version;
    const char *name;
};

/** struct nfp_ipc_server_desc
 */
struct nfp_ipc_server_desc {
    int version;
    int max_clients;
    const char *name;
};

/** struct nfp_ipc
 */
struct nfp_ipc {
    struct nfp_ipc_server_data server;
    struct nfp_ipc_client_data clients[NFP_IPC_MAX_CLIENTS];
    struct nfp_ipc_msg_data msg;
};

/** struct nfp_ipc_event
 */
struct nfp_ipc_event {
    struct nfp_ipc *nfp_ipc;
    int event_type;
    int client;
    struct nfp_ipc_msg *msg;
};

/** nfp_ipc_size
 */
int nfp_ipc_size(void);

/** nfp_ipc_server_init
    Initialize an NFP IPC server

    @nfp_ipc: storage for nfp_ipc structure, private to the server, of at least 'nfp_ipc_size()' bytes
    @desc:    structure filled out with maximum number of clients, server name, etc

    Clients cannot connect to a server until it has been initialized
 */
void nfp_ipc_server_init(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_server_desc *desc);

/** nfp_ipc_server_shutdown
    Shut down a previously initialized server
    Return -1 on error, 0 on success, 1 on timeout
    
    @nfp_ipc: previously initialized server structure
    @timeout: time to wait for clients to shutdown; use 0 to block indefinitely for clients

    Will wait for clients to stop before quitting the server. This will timeout after the
    given timeout, if that is non-zero.
 */
int nfp_ipc_server_shutdown(struct nfp_ipc *nfp_ipc, int timeout);

/** nfp_ipc_server_poll
    Poll the NFP IPC server for messages or events from clients.
    Returns NFP_IPC_EVENT_SHUTDOWN if shut down, NFP_IPC_EVENT_TIMEOUT on timeout,
    else NFP_IPC_EVENT_MESSAGE for a message, in which case event is filled out

    @nfp_ipc: previously initialized server structure
    @timeout: time to wait for messages, 0 for return immediately
    @event:   filled with event that occurred
 */
int nfp_ipc_server_poll(struct nfp_ipc *nfp_ipc, int timeout, struct nfp_ipc_event *event);

/** nfp_ipc_server_send_msg
    Send a message from the server to a client. The message has to have been allocated with
    nfp_ipc_alloc_msg or received from nfp_ipc_server_poll

    @nfp_ipc: previously initialized server structure
    @client:  client number to send the message to
    @msg:     message to send
 */
int nfp_ipc_server_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg);

/** nfp_ipc_alloc_msg
 */
struct nfp_ipc_msg *nfp_ipc_alloc_msg(struct nfp_ipc *nfp_ipc, int size);

/** nfp_ipc_free_msg
 */
void nfp_ipc_free_msg(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg);

/** nfp_ipc_client_start
 *
 * Start a new client. Returns client number allocated
 *
    @nfp_ipc: previously initialized server structure
    @desc:    filled-out client descriptor

 *
 * If added client 'n', then active_client_mask will now have bit 'n'
 * set, and total_clients will be incremented
 *
 * The approach is to select a client that is not active, and try to
 * claim that. If that fails, try again
 *
 * If all clients are busy, of the nfp_ipc is shutting down, then fail.
 *
 */
int nfp_ipc_client_start(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_client_desc *desc);

/** nfp_ipc_client_stop
 *
 * Stop a client that has previously been started
 *
 * Make state to be shutting down, and alerts server
 *
 */
void nfp_ipc_client_stop(struct nfp_ipc *nfp_ipc, int client);

/** nfp_ipc_client_send_msg
    Send a message from the client to the server. The message has to have been allocated with
    nfp_ipc_alloc_msg or received from nfp_ipc_client_poll

    @nfp_ipc: previously initialized server structure
    @client:  client number to send the message to
    @msg:     message to send
 */
int nfp_ipc_client_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg);

/** nfp_ipc_client_poll
 */
int nfp_ipc_client_poll(struct nfp_ipc *nfp_ipc, int client, int timeout, struct nfp_ipc_event *event);
