#ifndef IPC_H
#define IPC_H

#include <config.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <vpn.h>
#include <tlslib.h>
#include <cookies.h>

#define MAX_MSG_SIZE 256

typedef enum {
	AUTH_INIT=1,
	AUTH_REP,
	AUTH_REQ,
	AUTH_COOKIE_REQ,
	AUTH_MSG,
	RESUME_STORE_REQ,
	RESUME_DELETE_REQ,
	RESUME_FETCH_REQ,
	RESUME_FETCH_REP,
	CMD_UDP_FD,
	CMD_TUN_MTU,
	CMD_TERMINATE,
} cmd_request_t;

typedef enum {
	REP_AUTH_OK = 1,
	REP_AUTH_MSG = 2,
	REP_AUTH_FAILED = 3,
} cmd_auth_reply_t;

typedef enum {
	REP_RESUME_OK = 0,
	REP_RESUME_FAILED = 1,
} cmd_resume_reply_t;

/* AUTH_COOKIE_REQ */
struct __attribute__ ((__packed__)) cmd_auth_cookie_req_st {
	uint8_t cookie[COOKIE_SIZE];
	uint8_t tls_auth_ok;
	char cert_user[MAX_USERNAME_SIZE];
	char cert_group[MAX_GROUPNAME_SIZE];
};

/* AUTH_REQ */
struct __attribute__ ((__packed__)) cmd_auth_req_st {
	uint8_t pass_size;
	char pass[MAX_PASSWORD_SIZE];
};

struct __attribute__ ((__packed__)) cmd_auth_init_st {
	uint8_t user_present;
	char user[MAX_USERNAME_SIZE];
	uint8_t tls_auth_ok;
	char cert_user[MAX_USERNAME_SIZE];
	char cert_group[MAX_GROUPNAME_SIZE];
	char hostname[MAX_HOSTNAME_SIZE];
};

/* AUTH_REP */
struct __attribute__ ((__packed__)) cmd_auth_reply_st {
	uint8_t reply;
	uint8_t cookie[COOKIE_SIZE];
	uint8_t session_id[GNUTLS_MAX_SESSION_ID];
	char vname[IFNAMSIZ]; /* interface name */
	char user[MAX_USERNAME_SIZE];
	char msg[MAX_MSG_SIZE]; /* in case of REP_AUTH_CONTINUE */
};

/* RESUME_FETCH_REQ + RESUME_DELETE_REQ */
struct __attribute__ ((__packed__)) cmd_resume_fetch_req_st {
	uint8_t session_id_size;
	uint8_t session_id[GNUTLS_MAX_SESSION_ID];
};

/* RESUME_STORE_REQ */
struct __attribute__ ((__packed__)) cmd_resume_store_req_st {
	uint8_t session_id_size;
	uint8_t session_id[GNUTLS_MAX_SESSION_ID];
	uint16_t session_data_size;
	uint8_t session_data[MAX_SESSION_DATA_SIZE];
};

/* RESUME_FETCH_REP */
struct __attribute__ ((__packed__)) cmd_resume_fetch_reply_st {
	uint8_t reply;
	uint16_t session_data_size;
	uint8_t session_data[MAX_SESSION_DATA_SIZE];
};

/* TUN_MTU */
struct __attribute__ ((__packed__)) cmd_tun_mtu_st {
	uint16_t mtu;
};

#endif
