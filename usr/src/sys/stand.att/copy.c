/*	copy.c	4.2	83/02/16	*/

/*
 * Copy from to in 10K units.
 * Intended for use in system
 * installation.
 */
main()
{
	int from, to;
	char fbuf[50], tbuf[50];
	char buffer[10240];
	register int record;
	extern int errno;

	from = getdev("From", fbuf, 0);
	to = getdev("To", tbuf, 1);
	for (record = 0; ; record++) {
		int rcc, wcc;

		rcc = read(from, buffer, sizeof (buffer));
		if (rcc == 0)
			break;
		if (rcc < 0) {
			printf("Read error, errno=%d\n", errno);
			break;
		}
		if (rcc != sizeof (buffer))
			printf("Record %d: read short; expected %d, got %d\n",
				sizeof (buffer), rcc);
		wcc = write(to, buffer, rcc);
		if (wcc < 0) {
			printf("Write error: errno=%d\n", errno);
			break;
		}
		if (wcc != rcc) {
			printf("Write short; expected %d, got %d\n", rcc, wcc);
			break;
		}
	}
	printf("Copy completed: %d records copied\n", record);
	/* can't call exit here */
}

getdev(prompt, buf, mode)
	char *prompt, *buf;
	int mode;
{
	register int i;

	do {
		printf("%s: ", prompt);
		gets(buf);
		i = open(buf, mode);
	} while (i <= 0);
	return (i);
}
