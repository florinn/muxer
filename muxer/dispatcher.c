#include <signal.h>
#include "global.h"
#include "winsock.h"
#include "connection.h"
#include "mapping.h"
#include "message.h"


const char LOG_PATH[] = "Logs\\";
const char LOG_FILENAME[] = "muxer.log";
char LOG_PATH_FILENAME[MAX_LOG_FILENAME * 2 + 1];
const char LOG_INDEX[] = "muxer.log.index";
char LOG_PATH_INDEX[MAX_LOG_FILENAME * 2 + 1];


#define DEFAULT_PORTLEN 16

config_t global_config = { 
	.connection_in = { .ip = "localhost", .port = 27014 }, 
	.connection_out = { .ip = "localhost", .port = 27015 },
	.log_info = FALSE, .log_debug = TRUE, .log_error = FALSE };

BOOL				is_app_active = TRUE;
CRITICAL_SECTION	csec_app_state;

volatile BOOL		is_out_connection_OK = FALSE;
CRITICAL_SECTION	csec_out_connection;
connection_t		out_connection = 
{ 
	.socket = INVALID_SOCKET, 
	.ip_address = { .ip = "", .port = 0 },
	.r_buffer = "", .r_buffer_ptr = 0, 
	.w_buffer = "", .w_buffer_ptr = 0 
};

SOCKET listener_socket;
struct sockaddr_in output_addr;

HANDLE h_thread_cleanup;
HANDLE h_thread_comm_r;
HANDLE h_thread_comm_w;
HANDLE h_thread_process;


HANDLE h_event_start_threads;
HANDLE h_event_do_cleanup;
HANDLE h_event_cleanup_ended;
HANDLE h_event_process_data;
HANDLE h_event_new_message;

HANDLE h_event_process_thread_started;
HANDLE h_event_comm_w_thread_started;
HANDLE h_event_comm_r_thread_started;

HANDLE h_event_process_thread_stopped;
HANDLE h_event_comm_w_thread_stopped;
HANDLE h_event_comm_r_thread_stopped;

HANDLE h_event_process_thread_ended;
HANDLE h_event_comm_w_thread_ended;
HANDLE h_event_comm_r_thread_ended;

HANDLE h_start_events[3];
HANDLE h_stop_events[3];
HANDLE h_end_events[3];


static void		sigint_handler(int signum);
static void		init_critical_sections();
static void		delete_critical_sections();
static void		create_events();
static void		init_winsock();
static void		open_listener_socket();
static SOCKET	setup_listener_socket(const char* address, USHORT port);
static void		spawn_threads();
static void		process_data();


int main(int argc, char* argv[])
{
	// init log config
	sprintf_s(LOG_PATH_FILENAME, _countof(LOG_PATH_FILENAME), "%s%s", LOG_PATH, LOG_FILENAME);
	sprintf_s(LOG_PATH_INDEX, _countof(LOG_PATH_INDEX), "%s%s", LOG_PATH, LOG_INDEX);

	log_debug_printf(">>>> muxer: BEGIN");

	// setup ctrl-c handler
	signal(SIGINT, sigint_handler);

	// read config
	read_config(&global_config);

	// init critical sections
	init_critical_sections();

	// create events
	create_events();
	
	// init winsock
	init_winsock();

	// open listener socket
	open_listener_socket();

	// spawn threads
	spawn_threads();

	log_debug_printf("**** main loop: START");

	WaitForSingleObject(h_event_cleanup_ended, INFINITE);

	log_debug_printf("**** main loop: END");

	// close listener socket
	shutdown_connection(listener_socket);

	// delete critical sections
	delete_critical_sections();

	// cleanup winsock
	WSACleanup();

	log_debug_printf("<<<< muxer: END");

	return 0;
}

static void sigint_handler(int signum)
{
	EnterCriticalSection(&csec_app_state);
	is_app_active = FALSE;
	LeaveCriticalSection(&csec_app_state);

	SetEvent(h_event_do_cleanup);
}

