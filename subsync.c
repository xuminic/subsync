
/*  subsync.c -- resync the subtitle's time stamps
    Copyright (C) 2009-2017  "Andy Xuming" <xuming@users.sourceforge.net>

    This file is part of Subsync, a utility to resync subtitle files

    Subsync is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Subsync is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct	ScRate	{
	char	*id;
	double	fact;
} srtbl[6] = {
	{ "N-P",	1.1988 },	/* NTSC to PAL frame rate 29.97/25 */
	{ "P-N",	0.83417 },	/* PAL to NTSC frame rate 25/29.97 */
	{ "N-C",	1.25 },	/* NTSC to Cinematic frame rate 29.97/23.976 */
	{ "C-N",	0.8 },	/* Cinematic to NTSC frame rate 23.976/29.97 */
	{ "P-C",	1.04271 }, /* PAL to Cinematic frame rate 25/23.976 */
	{ "C-P",	0.95904 },  /* Cinematic to PAL frame rate 23.976/25 */
};

char	*subsync_help = "\
usage: subsync [OPTION] [sutitle_file]\n\
OPTION:\n\
  -c, --chop N:M        chop the specified number of subtitles (from 1)\n\
  -o,/--overwrite       overwrite the original file (/with backup file)\n\
  -r, --reorder [NUM]   reorder the serial number (SRT only)\n\
  -s, --span TIME ...   specifies the span of the time stamps for processing\n\
  -w, --write FILENAME  write to the specified file\n\
      -/+OFFSET         specifies the offset of the time stamps\n\
      -SCALE            specifies the scale ratio of the time stamps\n\
      --help, --version\n\
      --help-example\n\
OFFSET:\n\
  Time stamp offset; the prefix '+' or '-' defines delay or bring forward.\n\
  It can be defined by milliseconds: +19700, -10000\n\
  or by time stamp noting HH:MM:SS.MS: -0:0:10,199, +1:0:12.66\n\
  or by time stamp subtraction, the expect time stamp minus the actual\n\
  time stamp, for example: +01:44:31,660-01:44:36,290\n\
SCALE:\n\
  Time stamp scaling ratio; tweak the time stamp from different frame rates,\n\
  for example, between  PAL(25), NTSC(29.97) and Cinematic(23.976).\n\
  It can be defined by real number: 1.1988; or by predefined identifiers:\n\
  N-P(1.1988), P-N(0.83417), N-C(1.25), C-N(0.8), P-C(1.04271), C-P(0.95904)\n\
  or by time stamp dividing, the expect time stamp divided by the actual\n\
  time stamp, for example: -01:44:30,290/01:44:31,660\n\
";

char	*subsync_help_extra = "\
Debug Options:\n\
      --help-subtract   calculate the time offset\n\
      --help-divide     calculate the scale ratio of time stamps\n\
      --help-strtoms    test reading the time stamps\n\
      --help-debug      display the internal arguments\n\
      --help-example    display the example\n\
";

char	*subsync_help_example = "\
Examples:\n\
  Delay the subtitles for 12 seconds:\n\
    subsync +12000 source.ass > target.ass\n\
  Bring forward the subtitles for 607570 milliseconds:\n\
    subsync -00:10:07,570 source.ass > target.ass\n\
  Shifting the subtitles by (expected - actual) time stamps:\n\
    subsync +00:00:52,570-0:11:00,140 source.ass > target.ass\n\
  Which is identical to:\n\
    subsync -00:00:52,570-0:11:00,140 -w target.ass source.ass\n\
  Zooming the time stamps of the subtitles with a scale ratio of 1.000955:\n\
    subsync -1.000955 -w target.ass source.ass\n\
  Which is identical to (expected / actual) time stamps:\n\
    subsync -01:35:32,160/1:35:26,690 source.ass > target.ass\n\
  Shifting the subtitles and zoom its intervals, print in screen:\n\
    subsync +00:00:52,570-0:11:00,140 -01:35:32,160/1:35:26,690 source.ass\n\
  Shifting the subtitles from 1 minute 15 seconds to the end:\n\
    subsync -s 0:01:15.00 -00:01:38,880-0:03:02.50 source.ass > target.ass\n\
  Batch shifting the subtitles and overwrite the original files:\n\
    subsync -00:00:01,710-00:01:25,510 -o *.srt\n\
";

char	*subsync_version = "Subsync 0.11.0 \
Copyright (C) 2009-2017  \"Andy Xuming\" <xuming@sourceforge.net>\n\
This program comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it under certain\n\
conditions. For details see see `COPYING'.\n";

