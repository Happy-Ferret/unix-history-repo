/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

# include "sendmail.h"

#ifndef lint
#ifdef SMTP
static char sccsid[] = "@(#)usersmtp.c	5.20.1.2 (Berkeley) %G% (with SMTP)";
#else
static char sccsid[] = "@(#)usersmtp.c	5.20.1.2 (Berkeley) %G% (without SMTP)";
#endif
#endif /* not lint */

# include <sysexits.h>
# include <errno.h>

# ifdef SMTP

/*
**  USERSMTP -- run SMTP protocol from the user end.
**
**	This protocol is described in RFC821.
*/

#define REPLYTYPE(r)	((r) / 100)		/* first digit of reply code */
#define REPLYCLASS(r)	(((r) / 10) % 10)	/* second digit of reply code */
#define SMTPCLOSING	421			/* "Service Shutting Down" */

char	SmtpMsgBuffer[MAXLINE];		/* buffer for commands */
char	SmtpReplyBuffer[MAXLINE];	/* buffer for replies */
char	SmtpError[MAXLINE] = "";	/* save failure error messages */
int	SmtpPid;			/* pid of mailer */
/*
**  SMTPINIT -- initialize SMTP.
**
**	Opens the connection and sends the initial protocol.
**
**	Parameters:
**		m -- mailer to create connection to.
**		pvp -- pointer to parameter vector to pass to
**			the mailer.
**
**	Returns:
**		appropriate exit status -- EX_OK on success.
**		If not EX_OK, it should close the connection.
**
**	Side Effects:
**		creates connection and sends initial protocol.
*/

jmp_buf	CtxGreeting;

smtpinit(m, mci, pvp)
	struct mailer *m;
	MCONINFO *mci;
	char **pvp;
{
	register int r;
	EVENT *gte;
	register STAB *st;
	char buf[MAXNAME];
	static int greettimeout();
	extern STAB *stab();
	extern MCONINFO *openmailer();

	/*
	**  Open the connection to the mailer.
	*/

	if (mci->mci_state == MCIS_OPEN)
		syserr("smtpinit: already open");

	mci->mci_state = MCIS_CLOSED;
	SmtpError[0] = '\0';
	SmtpPhase = "user open";
	setproctitle("%s %s: %s", CurEnv->e_id, pvp[1], SmtpPhase);
	mci = openmailer(m, pvp, (ADDRESS *) NULL, TRUE);
	if (SmtpPid < 0)
	{
		if (tTd(18, 1))
			printf("smtpinit: cannot open %s: stat %d errno %d\n",
			   pvp[0], ExitStat, errno);
		if (CurEnv->e_xfp != NULL)
		{
			register char *p;
			extern char *errstring();
			extern char *statstring();

			if (errno == 0)
			{
				p = statstring(ExitStat);
				fprintf(CurEnv->e_xfp,
					"%.3s %s.%s... %s\n",
					p, pvp[1], m->m_name, p);
			}
			else
			{
				r = errno;
				fprintf(CurEnv->e_xfp,
					"421 %s.%s... Deferred: %s\n",
					pvp[1], m->m_name, errstring(errno));
				errno = r;
			}
		}
		return (ExitStat);
	}
	mci->mci_state = MCIS_OPEN;

	/*
	**  Get the greeting message.
	**	This should appear spontaneously.  Give it five minutes to
	**	happen.
	*/

	if (setjmp(CtxGreeting) != 0)
		goto tempfail1;
	gte = setevent((time_t) 300, greettimeout, 0);
	SmtpPhase = "greeting wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	clrevent(gte);
	if (r < 0 || REPLYTYPE(r) != 2)
		goto tempfail1;

	/*
	**  Send the HELO command.
	**	My mother taught me to always introduce myself.
	*/

	smtpmessage("HELO %s", m, MyHostName);
	SmtpPhase = "HELO wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	if (r < 0)
		goto tempfail1;
	else if (REPLYTYPE(r) == 5)
		goto unavailable;
	else if (REPLYTYPE(r) != 2)
		goto tempfail1;

	/*
	**  If this is expected to be another sendmail, send some internal
	**  commands.
	*/

	if (bitnset(M_INTERNAL, m->m_flags))
	{
		/* tell it to be verbose */
		smtpmessage("VERB", m);
		r = reply(m, mci);
		if (r < 0)
			goto tempfail2;

		/* tell it we will be sending one transaction only */
		smtpmessage("ONEX", m);
		r = reply(m, mci);
		if (r < 0)
			goto tempfail2;
	}

	/*
	**  Send the HOPS command.
	**	This is non-standard and may give an "unknown command".
	**		This is not an error.
	**	It can give a "bad hop count" error if the hop
	**		count is exceeded.
	*/

	/*
	**  Send the MAIL command.
	**	Designates the sender.
	*/

	expand("\001<", buf, &buf[sizeof buf - 1], CurEnv);
	if (CurEnv->e_from.q_mailer == LocalMailer ||
	    !bitnset(M_FROMPATH, m->m_flags))
	{
		smtpmessage("MAIL From:<%s>", m, buf);
	}
	else
	{
		smtpmessage("MAIL From:<@%s%c%s>", m, MyHostName,
			buf[0] == '@' ? ',' : ':', buf);
	}
	SmtpPhase = "MAIL wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	if (r < 0 || REPLYTYPE(r) == 4)
		goto tempfail2;
	else if (r == 250)
		return (EX_OK);
	else if (r == 552)
		goto unavailable;

	/* protocol error -- close up */
	smtpquit(m, mci);
	return (EX_PROTOCOL);

  tempfail1:
	/* log this as an error to avoid sure-to-be-void connections */
	mci->mci_exitstat = EX_TEMPFAIL;
	mci->mci_errno = errno;

  tempfail2:
	/* signal a temporary failure */
	smtpquit(m, mci);
	return (EX_TEMPFAIL);

  unavailable:
	/* signal service unavailable */
	smtpquit(m, mci);
	return (EX_UNAVAILABLE);
}