static void init_critical_sections(void)
{
	InitializeCriticalSection(&csec_app_state);
	InitializeCriticalSection(&csec_out_connection);

	InitializeCriticalSection(&csec_connections);
	
	InitializeCriticalSection(&csec_r_in_messages);
	InitializeCriticalSection(&csec_w_in_messages);
	InitializeCriticalSection(&csec_r_out_messages);
	InitializeCriticalSection(&csec_w_out_messages);

	InitializeCriticalSection(&csec_socket_to_recv_unicast_msg);
	InitializeCriticalSection(&csec_socket_to_recv_multicast_msg);
}

static void delete_critical_sections(void)
{
	DeleteCriticalSection(&csec_connections);
	DeleteCriticalSection(&csec_app_state);

	DeleteCriticalSection(&csec_r_in_messages);
	DeleteCriticalSection(&csec_w_in_messages);
	DeleteCriticalSection(&csec_r_out_messages);
	DeleteCriticalSection(&csec_w_out_messages);

	DeleteCriticalSection(&csec_socket_to_recv_unicast_msg);
	DeleteCriticalSection(&csec_socket_to_recv_multicast_msg);
}

static void create_events(void)
{
	h_event_start_threads = CreateEvent(NULL, TRUE, TRUE, NULL);
	h_event_do_cleanup = CreateEvent(NULL, TRUE, FALSE, NULL);
	h_event_cleanup_ended = CreateEvent(NULL, TRUE, FALSE, NULL);
	h_event_process_data = CreateEvent(NULL, FALSE, FALSE, NULL);
	h_event_new_message = CreateEvent(NULL, FALSE, FALSE, NULL);

	h_start_events[0] = h_event_process_thread_started = CreateEvent(NULL, FALSE, FALSE, NULL);
	h_start_events[1] = h_event_comm_w_thread_started = CreateEvent(NULL, FALSE, FALSE, NULL);
	h_start_events[2] = h_event_comm_r_thread_started = CreateEvent(NULL, FALSE, FALSE, NULL);

	h_stop_events[0] = h_event_process_thread_stopped = CreateEvent(NULL, TRUE, FALSE, NULL);
	h_stop_events[1] = h_event_comm_w_thread_stopped = CreateEvent(NULL, TRUE, FALSE, NULL);
	h_stop_events[2] = h_event_comm_r_thread_stopped = CreateEvent(NULL, TRUE, FALSE, NULL);

	h_end_events[0] = h_event_process_thread_ended = CreateEvent(NULL, FALSE, FALSE, NULL);
	h_end_events[1] = h_event_comm_w_thread_ended = CreateEvent(NULL, FALSE, FALSE, NULL);
	h_end_events[2] = h_event_comm_r_thread_ended = CreateEvent(NULL, FALSE, FALSE, NULL);
}

static void init_winsock(void)
{
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		log_error_printf("WSAStartup failed: %d", result);
		exit(1);
	}
}

static void open_listener_socket(void)
{
	closesocket(listener_socket);
	listener_socket = setup_listener_socket(global_config.connection_in.ip, htons(global_config.connection_in.port));
	log_debug_printf("establishing the listener... as socket: %d", listener_socket);
	if (listener_socket == INVALID_SOCKET) {
		log_debug_printf("%s", WSAGetLastErrorMessage("establish listener", 0));
		exit(1);
	}
}