time_t	tm_offset = 0;
double	tm_scale = 0.0;
time_t	tm_range[2] = { -1, -1 };
int	tm_chop[2] = { -1, -1 };
int	tm_srtsn = -1;		/* -1: not to orderize SRT sn  */
int	tm_overwrite = 0;	/* 1: overwrite  2: overwrite and backup */

static int retiming(FILE *fin, FILE *fout);
static time_t tweaktime(time_t ms);
static int chop_filter(char *s, int *magic);
static time_t strtoms(char *s, int *len, int *style);
static char *mstostr(time_t ms, int style);
static double arg_scale(char *s);
static time_t arg_offset(char *s);
static int isnumber(char *s);
static int help_tools(int argc, char **argv);
static void test_str_to_ms(void);

#define MOREARG(c,v)	{	\
	--(c), ++(v); \
	if (((c) == 0) || (**(v) == '-') || (**(v) == '+')) { \
		fprintf(stderr, "missing parameters\n"); \
		return -1; \
	}\
}


int main(int argc, char **argv)
{
	FILE	*fin = NULL, *fout = NULL;
	char	*oname;

	while (--argc && ((**++argv == '-') || (**argv == '+'))) {
		if (!strcmp(*argv, "-V") || !strcmp(*argv, "--version")) {
			printf(subsync_version);
			return 0;
		} else if (!strcmp(*argv, "-H") || !strcmp(*argv, "--help")) {
			puts(subsync_help);
			return 0;
		} else if (!strncmp(*argv, "--help-", 7)) {
			return help_tools(argc, argv);
		} else if (!strcmp(*argv, "-o")) {
			tm_overwrite = 1;	/* no backup */
		} else if (!strcmp(*argv, "--overwrite")) {
			tm_overwrite = 2;	/* has backup */
		} else if (!strcmp(*argv, "-c") || !strcmp(*argv, "--chop")) {
			MOREARG(argc, argv);
			if (sscanf(*argv, "%d : %d", tm_chop, tm_chop + 1) != 2) {
				tm_chop[0] = tm_chop[1] = -1;
			}
		} else if (!strcmp(*argv, "-r") || !strcmp(*argv, "--reorder")) {
			if ((argc > 0) && isnumber(argv[1])) {
				--argc;	tm_srtsn = (int)strtol(*++argv, NULL, 0);
			} else {
				tm_srtsn = 1;	/* set as default */
			}
		} else if (!strcmp(*argv, "-s") || !strcmp(*argv, "--span")) {
			MOREARG(argc, argv);
			tm_range[0] = arg_offset(*argv);
			/* the second parameter is optional, must begin in number */
			if ((argc > 0) && isdigit(argv[1][0])) {
				--argc; tm_range[1] = arg_offset(*++argv);
			}
		} else if (!strcmp(*argv, "-w") || !strcmp(*argv, "--write")) {
			MOREARG(argc, argv);
			if ((fout = fopen(*argv, "w")) == NULL) {
				perror(*argv);
			}
		} else if (!strcmp(*argv, "--")) {
			break;
		} else if (arg_offset(*argv) != -1) {
			tm_offset = arg_offset(*argv);
		} else if (arg_scale(*argv) != 0) {
			tm_scale = arg_scale(*argv);
		} else {
			fprintf(stderr, "%s: unknown parameter.\n", *argv);
			return -1;
		}
	}
	if ((tm_offset == 0) && (tm_scale == 0) && (tm_srtsn < 0) && 
			(tm_chop[0] < 0) && (tm_chop[1] < 0)) {
		puts(subsync_help);
		return 0;
	}

	/* input from stdin */
	if ((argc == 0) || !strcmp(*argv, "--")) {
		if (fout == NULL) {
			retiming(stdin, stdout);
		} else {
			retiming(stdin, fout);
			fclose(fout);
		}
		return 0;
	}

	/* don't overwrite but still batch processing 
	 * what's the point of this ??? */
	if (!tm_overwrite) {
		if (fout == NULL) {
			fout = stdout;
		}
		for ( ; argc; argc--, argv++) {
			if ((fin = fopen(*argv, "r")) == NULL) {
				perror(*argv);
				continue;
			}	
			retiming(fin, fout);
			fclose(fin);
		}
		if (fout != stdout) {
			fclose(fout);
		}
		return 0;
	}

	/* the overwrite option override the write option */
	if (fout != NULL) {
		fclose(fout);
	}

	/* 20180912 Using Unix trick to preserve the backup file
	 * Hope it's portable to Windows */
	for ( ; argc; argc--, argv++) {
		if ((fin = fopen(*argv, "r")) == NULL) {
			perror(*argv);
			continue;
		}
		if ((oname = malloc(strlen(*argv)+16)) == NULL) {
			fclose(fin);
			continue;
		}
		strcpy(oname, *argv);
		strcat(oname, ".bak");

		/* rename the original file now since the actual content are 
		 * still accessible via the file handler. */
		rename(*argv, oname);

		/* create the output file by its original name, though it
		 * has different i-node to the input file */
		if ((fout = fopen(*argv, "w")) == NULL) {
			perror(*argv);
			free(oname);
			fclose(fin);
			continue;
		}
		retiming(fin, fout);
		fclose(fout);
		fclose(fin);

		if (tm_overwrite == 1) {
			unlink(oname);
		}
		free(oname);
	}
	return 0;
}

