/*	kernel.h	6.2	85/06/07	*/

/*
 * Global variables for the kernel
 */

long	rmalloc();

/* 1.1 */
long	hostid;
char	hostname[MAXHOSTNAMELEN];
int	hostnamelen;

/* 1.2 */
struct	timeval boottime;
struct	timeval time;
struct	timezone tz;			/* XXX */
int	hz;
int	phz;				/* alternate clock's frequency */
int	tick;
int	lbolt;				/* awoken once a second */
int	realitexpire();

double	avenrun[3];

#ifdef GPROF
extern	int profiling;
extern	char *s_lowpc;
extern	u_long s_textsize;
extern	u_short *kcount;
#endif
