/* EMACS_MODES: !fill,tabstop=4,c */
/*
 * save and restore routines
 *
 * @(#)save.c	9.0	(rdk)	 7/17/84
 */

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "rogue.h"
#include "rogue.ext"

#ifdef BSD
#define srand48(seed)	srandom(seed)
#endif

EXTCHAR *sys_errlist[], version[];
EXTCHAR *ctime();
EXTINT errno;

typedef struct stat STAT;
STAT sbuf;

/*
 * ignore:
 *	Ignore ALL signals possible
 */
ignore()
{
	int i;

	for (i = 0; i < NSIG; i++)
		signal(i, SIG_IGN);
}

/*
 * save_game:
 *	Save the current game
 */
save_game()
{
	reg FILE *savef;
	reg int c;
	char buf[LINLEN];

	mpos = 0;
	if (file_name[0] != NULL) {
		msg("Save file (%s)? ", file_name);
		do {
			c = getchar();
			if(c == ESCAPE) {
				msg("");
				return FALSE;
			}
		} while (c != 'n' && c != 'y');
		mpos = 0;
		if (c == 'y')
			goto gotfile;
	}
	msg("File name: ");
	mpos = 0;
	buf[0] = NULL;
	if (get_str(buf, cw) == QUIT) {
		msg("");
		return FALSE;
	}
	strcpy(file_name, buf);
gotfile:
	c = dosave();		/* try to save this game */
	if (c == FALSE)
		msg("Could not save game to file %s", file_name);
	return c;
}

/*
 * auto_save:
 *	Automatically save a game
 */
auto_save()
{
	dosave();		/* save this game */
	byebye(1);		/* so long for now */
}

/*
 * game_err:
 *	When an error occurs. Set error flag and save game.
 */
game_err()
{
	int ok;

	ok = dosave();			/* try to save this game */
	clear();
	refresh();
	endwin();

	printf("\nInternal error !!!\n\nYour game was ");
	if (ok)
		printf("saved.");
	else
		printf("NOT saveable.");

	printf("\n\nTo assist in solving this error, do this:\n\n");
	printf("\tmv core YOUR_ID.core\n");
	printf("\tuucp ihnp4!akgua!bcs3b2!rdk!YOUR_ID.core YOUR_ID.core\n");
	fflush(stdout);

	signal(SIGIOT, SIG_DFL);	/* allow core dump signal */
	abort();			/* cause core dump */
	byebye(3);
}

/*
 * dosave:
 *	Set UID back to user and save the game
 */
dosave()
{
	FILE *savef;

	ignore();
	setuid(playuid);
	setgid(playgid);
	umask(022);

	if (file_name[0] != NULL) {
		if ((savef = fopen(file_name,"w")) != NULL)
		{
			save_file(savef);
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * save_file:
 *	Do the actual save of this game to a file
 */
save_file(savef)
FILE *savef;
{
	reg int fnum;

	/*
	 * force allocation of the buffer now so that inodes, etc
	 * can be checked when restoring saved games.
	 */
	fnum = fileno(savef);
	fstat(fnum, &sbuf);
	write(fnum, "RDK", 4);
	lseek(fnum, 0L, 0);
	write(fnum, version, (unsigned int) (sbrk(0) - version));
	close(fnum);
	signal(SIGINT, byebye);
	signal(SIGQUIT, byebye);
	wclear(cw);
	draw(cw);
}

/*
 * restore:
 *	Restore a saved game from a file
 */
restore(file, envp)
char *file, **envp;
{
	register inf, pid;
	int ret_status;
	extern char **environ;
	char buf[LINLEN];
	STAT sbuf2;

	if ((inf = open(file, 0)) < 0) {
		printf("Cannot read save game %s\n",file);
		return FALSE;
	}

	fflush(stdout);
	read(inf, buf, strlen(version) + 1);

	if (strcmp(buf, version) != 0) {
		printf("Sorry, saved game version is out of date.\n");
		return FALSE;
	}

	fstat(inf, &sbuf2);
	fflush(stdout);
	brk(version + sbuf2.st_size);
	lseek(inf, 0L, 0);
	read(inf, version, (unsigned int) sbuf2.st_size);

	/*
	 * we do not close the file so that we will have a hold of the
	 * inode for as long as possible
	 */
	if (!wizard)
	{
		if(sbuf2.st_ino!=sbuf.st_ino || sbuf2.st_dev!=sbuf.st_dev) {
			printf("Sorry, saved game is not in the same file.\n");
			return FALSE;
		}
	}

	mpos = 0;
	mvwprintw(cw, 0, 0, "%s: %s", file, ctime(&sbuf2.st_mtime));

	/* defeat multiple restarting from the same place */

	if (!wizard)
	{
		if (sbuf2.st_nlink != 1)
		{
			printf("Cannot restore from a linked file\n");
			return FALSE;
		}
		else
		{
			while((pid = fork()) < 0)
				sleep(1);

			/* set id to unlink file */
			if(pid == 0)
			{
				setuid(playuid);
				setgid(playgid);
				unlink(file);
				exit(0);
			}
			/* wait for unlink to finish */
			else
			{
				while(wait(&ret_status) != pid)
					continue;
				if (ret_status < 0)
				{
					printf("Cannot unlink file\n");
					return FALSE;
				}
			}
		}
	}
	environ = envp;
/*
	if (!My_term && isatty(2)) {
		struct screen *sp;
		struct screen *set_term();
		char *sp;

		gettmode();

		if ((sp = getenv("TERM")) == NULL)
			sp = Def_term;

		setterm(sp);
		setterm("q1");
		_tty_ch = 2;

		sp = newterm(getenv("TERM"), stdout);
		set_term(sp);
	}
	else
		setterm(Def_term);
*/

	strcpy(file_name, file);
	setup();
	restscr(cw);
	srand48(getpid());
	playit();
}