static int retiming(FILE *fin, FILE *fout)
{
	char	buf[2048], *s;
	time_t	ms;
	int	n, style, srtsn;
	int	magic = -1;		/* -1: uncertain 0: SRT 1: SSA */

	/* seperate the Byte order mark (utf8/EF BB BF) if exists */
	buf[0] = (char)fgetc(fin);
	if ((unsigned char)buf[0] != 0xef) {
		ungetc((unsigned char)buf[0], fin);
	} else {
		buf[1] = (char)fgetc(fin);
		buf[2] = (char)fgetc(fin);
		fwrite(buf, 1, 3, fout);
	}

	srtsn = tm_srtsn;
	while (fgets(buf, sizeof(buf)-1, fin)) {
		if (chop_filter(buf, &magic)) {
			continue;	/* skip the specified subtitles */
		}

		/* skip and output the whitespaces */
		for (s = buf; (*s > 0) && (*s <= 0x20); s++) fputc(*s, fout);
		
		/* SRT: 00:02:17,440 --> 00:02:20,375
		 * ASS: Dialogue: Marked=0,0:02:42.42,0:02:44.15,Wolf main,
		 *           autre,0000,0000,0000,,Toujours rien. */
		if (!strncmp(s, "Dialogue:", 9)) {	/* ASS/SSA timestamp */
			/* output everything before the first timestamp */
			while (*s != ',') fputc(*s++, fout);
			/* output the ',' also */
			fputc(*s++, fout);
			/* read and skip the first timestamp */
			ms = strtoms(s, &n, &style);
			s += n;
			/* output the tweaked timestamp */
			fputs(mstostr(tweaktime(ms), style), fout);
			/* output everything before the second timestamp */
			while (*s != ',') fputc(*s++, fout);
			/* output the ',' also */
			fputc(*s++, fout);
			/* read and skip the second timestamp */
			ms = strtoms(s, &n, &style);
			s += n;
			/* output the tweaked timestamp */
			fputs(mstostr(tweaktime(ms), style), fout);
		} else if ((ms = strtoms(s, &n, &style)) != -1) {	/* SRT timestamp */
			/* skip the first timestamp */
			s += n;
			/* output the tweaked timestamp */
			fputs(mstostr(tweaktime(ms), style), fout);

			/* output everything before the second timestamp */
			while (!isdigit(*s)) fputc(*s++, fout);
			/* read and skip the second timestamp */
			ms = strtoms(s, &n, &style);
			s += n;
			/* output the tweaked timestamp */
			fputs(mstostr(tweaktime(ms), style), fout);
		} else if ((srtsn > 0) && isnumber(s)) {
			/* SRT serial numbers to be re-ordered */
			fprintf(fout, "%d", srtsn++);
			while (isdigit(*s)) s++;
		} 
		/* output rest of things */
		fputs(s, fout);
	}
	return 0;
}

static time_t tweaktime(time_t ms)
{
	if (tm_range[0] > -1) {	/* check the time stamp range */
		if (ms < tm_range[0]) {
			return ms;
		}
		if ((tm_range[1] > -1) && (ms > tm_range[1])) {
			return ms;
		}
	}
	if (tm_offset) {
		ms += tm_offset;
	}
	if (tm_scale != 0.0) {
		ms *= tm_scale;
	}
	return ms;
}

