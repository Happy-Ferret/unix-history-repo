/*	tftp.h	4.1	82/08/16	*/

/*
 * Trivial File Transfer Protocol (IEN-133)
 */
#define	SEGSIZE		512		/* data segment size */
#define	TIMEOUT		5		/* retransmits every 5 seconds */
#define	MAXTIMEOUT	(5*TIMEOUT)	/* abort if no success by then */

/*
 * Packet types.
 */
#define	RRQ	01			/* read request */
#define	WRQ	02			/* write request */
#define	DATA	03			/* data packet */
#define	ACK	04			/* acknowledgement */
#define	ERROR	05			/* error code */

struct	tftphdr {
	short	th_opcode;		/* packet type */
	union {
		short	tu_block;	/* block # */
		short	tu_code;	/* error code */
		char	tu_stuff[1];	/* request packet stuff */
	} th_u;
	char	th_data[1];		/* data or error string */
};

#define	th_block	th_u.tu_block
#define	th_code		th_u.tu_code
#define	th_stuff	th_u.tu_stuff
#define	th_msg		th_data

/*
 * Error codes.
 */
#define	EUNDEF		0		/* not defined */
#define	ENOTFOUND	1		/* file not found */
#define	EACCESS		2		/* access violation */
#define	ENOSPACE	3		/* disk full or allocation exceeded */
#define	EBADOP		4		/* illegal TFTP operation */
#define	EBADID		5		/* unknown transfer ID */
#define	EEXISTS		6		/* file already exists */
#define	ENOUSER		7		/* no such user */