static SOCKET setup_listener_socket(const char* address, USHORT port)
{
	SOCKET sd;
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	inet_pton(AF_INET, address, &(sa.sin_addr));
	sa.sin_port = port;

	if (sa.sin_addr.s_addr != INADDR_NONE) {
		sd = socket(AF_INET, SOCK_STREAM, 0);
		if (sd != INVALID_SOCKET) {
			if (bind(sd, (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) != SOCKET_ERROR) {
				listen(sd, SOMAXCONN);
				return sd;
			}
			else
				log_debug_printf("%s", WSAGetLastErrorMessage("bind() failed", 0));
		}
	}

	return INVALID_SOCKET;
}


static void cleanup(void)
{
	EnterCriticalSection(&csec_out_connection);

	is_out_connection_OK = FALSE;

	out_connection.r_buffer_ptr = 0;
	out_connection.w_buffer_ptr = 0;

	remove_all_messages();
	remove_all_mappings();
	remove_all_connections();
	
	LeaveCriticalSection(&csec_out_connection);
}

DWORD WINAPI thread_cleanup(LPVOID param)
{
	log_debug_printf("**** cleanup thread: START");

	ResetEvent(h_event_cleanup_ended);

	do {
		WaitForSingleObject(h_event_do_cleanup, INFINITE);

		ResetEvent(h_event_start_threads);

		SetEvent(h_event_new_message);
		SetEvent(h_event_process_data);

		WaitForMultipleObjects(3, h_stop_events, TRUE, INFINITE);

		cleanup();

		ResetEvent(h_event_do_cleanup);

		SetEvent(h_event_new_message);
		SetEvent(h_event_process_data);
		SetEvent(h_event_start_threads);

	} while (is_app_active);

	WaitForMultipleObjects(3, h_end_events, TRUE, 10000);

	SetEvent(h_event_cleanup_ended);

	log_debug_printf("**** cleanup thread: END");

	return 0;
}

static void setup_r_fd_set(fd_set* r_fd, fd_set* e_fd)
{
	queue_entry_ptr qe = NULL;
	connection_ptr p = NULL;

	FD_ZERO(r_fd);
	FD_ZERO(e_fd);

	if (listener_socket != INVALID_SOCKET) {
		FD_SET(listener_socket, r_fd);
		FD_SET(listener_socket, e_fd);
	}

	// this keeps the loop going...
	if (is_out_connection_OK) {
		FD_SET(out_connection.socket, r_fd);
		FD_SET(out_connection.socket, e_fd);
	}
	
	EnterCriticalSection(&csec_connections);

	qe = connections.head;

	// add client connections
	while (qe != NULL) {
		p = (connection_ptr)(qe->ref);
		
		assert(p != NULL);
		assert(p->r_buffer_ptr >= 0);
		
		if (p->r_buffer_ptr < BUFFER_KSIZE)
			// there's space in the read buffer, so pay attention to incoming data
			FD_SET(p->socket, r_fd);

		FD_SET(p->socket, e_fd);

		qe = qe->next;
	}

	LeaveCriticalSection(&csec_connections);
}

static int read_listener_socket(fd_set* r_fd, fd_set* e_fd)
{
	struct sockaddr_in sin_remote;
	int addr_size = sizeof(sin_remote);
	u_long no_block = 1;
	connection_ptr new_connection = NULL;
	queue_entry_ptr new_qe = NULL;

	// something happened on one of the sockets
	if (FD_ISSET(listener_socket, r_fd)) {
		SOCKET socket = accept(listener_socket, (struct sockaddr*)&sin_remote, &addr_size);

		if (socket != INVALID_SOCKET) {
			char ip_addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(sin_remote.sin_addr), ip_addr, INET_ADDRSTRLEN);
			u_short ip_port = ntohs(sin_remote.sin_port);

			log_debug_printf("accepted connection from %s:%d, socket: %d", ip_addr, ip_port, socket);

			// add connection to connections list
			new_connection = alloc_connection(socket, ip_addr, ip_port);
			new_qe = q_alloc((void*)new_connection);
			EnterCriticalSection(&csec_connections);
			q_put(&connections, new_qe);
			LeaveCriticalSection(&csec_connections);

			// mark the socket as non-blocking for safety
			ioctlsocket(socket, FIONBIO, &no_block);
		}
		else {
			log_debug_printf("%s", WSAGetLastErrorMessage("accept() failed", 0));
			return -1;
		}
	}
	else if (FD_ISSET(listener_socket, e_fd)) {
		int err;
		int errlen = sizeof(err);
		getsockopt(listener_socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);

		log_debug_printf("%s", WSAGetLastErrorMessage("exception on listening socket: ", err));
		return -1;
	}

	return 0;
}