static int chop_filter(char *s, int *magic)
{
	static	int	subidx;

	if ((tm_chop[0] < 0) && (tm_chop[1] < 0)) {
		return 0;	/* disabled */
	}

	switch (*magic) {
	case 0:			/* subrip */
		if (isnumber(s)) {
			subidx++;
		}
		//printf("SRT %d\n", subidx);
		if ((tm_chop[0] > 0) && (subidx < tm_chop[0])) {
			break;	/* no chop */
		}
		if ((tm_chop[1] > 0) && (subidx > tm_chop[1])) {
			break;	/* no chop */
		}
		return 1;
	case 1:			/* ASS/SSA */
		if (strncmp(s, "Dialogue:", 9)) {
			break;;
		}
		subidx++;
		//printf("ASS %d\n", subidx);
		if ((tm_chop[0] > 0) && (subidx < tm_chop[0])) {
			break;	/* no chop */
		}
		if ((tm_chop[1] > 0) && (subidx > tm_chop[1])) {
			break;	/* no chop */
		}
		return 1;
	default:
		if (*magic > 0) {
			break;	/* something wrong */
		}
		if (isnumber(s)) {
			*magic = 0;
			subidx++;
		} else if (strtoms(s, NULL, NULL) != -1) {       /* SRT timestamp */
			*magic = 0;
			subidx++;
		} else if (!strncmp(s, "[Events]", 8)) {
			*magic = 1;
			break;
		} else if (!strncmp(s, "[Script Info]", 13)) {
			*magic = 1;
			break;
		} else if (!strncmp(s, "Dialogue:", 9)) {
			*magic = 1;
			subidx++;
		} else {
			break;
		}
		if ((tm_chop[0] > 0) && (subidx < tm_chop[0])) {
			break;	/* no chop */
		}
		if ((tm_chop[1] > 0) && (subidx > tm_chop[1])) {
			break;	/* no chop */
		}
		return 1;
	}
	return 0;	/* no skip */
}

static time_t strtoms(char *s, int *len, int *style)
{
	char	*pattern[] = {
		"%d : %d : %d , %d%n",		/* SRT */
		"%d : %d : %d . %d%n",		/* ASS/SSA */
		"%d : %d : %d : %d%n",
		"%d . %d . %d . %d%n",
		"%d - %d - %d - %d%n",
		NULL };
	int	i, hour, min, sec, msec, num, type;

	if (len) {
		*len = 0;
	}

	type = 0;
	if ((*s == '+') || (*s == '-')) {
		type = *s++;
	}
	for (i = 0; pattern[i]; i++) {
		if (sscanf(s, pattern[i], &hour, &min, &sec, &msec, &num) == 4) {
			break;
		}
	}
	//printf("%s:  %d-%d-%d-%d (%d)\n", s, hour, min, sec, msec, num);
	if (!pattern[i]) {
		return -1;	/* parameters not match */
	}
	
	if ((min < 0) || (min > 59)) {
		return -1;
	}
	if ((sec < 0) || (sec > 59)) {
		return -1;
	}

	/* special case: ASS/SSA uses centiseconds */
	if (i == 1) {
		if ((msec < 0) || (msec > 99)) {
			return -1;
		}
		msec *= 10;
	} else {
		if ((msec < 0) || (msec > 999)) {
			return -1;
		}
	}

	if (len) {
		*len = num;
	}
	if (style) {
		*style = i;
	}

	/* convert to seconds */
	sec += hour * 3600 + min * 60;
	if (type == '-') {
		return - ((time_t)sec * 1000 + msec);
	}
	return (time_t)sec * 1000 + msec;
}

static char *mstostr(time_t ms, int style)
{
	static	char	stmp[32];
	char	*buf = stmp;
	int	hh, mm, ss;

	if (ms < 0) {
		ms = -ms;
		*buf++ = '-';
	}

	hh = (int)(ms / 3600000L);
	ms %= 3600000L;
	mm = (int)(ms / 60000);
	ms %= 60000;
	ss = (int)(ms / 1000);
	ms %= 1000;

	switch (style) {
	case 1:		/* ASS */
		sprintf(buf, "%d:%02d:%02d.%02ld", hh, mm, ss, ms / 10);
		break;
	case 2:
		sprintf(buf, "%02d:%02d:%02d:%03ld", hh, mm, ss, ms);
		break;
	case 3:
		sprintf(buf, "%02d.%02d.%02d.%03ld", hh, mm, ss, ms);
		break;
	case 4:
		sprintf(buf, "%02d-%02d-%02d-%03ld", hh, mm, ss, ms);
		break;
	case 0:		/* SRT */
	default:
		sprintf(buf, "%02d:%02d:%02d,%03ld", hh, mm, ss, ms);
		break;
	}
	return stmp;
}

/* valid parameters:
 * [+-]N-P, [+-]P-N, [+-]N-C, [+-]C-N, [+-]P-C, [+-]C-P, [+-]0.1234
 * [+-]01:44:30,290/01:44:31,660
 * Note that all leading '+' and '-' are ignored because ratio is a scalar.
 */