static
greettimeout()
{
	/* timeout reading the greeting message */
	longjmp(CtxGreeting, 1);
}
/*
**  SMTPRCPT -- designate recipient.
**
**	Parameters:
**		to -- address of recipient.
**		m -- the mailer we are sending to.
**
**	Returns:
**		exit status corresponding to recipient status.
**
**	Side Effects:
**		Sends the mail via SMTP.
*/

smtprcpt(to, m, mci)
	ADDRESS *to;
	register MAILER *m;
	MCONINFO *mci;
{
	register int r;
	extern char *remotename();

	smtpmessage("RCPT To:<%s>", m, to->q_user);

	SmtpPhase = "RCPT wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	if (r < 0 || REPLYTYPE(r) == 4)
		return (EX_TEMPFAIL);
	else if (REPLYTYPE(r) == 2)
		return (EX_OK);
	else if (r == 550 || r == 551 || r == 553)
		return (EX_NOUSER);
	else if (r == 552 || r == 554)
		return (EX_UNAVAILABLE);
	return (EX_PROTOCOL);
}
/*
**  SMTPDATA -- send the data and clean up the transaction.
**
**	Parameters:
**		m -- mailer being sent to.
**		e -- the envelope for this message.
**
**	Returns:
**		exit status corresponding to DATA command.
**
**	Side Effects:
**		none.
*/

smtpdata(m, mci, e)
	struct mailer *m;
	register MCONINFO *mci;
	register ENVELOPE *e;
{
	register int r;

	/*
	**  Send the data.
	**	First send the command and check that it is ok.
	**	Then send the data.
	**	Follow it up with a dot to terminate.
	**	Finally get the results of the transaction.
	*/

	/* send the command and check ok to proceed */
	smtpmessage("DATA", m);
	SmtpPhase = "DATA wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	if (r < 0 || REPLYTYPE(r) == 4)
		return (EX_TEMPFAIL);
	else if (r == 554)
		return (EX_UNAVAILABLE);
	else if (r != 354)
		return (EX_PROTOCOL);

	/* now output the actual message */
	(*e->e_puthdr)(mci->mci_out, m, CurEnv);
	putline("\n", mci->mci_out, m);
	(*e->e_putbody)(mci->mci_out, m, CurEnv);

	/* terminate the message */
	fprintf(mci->mci_out, ".%s", m->m_eol);
	if (Verbose && !HoldErrs)
		nmessage(Arpa_Info, ">>> .");

	/* check for the results of the transaction */
	SmtpPhase = "result wait";
	setproctitle("%s %s: %s", CurEnv->e_id, CurHostName, SmtpPhase);
	r = reply(m, mci);
	if (r < 0 || REPLYTYPE(r) == 4)
		return (EX_TEMPFAIL);
	else if (r == 250)
		return (EX_OK);
	else if (r == 552 || r == 554)
		return (EX_UNAVAILABLE);
	return (EX_PROTOCOL);
}
/*
**  SMTPQUIT -- close the SMTP connection.
**
**	Parameters:
**		m -- a pointer to the mailer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sends the final protocol and closes the connection.
*/

