/**
 * @file sctpAssociate.c
 * @provides sctpAssociate.
 *
 * $Id$
 */

#include <stddef.h>
#include <stdlib.h>
#include <network.h>
#include <mailbox.h>
#include <sctp.h>

static void sctpInitTimeout(void *args);
static void sctpCookieTimeout(void *args);

/**
 * Associate an SCTP instance with another endpoint. `instance` must be a
 * valid SCTP instance.  `remoteaddr` and `remotept` define the destination
 * transport address. `nos` defines the number of output streams requested.
 * NOTE: Xinu has a 1-1 correspondence between SCTP instances and
 * associations.
 * NOTE: This is a BLOCKING call, on return the SCTP instance will be
 * populated with all possible remote addresses and nos may be less than
 * the requested amount. The user must confirm that this is sufficient.
 * See RFC 4960, pg. 111--112.
 * @param *instance Initialized SCTP instance
 * @param *remoteaddr Destination network address
 * @param remotept Destination transport protocol port
 * @param nos Number of output streams
 * @return NULL if there is an error, `instance` on success
 */
struct sctp *sctpAssociate(struct sctp *instance, struct netaddr *remoteaddr,
                           int remotept, int nos)
{
	int status;
	struct sctpChunkInit init_chunk;
	struct sctpChunkCookieEcho cookie_chunk;
	struct sctpTimer *timer;

	/* Make sure instance is valid */
	if (NULL == instance || SCTP_DEV_FREE == instance->dev_state)
	{
		return NULL;
	}

	/* "create" TCB (already allocated durning Initialize) */
	instance->my_tag = rand();
	instance->remoteport = remotept;
	memcpy(&instance->remoteip[0], remoteaddr, sizeof(*remoteaddr));
	instance->primary_path = 0;

	/* send INIT message */
    init_chunk.head.type = SCTP_TYPE_INIT;
    init_chunk.head.flags = 0;
    init_chunk.head.length = sizeof(init_chunk);
	init_chunk.init_tag = instance->my_tag;
	init_chunk.a_rwnd = 1; //XXX Figure this out
	init_chunk.n_out_streams = nos;
	init_chunk.n_in_streams = SCTP_MAX_STREAMS; // XXX ???
	init_chunk.init_tsn = 1; //XXX Figure this out
    sctpOutput(instance, &init_chunk, sizeof(init_chunk));

	/* start init timer for timeout */
	// XXX: Find value for timeout (first param)
	timer = sctpTimerStart(10, 0, &sctpInitTimeout, instance);

	/* state -> COOKIE_WAIT */
	instance->state = SCTP_STATE_COOKIE_WAIT;

	/* wait for INIT ACK */
	status = mailboxReceive(instance->signal);
	if (status != SCTP_TYPE_INIT_ACK)
	{
		SCTP_TRACE("Received bad status: %d", status);
	}

	/* stop init timer */
	sctpTimerCancel(timer);

	// XXX Where do I get the sent cookie from? tcb? mailbox?
	/* send COOKIE_ECHO message */
	//memcpy(&cookie_chunk.cookie, cookie, sizeof(cookie));
    cookie_chunk.head.type = SCTP_TYPE_COOKIE_ECHO;
    cookie_chunk.head.flags = 0;
    cookie_chunk.head.length = sizeof(init_chunk);
    sctpOutput(instance, &cookie_chunk, sizeof(cookie_chunk));

	/* start cookie timer */
    // XXX: find value for timeout (first param)
	timer = sctpTimerStart(10, 0, &sctpCookieTimeout, instance);

	/* state -> COOKIE_ECHOED */
	instance->state = SCTP_STATE_COOKIE_ECHOED;

	/* wait for COOKIE ACK */
	status = mailboxReceive(instance->signal);
	if (status != SCTP_TYPE_COOKIE_ACK)
	{
		SCTP_TRACE("Received bad status: %d", status);
	}

	/* stop cookie timer */
	sctpTimerCancel(timer);

	/* state -> ESTABLISHED */
	instance->state = SCTP_STATE_ESTABLISHED;

	// XXX: Do we support callbacks?
	//(*instance->callback)(SCTP_ULP_CONNECTION_UP);

    return instance;
}

static void sctpInitTimeout(void *args)
{
    SCTP_TRACE("Timeout: 0x%08x", args);
}

static void sctpCookieTimeout(void *args)
{
    SCTP_TRACE("Timeout: 0x%08x", args);
}