static double arg_scale(char *s)
{
	int	i;
	double	tmp;

	/* skip the leading '+' or '-' */
	if ((*s == '+') || (*s == '-')) {
		s++;
	}
	/* search the identity table first for something like "N-P" */
	for (i = 0; i < sizeof(srtbl)/sizeof(struct ScRate); i++) {
		if (!strcmp(s, srtbl[i].id)) {
			return srtbl[i].fact;
		}
	}
	/* or calculate the scale ratio by the form of 
	 *  01:44:30,290/01:44:31,660 */
	if (strchr(s, '/')) {
		time_t	mf, mt;

		if ((mf = strtoms(s, NULL, NULL)) == -1) {
			return 0.0;
		}
		s = strchr(s, '/');
		if ((mt = strtoms(++s, NULL, NULL)) == -1) {
			return 0.0;
		}
		return (double)mf / (double)mt;
	}
	/* or it's just a simple real number: 1.2345E12 */
	if (strchr(s, '.')) {
		char	*endp;

		tmp = strtod(s, &endp);
		if (*endp == 0) {
			return tmp;
		}
	}
	return 0.0;
}

/* valid parameters:
 * [+-]01:44:30,290, [+-]134600, [+-]01:44:31,660-01:44:30,290
 * Note that all leading '+' and '-' are required for vectoring
 */
static time_t arg_offset(char *s)
{
	char	*endp;
	time_t	ms;

	/* ignore the form of 01:44:30,290/01:44:31,660 because it's for scaling */
	if (strchr(s, '/')) {
		return -1;
	}
	/* seperate the form -01:44:31,660-01:44:30,290 from -01:44:31,660 */
	if (strchr(s+1, '-')) {
		s++;	/* ignore the switch charactor '+' or '-' */
		if ((ms = strtoms(s, NULL, NULL)) == -1) {
			return -1;
		}
		s = strchr(s, '-');
		if (strtoms(++s, NULL, NULL) == -1) {
			return -1;
		}
		ms -= strtoms(s, NULL, NULL);
		return ms;
	}
	/* process the form of [+-]01:44:31,660 */
	if ((ms = strtoms(s, NULL, NULL)) != -1) {
		return ms;
	}
	/* or it's simply a number by milliseconds [+-]134600 */
	ms = strtol(s, &endp, 0);
	if (*endp == 0) {
		return ms;
	}
	return -1;
}

static int isnumber(char *s)
{
	if (!isdigit(*s)) {
		return 0;
	}
	while (isdigit(*s)) s++;
	return (*s > 0x20) ? 0 : 1;
}

static int help_tools(int argc, char **argv)
{
	time_t	ms;
	double	tmp;

	if (!strcmp(*argv,  "--help-strtoms")) {
		test_str_to_ms();
	} else if (!strncmp(*argv, "--help-subtract", 10)) {
		if (argc < 3) {
			fprintf(stderr, "Two time stamps required.\n");
			return 1;
		}
		ms = arg_offset(argv[1]);
		ms -= arg_offset(argv[2]);
		printf("Time difference is %s (%ld ms)\n", mstostr(ms, 0), ms);
	} else if (!strncmp(*argv, "--help-divide", 10)) {
		if (argc < 3) {
			fprintf(stderr, "Two time stamps required.\n");
			return 1;
		}
		ms = arg_offset(argv[1]);
		tmp = (double)ms / (double)arg_offset(argv[2]);
		printf("Time scale ratio is %f\n", tmp);
	} else if (!strcmp(*argv, "--help-debug")) {
		printf("Time Stamp Offset:   %ld\n", tm_offset);
		printf("Time Stamp Scaling:  %f\n", tm_scale);
		printf("Time Stamp range:    from %ld to %ld\n", tm_range[0], tm_range[1]);
		printf("SRT serial Number:   from %d\n", tm_srtsn);
		printf("Subtitle chopping:   from %d to %d\n", tm_chop[0], tm_chop[1]);
	} else if (!strcmp(*argv, "--help-example")) {
		puts(subsync_help_example);
	} else {
		puts(subsync_help_extra);
	}
	return 0;
}

static void test_str_to_ms(void)
{
	int	i, n, style;
	time_t	ms;
	char	*testbl[] = {
		"00:02:09,996",
		"12:34:56,789",
		"1,2;3-456",
		"::5:123",
		"12",
		"12,3",
		"12,,,345",
		"  12 : 34 : 56 : 789 ",
		" +12:34:56,789",
		" + 12:34:56,789",
		" -12:34:56,789",
		"::::",
		NULL
	};

	for (i = 0; testbl[i]; i++) {
		ms = strtoms(testbl[i], &n, &style);
		printf("%s(%d): %s =%ld\n", testbl[i], n, mstostr(ms, style), ms);
	}
}

