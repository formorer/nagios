#ifndef INCLUDE_worker_h__
#define INCLUDE_worker_h__
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "libnagios.h"

/**
 * @file worker.h
 * @brief Worker implementation along with various helpers
 *
 * This code isn't really in the "library" category, but it's tucked
 * in here to provide a good resource for writing remote workers and
 * as an example on how to use the API's found here.
 */

/*
 * Since PAIR_SEP is a nul byte, and MSG_DELIM is all nuls, we must
 * take care to use different MSG_DELIM_LEN depending on if we're
 * sending or receiving. Change this if PAIR_SEP alters.
 */
#define MSG_DELIM "\0\0\0" /**< message limiter */
#define MSG_DELIM_LEN (sizeof(MSG_DELIM) - 1) /**< message delimiter length */
#define MSG_DELIM_LEN_SEND (MSG_DELIM_LEN - 1) /**< msg delim len when sendin */
#define MSG_DELIM_LEN_RECV (MSG_DELIM_LEN) /**< msg delimm len when receivin */
#define PAIR_SEP 0 /**< pair separator for buf2kvvec() and kvvec2buf() */
#define KV_SEP '=' /**< key/value separator for buf2kvvec() and kvvec2buf() */

struct worker_process;

/** Worker job data */
typedef struct worker_job {
	int id;         /**< job id */
	int type;       /**< internal only */
	time_t timeout; /**< timeout, in absolute time */
	char *command;  /**< command string for this job */
	struct worker_process *wp; /**< worker process running this job */
	void *arg;      /**< any random argument */
} worker_job;

/** A worker process as seen from its controller */
typedef struct worker_process {
	const char *type; /**< identifying typename of this worker */
	int sd;    /**< communication socket */
	pid_t pid; /**< pid */
	int max_jobs; /**< Max number of jobs we can handle */
	int jobs_running; /**< jobs running */
	int jobs_started; /**< jobs started */
	struct timeval start; /**< worker start time */
	iocache *ioc; /**< iocache for reading from worker */
	worker_job **jobs; /**< array of jobs */
	int job_index; /**< round-robin slot allocator (this wraps) */
	struct worker_process *prev_wp; /**< previous worker in list */
	struct worker_process *next_wp; /**< next worker in list */
} worker_process;

/**
 * Spawn a worker process
 * @param[in] init_func The initialization function for the worker
 * @param[in] init_arg Initialization argument for the worker
 * @return A worker process struct on success (for the parent). Null on errors
 */
extern worker_process *spawn_worker(void (init_func)(void *), void *init_arg);

/**
 * Send a key/value vector as a bytestream through a socket
 * @param[in] sd The socket descriptor to send to
 * @param kvv The key/value vector to send
 * @return The number of bytes sent, or -1 on errors
 */
extern int send_kvvec(int sd, struct kvvec *kvv);

/**
 * Create a short-lived string in stack-allocated memory
 * The number and size of strings is limited (currently to 256 strings of
 * 32 bytes each), so beware and use this sensibly. Intended for
 * number-to-string conversion.
 * @param[in] fmt The format string
 * @return A pointer to the formatted string on success. Undefined on errors
 */
extern const char *mkstr(const char *fmt, ...);

/**
 * Calculate the millisecond delta between two timeval structs
 * @param[in] start The start time
 * @param[in] stop The stop time
 * @return The millisecond delta between the two structs
 */
extern int tv_delta_msec(const struct timeval *start, const struct timeval *stop);

/**
 * Get timeval delta as seconds
 * @param start The start time
 * @param stop The stop time
 * @return time difference in fractions of seconds
 */
extern float tv_delta_f(const struct timeval *start, const struct timeval *stop);

/**
 * Set some common socket options
 * @param[in] sd The socket to set options for
 * @param[in] bufsize Size to set send and receive buffers to
 * @return 0 on success. < 0 on errors
 */
extern int set_socket_options(int sd, int bufsize);
#endif /* INCLUDE_worker_h__ */