static BOOL read_data(connection_ptr conn)
{
	int bytes = 0;

	assert(conn->r_buffer_ptr >= 0);
	assert(conn->r_buffer_ptr <= BUFFER_KSIZE);

	bytes = recv(conn->socket, conn->r_buffer + conn->r_buffer_ptr, BUFFER_KSIZE - (int)conn->r_buffer_ptr, 0);

	if (bytes == 0)	{
		log_debug_printf("socket: %d was closed by the client... shutting down", conn->socket);
		return FALSE;
	}
	else if (bytes == SOCKET_ERROR)	{
		int err;
		int errlen = sizeof(err);
		getsockopt(conn->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
		return (err == WSAEWOULDBLOCK);
	}

	// advance the buffer size counter
	conn->r_buffer_ptr += bytes;

	assert(conn->r_buffer_ptr >= 0);
	assert(conn->r_buffer_ptr <= BUFFER_KSIZE);

	return TRUE;
}

static void read_data_from_out_buffer()
{
	out_message_ptr new_out_message = NULL;
	queue_entry_ptr new_queue_entry = NULL;

	queue_t pdus = { .head = NULL, .tail = NULL };
	queue_entry_ptr qe = NULL;
	pdu_ptr p = NULL;

	read_pdus_from_recv_buffer(out_connection.r_buffer, _countof(out_connection.r_buffer), &out_connection.r_buffer_ptr, &pdus);

	qe = pdus.head;
	while (qe != NULL) {
		p = (pdu_ptr)(qe->ref);
		
		new_out_message = alloc_out_message(p);
		new_queue_entry = q_alloc((void*)new_out_message);

		EnterCriticalSection(&csec_r_out_messages);
		q_put(&r_out_messages, new_queue_entry);
		LeaveCriticalSection(&csec_r_out_messages);

		SetEvent(h_event_process_data);

		qe = qe->next;
	}
}

static void read_out_socket(fd_set* r_fd, fd_set* e_fd)
{
	BOOL ok = TRUE;
	const char* error_type = 0;

	EnterCriticalSection(&csec_out_connection);

	// see if this socket's flag is set in any of the FD sets
	if (FD_ISSET(out_connection.socket, e_fd)) {
		ok = FALSE;
		error_type = "general socket error";
		FD_CLR(out_connection.socket, e_fd);
	}
	else
		if (FD_ISSET(out_connection.socket, r_fd)) {
			ok = read_data(&out_connection);
			error_type = "read error";
			FD_CLR(out_connection.socket, r_fd);
		}

	if (!ok) {
		int err;
		int errlen = sizeof(err);
		getsockopt(out_connection.socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
		if (err != NO_ERROR)
			log_debug_printf("%s", WSAGetLastErrorMessage(error_type, err));

		SetEvent(h_event_do_cleanup);
	}
	else
		read_data_from_out_buffer();

	LeaveCriticalSection(&csec_out_connection);
}

static void read_data_from_in_buffer(connection_ptr client_connection)
{
	in_message_ptr new_client_message = NULL;
	queue_entry_ptr new_queue_entry = NULL;

	queue_t pdus = { .head = NULL, .tail = NULL };
	queue_entry_ptr qe = NULL;
	pdu_ptr p = NULL;

	read_pdus_from_recv_buffer(
		client_connection->r_buffer, _countof(client_connection->r_buffer), &client_connection->r_buffer_ptr, &pdus);

	qe = pdus.head;

	// add client connections
	while (qe != NULL) {
		p = (pdu_ptr)(qe->ref);

		new_client_message = alloc_in_message(client_connection->socket, p);
		new_queue_entry = q_alloc((void*)new_client_message);

		EnterCriticalSection(&csec_r_in_messages);
		q_put(&r_in_messages, new_queue_entry);
		LeaveCriticalSection(&csec_r_in_messages);

		SetEvent(h_event_process_data);

		qe = qe->next;
	}
}

static void read_in_socket(fd_set* r_fd, fd_set* e_fd)
{
	queue_entry_ptr qe = NULL;
	connection_ptr p = NULL;

	EnterCriticalSection(&csec_connections);

	qe = connections.head;

	while (qe != NULL) {
		BOOL ok = TRUE;
		const char* error_type = 0;

		p = (connection_ptr)(qe->ref);

		assert(p != NULL);

		// see if this socket's flag is set in any of the FD sets
		if (FD_ISSET(p->socket, e_fd)) {
			ok = FALSE;
			error_type = "general socket error";
			FD_CLR(p->socket, e_fd);
		}
		else
			if (FD_ISSET(p->socket, r_fd)) {
				ok = read_data(p);
				error_type = "read error";
				FD_CLR(p->socket, r_fd);
			}

		if (!ok) {
			int err;
			int errlen = sizeof(err);
			getsockopt(p->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
			if (err != NO_ERROR)
				log_debug_printf("%s", WSAGetLastErrorMessage(error_type, err));

			shutdown_connection(p->socket);

			remove_connection(p->socket);

			qe = connections.head;
		}
		else {
			read_data_from_in_buffer(p);

			qe = qe->next;
		}
	}

	LeaveCriticalSection(&csec_connections);
}

DWORD WINAPI thread_comm_read(LPVOID param)
{
	log_debug_printf("**** comm read thread: START");

	fd_set read_fd, write_fd, except_fd;
	TIMEVAL tmout = { 2, 2000 };

	SetEvent(h_event_comm_r_thread_started);

	do {
		SetEvent(h_event_comm_r_thread_stopped);
		WaitForSingleObject(h_event_start_threads, INFINITE);
		ResetEvent(h_event_comm_r_thread_stopped);

		FD_ZERO(&write_fd);
		setup_r_fd_set(&read_fd, &except_fd);

		if (select(0, &read_fd, &write_fd, &except_fd, &tmout) > 0)	{
			read_listener_socket(&read_fd, &except_fd);
			
			if (is_out_connection_OK)
				read_out_socket(&read_fd, &except_fd);
			
			read_in_socket(&read_fd, &except_fd);
		}
		else
			log_debug_printf("%s", WSAGetLastErrorMessage("select() failed", 0));

	} while (is_app_active);

	SetEvent(h_event_comm_r_thread_stopped);
	SetEvent(h_event_comm_r_thread_ended);
	
	log_debug_printf("**** comm read thread: END");

	return 0;
}

static void write_data_to_out_buffer(void)
{
	// simplest case considered (entire message copy)
	queue_entry_ptr qe = NULL;
	out_message_ptr p;

	EnterCriticalSection(&csec_out_connection);

	assert(out_connection.w_buffer_ptr >= 0);
	assert(out_connection.w_buffer_ptr <= BUFFER_KSIZE);

	if (out_connection.w_buffer_ptr == 0) {
		EnterCriticalSection(&csec_w_out_messages);
		qe = q_get(&w_out_messages);
		LeaveCriticalSection(&csec_w_out_messages);

		if (qe != NULL)	{
			p = (out_message_ptr)(qe->ref);

			write_pdu_to_send_buffer(p->pdu, out_connection.w_buffer, _countof(out_connection.w_buffer), &out_connection.w_buffer_ptr);

			q_free(&w_out_messages, qe);

			free_out_message(p);

			log_debug_printf("inserted %d chars into W buffer of socket out %d", out_connection.w_buffer_ptr, out_connection.socket);
		}
	}

	LeaveCriticalSection(&csec_out_connection);
}

static void free_w_in_messages_mapped_to_invalid_sockets(void)
{
	// simplest case considered (entire message copy)
	queue_entry_ptr qe_msg = NULL, qe_msg_prev = NULL;
	queue_entry_ptr qe_conn = NULL;
	connection_ptr p = NULL;
	in_message_ptr r = NULL;
	BOOL existsAssocSocket = FALSE;
	unsigned int count = 0;
	
	EnterCriticalSection(&csec_w_in_messages);

	qe_msg = w_in_messages.head;

	while (qe_msg != NULL) {
		r = (in_message_ptr)(qe_msg->ref);
		assert(r != NULL);

		existsAssocSocket = FALSE;

		EnterCriticalSection(&csec_connections);

		qe_conn = connections.head;

		while (qe_conn != NULL)	{
			p = (connection_ptr)(qe_conn->ref);
			assert(p != NULL);
			assert(p->w_buffer_ptr >= 0);
			assert(p->w_buffer_ptr <= BUFFER_KSIZE);

			if (r->socket == p->socket)	{
				existsAssocSocket = TRUE;
				break;
			}

			qe_conn = qe_conn->next;
		}

		LeaveCriticalSection(&csec_connections);

		if (existsAssocSocket != TRUE) {
			qe_msg_prev = qe_msg;
			qe_msg = qe_msg->next;

			assert(qe_msg_prev != NULL);

			q_free(&w_in_messages, qe_msg_prev);

			free_in_message(r);
		}
		else
			qe_msg = qe_msg->next;
	}

	LeaveCriticalSection(&csec_w_in_messages);

	log_debug_printf("done removing %d messages from 'w_in_messages'", count);
}

static void write_data_to_in_buffer(void)
{
	// simplest case considered (entire message copy)
	queue_entry_ptr qe_msg = NULL, qe_conn = NULL;
	connection_ptr p = NULL;
	in_message_ptr r = NULL;

	EnterCriticalSection(&csec_connections);

	qe_conn = connections.head;

	while (qe_conn != NULL) {
		p = (connection_ptr)(qe_conn->ref);
		assert(p != NULL);
		assert(p->w_buffer_ptr >= 0);
		assert(p->w_buffer_ptr <= BUFFER_KSIZE);

		if (p->w_buffer_ptr == 0) {
			EnterCriticalSection(&csec_w_in_messages);

			qe_msg = w_in_messages.head;

			while (qe_msg != NULL) {
				r = (in_message_ptr)(qe_msg->ref);
				assert(r != NULL);

				if (r->socket == p->socket) {
					write_pdu_to_send_buffer(r->pdu, p->w_buffer, _countof(p->w_buffer), &p->w_buffer_ptr);

					q_free(&w_in_messages, qe_msg);

					free_in_message(r);

					log_debug_printf("inserted %d chars into W buffer of socket in %d", p->w_buffer_ptr, p->socket);

					break;
				}

				qe_msg = qe_msg->next;
			}

			LeaveCriticalSection(&csec_w_in_messages);
		}

		qe_conn = qe_conn->next;
	}

	LeaveCriticalSection(&csec_connections);
}

static void setup_w_fd_set(fd_set* w_fd, fd_set* e_fd)
{
	queue_entry_ptr qe = NULL;
	connection_ptr p = NULL;

	FD_ZERO(w_fd);
	FD_ZERO(e_fd);

	// this keeps the loop going...
	if (is_out_connection_OK) {
		FD_SET(out_connection.socket, w_fd);
		FD_SET(out_connection.socket, e_fd);
	}
	
	EnterCriticalSection(&csec_connections);

	qe = connections.head;

	while (qe != NULL) {
		p = (connection_ptr)(qe->ref);
		assert(p != NULL);
		FD_SET(p->socket, w_fd);
		FD_SET(p->socket, e_fd);

		qe = qe->next;
	}

	LeaveCriticalSection(&csec_connections);
}

static BOOL write_data(connection_ptr conn)
{
	int bytes = 0;

	assert(conn->w_buffer_ptr >= 0);
	assert(conn->w_buffer_ptr <= BUFFER_KSIZE);

	if (conn->w_buffer_ptr > 0) {
		bytes = send(conn->socket, conn->w_buffer, (int)conn->w_buffer_ptr, 0);

		if (bytes == SOCKET_ERROR) {
			int err;
			int errlen = sizeof(err);
			getsockopt(conn->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
			return (err == WSAEWOULDBLOCK);
		}

		if (bytes == conn->w_buffer_ptr)
			// everything got sent, so take a shortcut on clearing buffer
			conn->w_buffer_ptr = 0;
		else {
			// we sent part of the buffer's data, remove that data from the buffer
			conn->w_buffer_ptr -= bytes;

			assert(conn->w_buffer_ptr >= 0);
			assert(conn->w_buffer_ptr <= BUFFER_KSIZE);

			memmove_s(conn->w_buffer, _countof(conn->w_buffer),	conn->w_buffer + bytes, conn->w_buffer_ptr);
		}
	}

	return TRUE;
}

static void write_out_socket(fd_set* w_fd, fd_set* e_fd)
{
	BOOL ok = TRUE;
	const char* error_type = 0;
	
	EnterCriticalSection(&csec_out_connection);

	// see if this socket's flag is set in any of the FD sets
	if (FD_ISSET(out_connection.socket, e_fd)) {
		ok = FALSE;
		error_type = "general socket error";
		FD_CLR(out_connection.socket, e_fd);
	}
	else
		if (FD_ISSET(out_connection.socket, w_fd)) {
			ok = write_data(&out_connection);
			error_type = "write error";
			FD_CLR(out_connection.socket, w_fd);
		}

	if (!ok) {
		int err;
		int errlen = sizeof(err);
		getsockopt(out_connection.socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
		if (err != NO_ERROR)
			log_debug_printf("%s", WSAGetLastErrorMessage(error_type, err));

		SetEvent(h_event_do_cleanup);
	}

	LeaveCriticalSection(&csec_out_connection);
}

static void write_in_socket(fd_set* w_fd, fd_set* e_fd)
{
	queue_entry_ptr qe = NULL;
	connection_ptr p = NULL;

	EnterCriticalSection(&csec_connections);

	qe = connections.head;

	while (qe != NULL) {
		BOOL ok = TRUE;
		const char* error_type = 0;

		p = (connection_ptr)(qe->ref);

		assert(p != NULL);

		// see if this socket's flag is set in any of the FD sets
		if (FD_ISSET(p->socket, e_fd)) {
			ok = FALSE;
			error_type = "general socket error";
			FD_CLR(p->socket, e_fd);
		}
		else
			if (FD_ISSET(p->socket, w_fd)) {
				ok = write_data(p);
				error_type = "write error";
				FD_CLR(p->socket, w_fd);
			}

		if (!ok) {
			int err;
			int errlen = sizeof(err);
			getsockopt(p->socket, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
			if (err != NO_ERROR)
				log_debug_printf("%s", WSAGetLastErrorMessage(error_type, err));

			shutdown_connection(p->socket);

			// remove the connection associated to the socket
			remove_connection(p->socket);

			qe = connections.head;
		}
		else
			// go on to next connection
			qe = qe->next;
	}

	LeaveCriticalSection(&csec_connections);
}

DWORD WINAPI thread_comm_write(LPVOID param)
{
	log_debug_printf("**** comm write thread: START");
	
	fd_set read_fd, write_fd, except_fd;
	int count = 1;

	SetEvent(h_event_comm_w_thread_started);

	do {
		SetEvent(h_event_comm_w_thread_stopped);
		WaitForSingleObject(h_event_start_threads, INFINITE);
		ResetEvent(h_event_comm_w_thread_stopped);

		WaitForSingleObject(h_event_new_message, INFINITE);

		write_data_to_out_buffer();

		count++;
		if (count % 1000 == 0) {
			free_w_in_messages_mapped_to_invalid_sockets();
			count = 1;
		}

		write_data_to_in_buffer();

		FD_ZERO(&read_fd);
		setup_w_fd_set(&write_fd, &except_fd);

		if (select(0, &read_fd, &write_fd, &except_fd, NULL) > 0) {
			if (is_out_connection_OK)
				write_out_socket(&write_fd, &except_fd);
			
			write_in_socket(&write_fd, &except_fd);
		}
		else
			log_debug_printf("%s", WSAGetLastErrorMessage("select() failed", 0));

		if (w_in_messages.head != NULL || w_out_messages.head != NULL)
			SetEvent(h_event_new_message);

	} while (is_app_active);

	EnterCriticalSection(&csec_out_connection);

	write_data(&out_connection);
	
	// close output socket
	shutdown_connection(out_connection.socket);
	LeaveCriticalSection(&csec_out_connection);

	SetEvent(h_event_comm_w_thread_stopped);
	SetEvent(h_event_comm_w_thread_ended);
	
	log_debug_printf("**** comm write thread: END");

	return 0;
}

DWORD WINAPI thread_processing(LPVOID param)
{
	log_debug_printf("**** processing thread: START");

	SetEvent(h_event_process_thread_started);

	do {
		SetEvent(h_event_process_thread_stopped);
		WaitForSingleObject(h_event_start_threads, INFINITE);
		ResetEvent(h_event_process_thread_stopped);

		WaitForSingleObject(h_event_process_data, INFINITE);
		process_data();

		if (r_in_messages.head != NULL || r_out_messages.head != NULL)
			SetEvent(h_event_process_data);

	} while (is_app_active);

	SetEvent(h_event_process_thread_stopped);
	SetEvent(h_event_process_thread_ended);
	
	log_debug_printf("**** processing thread: END");

	return 0;
}

static void spawn_threads(void)
{
	h_thread_cleanup = CreateThread(NULL, 0, thread_cleanup, NULL, 0, NULL);
	h_thread_comm_r = CreateThread(NULL, 0, thread_comm_read, NULL, 0, NULL);
	h_thread_comm_w = CreateThread(NULL, 0, thread_comm_write, NULL, 0, NULL);
	h_thread_process = CreateThread(NULL, 0, thread_processing, NULL, 0, NULL);
	
	SetEvent(h_event_start_threads);
}

static BOOL connect_to_output(void)
{
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char connection_out_port[DEFAULT_PORTLEN];
	sprintf_s(connection_out_port, DEFAULT_PORTLEN, "%d", global_config.connection_out.port);

	// resolve the server address and port
	int iResult = getaddrinfo(global_config.connection_out.ip, connection_out_port, &hints, &result);
	if (iResult != 0) {
		log_error_printf("getaddrinfo failed with error: %d", iResult);
		return FALSE;
	}

	// attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// create a SOCKET for connecting to output
		out_connection.socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (out_connection.socket == INVALID_SOCKET) {
			log_error_printf("socket failed with error: %d", WSAGetLastError());
			closesocket(out_connection.socket);
			out_connection.socket = INVALID_SOCKET;
			return FALSE;
		}

		// connect to output
		iResult = connect(out_connection.socket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			log_error_printf("%s", WSAGetLastErrorMessage("error connect to output: ", WSAGetLastError()));
			closesocket(out_connection.socket);
			out_connection.socket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (out_connection.socket == INVALID_SOCKET) {
		log_debug_printf("unable to connect to output!");
		closesocket(out_connection.socket);
		out_connection.socket = INVALID_SOCKET;
		return FALSE;
	}
	else
		log_debug_printf("connected to output at %s:%d, socket: %d", 
		global_config.connection_out.ip, global_config.connection_out.port, out_connection.socket);

	return TRUE;
}

static void process_data(void)
{
	EnterCriticalSection(&csec_out_connection);
	if (is_out_connection_OK == FALSE) {
		// connect to output
		if (connect_to_output() == FALSE) {
			LeaveCriticalSection(&csec_out_connection);
			Sleep(2000);
			return;
		}
		else
			is_out_connection_OK = TRUE;
	}
	LeaveCriticalSection(&csec_out_connection);

	process_incoming_out_messages();
	process_incoming_in_messages();
}