smtpquit(mci, m)
	register MCONINFO *mci;
	register MAILER *m;
{
	int i;

	/* if the connection is already closed, don't bother */
	if (mci->mci_state == MCIS_CLOSED)
		return;

	/* send the quit message if not a forced quit */
	if (mci->mci_state == MCIS_OPEN || mci->mci_state == MCIS_SSD)
	{
		smtpmessage("QUIT", m);
		(void) reply(m, mci);
		if (mci->mci_state == MCIS_CLOSED)
			return;
	}

	/* now actually close the connection and pick up the zombie */
	i = endmailer(mci, m->m_argv[0]);
	if (i != EX_OK)
		syserr("smtpquit %s: stat %d", m->m_argv[0], i);
}
/*
**  REPLY -- read arpanet reply
**
**	Parameters:
**		m -- the mailer we are reading the reply from.
**
**	Returns:
**		reply code it reads.
**
**	Side Effects:
**		flushes the mail file.
*/

reply(mci, m)
	MCONINFO *mci;
	MAILER *m;
{
	(void) fflush(mci->mci_out);

	if (tTd(18, 1))
		printf("reply\n");

	/*
	**  Read the input line, being careful not to hang.
	*/

	for (;;)
	{
		register int r;
		register char *p;

		/* actually do the read */
		if (CurEnv->e_xfp != NULL)
			(void) fflush(CurEnv->e_xfp);	/* for debugging */

		/* if we are in the process of closing just give the code */
		if (mci->mci_state == MCIS_CLOSED)
			return (SMTPCLOSING);

		/* get the line from the other side */
		p = sfgets(SmtpReplyBuffer, sizeof SmtpReplyBuffer, mci->mci_in);
		if (p == NULL)
		{
			extern char MsgBuf[];		/* err.c */
			extern char Arpa_TSyserr[];	/* conf.c */

			/* if the remote end closed early, fake an error */
			if (errno == 0)
# ifdef ECONNRESET
				errno = ECONNRESET;
# else ECONNRESET
				errno = EPIPE;
# endif ECONNRESET

			message(Arpa_TSyserr, "reply: read error");
			/* if debugging, pause so we can see state */
			if (tTd(18, 100))
				pause();
# ifdef LOG
			syslog(LOG_INFO, "%s", &MsgBuf[4]);
# endif LOG
			mci->mci_state = MCIS_CLOSED;
			smtpquit(m);
			return (-1);
		}
		fixcrlf(SmtpReplyBuffer, TRUE);

		if (CurEnv->e_xfp != NULL && index("45", SmtpReplyBuffer[0]) != NULL)
		{
			/* serious error -- log the previous command */
			if (SmtpMsgBuffer[0] != '\0')
				fprintf(CurEnv->e_xfp, ">>> %s\n", SmtpMsgBuffer);
			SmtpMsgBuffer[0] = '\0';

			/* now log the message as from the other side */
			fprintf(CurEnv->e_xfp, "<<< %s\n", SmtpReplyBuffer);
		}

		/* display the input for verbose mode */
		if (Verbose && !HoldErrs)
			nmessage(Arpa_Info, "%s", SmtpReplyBuffer);

		/* if continuation is required, we can go on */
		if (SmtpReplyBuffer[3] == '-' || !isdigit(SmtpReplyBuffer[0]))
			continue;

		/* decode the reply code */
		r = atoi(SmtpReplyBuffer);

		/* extra semantics: 0xx codes are "informational" */
		if (r < 100)
			continue;

		/* reply code 421 is "Service Shutting Down" */
		if (r == SMTPCLOSING && mci->mci_state != MCIS_SSD)
		{
			/* send the quit protocol */
			mci->mci_state = MCIS_SSD;
			smtpquit(m);
		}

		/* save temporary failure messages for posterity */
		if (SmtpReplyBuffer[0] == '4' && SmtpError[0] == '\0')
			(void) strcpy(SmtpError, &SmtpReplyBuffer[4]);

		return (r);
	}
}
/*
**  SMTPMESSAGE -- send message to server
**
**	Parameters:
**		f -- format
**		m -- the mailer to control formatting.
**		a, b, c -- parameters
**
**	Returns:
**		none.
**
**	Side Effects:
**		writes message to mci->mci_out.
*/

/*VARARGS1*/
smtpmessage(f, m, mci, a, b, c)
	char *f;
	MAILER *m;
	MCONINFO *mci;
{
	(void) sprintf(SmtpMsgBuffer, f, a, b, c);
	if (tTd(18, 1) || (Verbose && !HoldErrs))
		nmessage(Arpa_Info, ">>> %s", SmtpMsgBuffer);
	if (mci->mci_out != NULL)
		fprintf(mci->mci_out, "%s%s", SmtpMsgBuffer,
			m == 0 ? "\r\n" : m->m_eol);
}

# endif SMTP
