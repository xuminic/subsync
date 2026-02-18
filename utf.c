
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <iconv.h>

#include "utf.h"

#define StrNCmp(a,b)	strncmp((a),(b),strlen(b))
#define StrNCpy(a,b,c)	strncpy((a),(b),(c)-1)

static	MMTAB	bom_codepage[] = {
	{ "\xEF\xBB\xBF",	3,	"UTF-8" },
	{ "\xFE\xFF",		2,	"UTF-16BE" },
	{ "\xFF\xFE",		2,	"UTF-16LE" },
	{ "\x00\x00\xFE\xFF",	4,	"UTF-32BE" },
	{ "\xFF\xFE\x00\x00",	4,	"UTF-32LE" },
	{ NULL, 0, NULL }
};

typedef int (*getchar_t)(FILE *);
static getchar_t	bom_getc = fgetc;

static char *utf_decoding(UTFB *utf, char *buf, int len);
static int utf_equal(UTFB *utf, char *s, int ch);
static int utf_bom_detect(UTFB *utf, FILE *fp);
static int utf_user_defined(UTFB *utf, char *s);
static int bom_endian(char *s, int *width);
static int magic_length(MMTAB *mtab);
static int magic_match(MMTAB *mtab, char *s, int len);
static MMTAB *magic_search(MMTAB *mtab, char *s, int len);
static int idname(char *s);

#ifdef UTF_MAIN
static void hexdump(char *prompt, char *s, int len)
{
	printf("%s", prompt ? prompt : "");
	while (len--) printf("%02x ", (unsigned char) *s++);
	puts("");
}
#endif


UTFB *utf_open(FILE *fp, char *decode, char *encode)
{
	UTFB	*utf;

	if ((utf = malloc(sizeof(UTFB))) == NULL) {
		return NULL;
	}
	memset(utf, 0, sizeof(UTFB));
	utf->cd_dec = utf->cd_enc = (iconv_t) -1;

	if (!decode || !*decode) {
		utf_bom_detect(utf, fp);
	} else if (utf_user_defined(utf, decode) < 0) {
		utf_bom_detect(utf, fp);
	}

	/* worst case scenario: 
	 * User defined the utf-16 or utf-32, so utf_user_defined() return -2
	 * to enforce the utf_bom_detect(). But what if there's no BOM in the input?
	 * In that case, we put the default to "LE" to match the Windows behaviour */
	if (utf->endian == UTF_TRY_ENDIAN) {
		strcat(utf->na_dec, "le");
	}

		
	/* if the input source is not UTF-8, we need the iconv to decode
	 * the source stream to UTF-8 */
	if (utf->na_dec[0]) {
		utf->cd_dec = iconv_open("UTF-8", utf->na_dec);
		if (utf->cd_dec == (iconv_t) -1) {
			utf_close(utf);
			fprintf(stderr, "utf_open: decoding %s\n", utf->na_dec);
			return NULL;
		}
	}


	/* by default, the subsync using utf-8 or original to process subtitles,
	 * and the output file would be utf-8 or original. However, the encode
	 * still can be specified as utf-8, which means to add a explicit BOM
	 * in the beginning of the file */
	/* Note: normally utf-8 doesn't need BOM in case of breaking concatenated
	 * files. However, Window notepad need a clear BOM to open it */
	if (encode && *encode) {
		int	eid = idname(encode);

		StrNCpy(utf->na_enc, encode, sizeof(utf->na_enc));
		/* if the output target is utf-16 or utf-32 without specifying
		 * its endianness, we will set it to "LE" for Windows sake */
		if ((eid == idname("utf16")) || (eid == idname("utf32")) ||
				(eid == idname("ucs2")) || (eid == idname("ucs4"))) {
			strcat(utf->na_enc, "le");
		}

		/* if the output target is not UTF-8, we need the iconv to
		 * encode the target codepage. 
		 * Reference to libiconv-1.18/lib/encodings.def */
		if (strcmp(encode, "CP65001") && (eid != idname("utf8"))) {
			utf->cd_enc = iconv_open(utf->na_enc, "UTF-8");
			if (utf->cd_enc == (iconv_t) -1) {
				utf_close(utf);
				fprintf(stderr, "utf_open: encoding %s\n", encode);
				return NULL;
			}
		}
	}
	return utf;
}

void utf_close(UTFB *utf)
{
	if (utf->cd_dec != (iconv_t) -1) {
		iconv_close(utf->cd_dec);
	}
	if (utf->cd_enc != (iconv_t) -1) {
		iconv_close(utf->cd_enc);
	}
	free(utf);
}

/* It normally doesn't produce BOM for utf-8 content. However, if utf-8 
 * is explicitly specified by "utf->na_enc", it would output the BOM.
 * For example, Windows notepad may need this */
int utf_write_bom(UTFB *utf, FILE *fp)
{
	int	i;

	if (!utf->na_enc[0]) {
		return -1;
	}

	for (i = 0; bom_codepage[i].magic; i++) {
		if (idname(bom_codepage[i].magic_name) == idname(utf->na_enc)) {
			fwrite(bom_codepage[i].magic, 1, bom_codepage[i].magic_len, fp);
			return 0;
		}
	}
	return -2;
}

int utf_puts(UTFB *utf, FILE *fp, char *buf)
{
	size_t	n, rc, inlft, outlft;
	char	*inbuf, *outbuf, lbuf[64];

	if (utf->cd_enc == (iconv_t) -1) {
		return fputs(buf, fp);
	}

	inbuf = buf;
	inlft = strlen(buf);
	while (inlft > 0) {
		outlft = sizeof(lbuf);
		outbuf = lbuf;
		rc = iconv(utf->cd_enc, &inbuf, &inlft, &outbuf, &outlft);
		if ((n = sizeof(lbuf) - outlft) > 0) {
			fwrite(lbuf, 1, n, fp);
		}
		if (rc == (size_t) -1) {
			if (errno == EILSEQ) {	/* illegal character */
				inbuf++, inlft--;	/* skip one char and try again */
			} else if ((errno != EINVAL) && (errno != E2BIG)) {
				break;	/* fatal error */
			}
		}
	}
	return (int)(strlen(buf) - inlft);
}

char *utf_gets(UTFB *utf, FILE *fp, char *buf, int len)
{
	int	i;

	if (!utf->na_dec[0]) {	/* default / utf-8 */
		if (utf->idx == 0) {	/* no buffered BOM reading */
			return fgets(buf, len, fp);
		}

		if (len - 1 < utf->idx) {
			i = len - 1;
			memcpy(buf, utf->buffer, i);
			buf[i] = 0;

			utf->idx -= i;
			memmove(utf->buffer, utf->buffer + i, utf->idx);
			return buf;
		}

		i = utf->idx;
		utf->idx = 0;
		memcpy(buf, utf->buffer, i);
		
		/* 0xa would stop BOM searching anyway so it must be
		 * the last item in the buffer */
		if (utf->buffer[i - 1] == 0xa) {
			buf[i] = 0;
			return buf;
		}
		return fgets(buf + i, len - i, fp);
	}

	if (utf->width == 1) {
		char	*tbuf = utf->buffer;
		if (utf->idx == 0) {
			tbuf = fgets(utf->buffer, sizeof(utf->buffer), fp);
		} else if (utf->buffer[utf->idx - 1] != 0xa) {
			tbuf = fgets(utf->buffer + utf->idx, sizeof(utf->buffer) - utf->idx, fp);
		}
		if (tbuf == NULL) {
			return NULL;
		}

		utf->idx = strlen(utf->buffer);
		return utf_decoding(utf, buf, len);
	}

	/* make sure the readings happening on the "utf->width" boundary */
	if (utf->idx) {		/* fix the broken word */
		if ((i = utf->width - utf->idx % utf->width) < utf->width) {
			fread(&utf->buffer[utf->idx], 1, i, fp);
			utf->idx += i;
		}
		i = utf->idx - utf->width;
		if (utf_equal(utf, &utf->buffer[i], 0xa)) {
			return utf_decoding(utf, buf, len);
		}
	}

	for ( ; utf->idx < MIN(len, sizeof(utf->buffer)); utf->idx += utf->width) {
		if (fread(&utf->buffer[utf->idx], 1, utf->width, fp) < 1) {
			break;
		}
		if (utf_equal(utf, &utf->buffer[utf->idx], 0xa)) {
			utf->idx += utf->width;
			break;
		}
	}
	return utf_decoding(utf, buf, len);
}

static char *utf_decoding(UTFB *utf, char *buf, int len)
{
	size_t	in_bytes_left, out_bytes_left;
	char	*in_buf, *out_buf;

	out_buf = buf;
	out_bytes_left = len - 1;

	in_buf = utf->buffer;
	in_bytes_left = utf->idx;
	utf->idx = 0;
	hexdump("utf_decoding: ", utf->buffer, in_bytes_left);

	if (iconv(utf->cd_dec, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left) < 0) {
		perror("utf_decoding");
		return NULL;
	}
	*out_buf = 0;
	hexdump("utf_decoding: ", buf, len - out_bytes_left);
	return buf;
}

static int utf_equal(UTFB *utf, char *s, int ch)
{
	char	rune[4];

	if ((utf->na_dec[0] == 0) || (utf->width == 1)) {
		return (*s == (char) ch);
	}

	memset(rune, 0, sizeof(rune));
	if (utf->width == 2) {
		if (utf->endian == UTF_LIT_ENDIAN) {	/* LE */
			rune[0] = (char) ch;	/* "\xa\0" */
		} else {
			rune[1] = (char) ch;	/* "\0\xa" */
		}
		//printf("utf_equal: %u %u %u %u\n", s[0], s[1], rune[0], rune[1]);
		return (!memcmp(s, rune, 2));
	}
	if (utf->endian == UTF_LIT_ENDIAN) {	/* LE */
		rune[0] = (char) ch;	/* "\xd\0\0\0" */
	} else {
		rune[3] = (char) ch;	/* "\0\0\0\xd" */
	}
	return (!memcmp(s, rune, 4));
}

static int utf_bom_detect(UTFB *utf, FILE *fp)
{
	MMTAB	*mtab;

	if (fp == NULL) {
		return -1;	/* ignore detection: UTF-8 */
	}
	for (utf->idx = 0; utf->idx < magic_length(bom_codepage); ) {
		utf->buffer[utf->idx++] = (char) bom_getc(fp);
		if (magic_match(bom_codepage, utf->buffer, utf->idx) < 0) {
			break;	/* the last reading does not match */
		}
	}
		
	/* Note that the utf->buffer have the number of 'utf->idx' 
	 * bytes avaible for the next reading */
	mtab = magic_search(bom_codepage, utf->buffer, utf->idx);
	if (mtab == NULL) {
		return -1;	/* BOM not found */
	}
	
	/* deduct the width and endianness by the magic name */
	/* no need to offset the BOM because iconv will take it off */
	utf->endian = bom_endian(mtab->magic_name, &utf->width);

	/* We don't change utf->na_dec if it's already set.
	 * We also ignore UTF-8 because it's default setting. */
	if (!utf->na_dec[0] && strcmp(mtab->magic_name, "UTF-8")) {
		StrNCpy(utf->na_dec, mtab->magic_name, sizeof(utf->na_dec));
		/* since the BOM is included in the input stream,
		 * we must NOT specify the endianness in the iconv name,
		 * otherwise iconv will produce double BOM output */
		utf->na_dec[strlen(utf->na_dec) - 2] = 0;
	}
	return 0;
}
	
static int utf_user_defined(UTFB *utf, char *s)
{
	iconv_t	tmp_iconv;

	/* validate the IANA name */
	utf->width = 1;
	if ((tmp_iconv = iconv_open("UTF-8", s)) == (iconv_t) -1) {
		return -1;
	}
	iconv_close(tmp_iconv);

	StrNCpy(utf->na_dec, s, sizeof(utf->na_dec));
	if (idname(s) < 0) {
		return 0;
	}
	if (idname(s) == idname("utf7")) {
		return 0;
	}
	if (idname(s) == idname("utf8")) {
		utf->na_dec[0] = 0;	/* We ignore UTF-8 setting */
		return 0;
	}

	utf->endian = bom_endian(s, &utf->width);
	if (utf->endian == UTF_TRY_ENDIAN) {
		/* When we know it's 2- or 4- bytes coding but we don't know
		 * the endianness, it requires an autodetection */
		return -2;
	}
	return 0;
}


/* Only the 2-byte or 4-byte schemes need to be converted.
 * UTF-16, UCS-2, UTF-32, UCS-4
 * Refer to libiconv-1.18/lib/encodings.def */
static int bom_endian(char *s, int *width)
{
	int	tmp;

	if (width == NULL) {
		width = &tmp;
	}
	*width = 1;
	if (!StrNCmp(s, "utf") || !StrNCmp(s, "UTF")) {
		s += (s[3] == '-') ? 4 : 3;
		*width = (*s == '1') ? 2 : ((*s == '3') ? 4 : 1);
		s += 2;
	} else if (!StrNCmp(s, "ucs") || !StrNCmp(s, "UCS")) {
		s += (s[3] == '-') ? 4 : 3;
		*width = (*s == '2') ? 2 : ((*s == '4') ? 4 : 1);
		s += 1;
	} else {
		return UTF_BIG_ENDIAN;
	}

	if (!strcmp(s, "be") || !strcmp(s, "BE")) {
		return UTF_BIG_ENDIAN;
	}
	if (!strcmp(s, "le") || !strcmp(s, "LE")) {
		return UTF_LIT_ENDIAN;
	}
	return UTF_TRY_ENDIAN;
}

static int magic_length(MMTAB *mtab)
{
	int	i, n;

	for (i = n = 0; mtab[i].magic_name; i++) {
		n = MAX(mtab[i].magic_len, n);
	}
	return n;
}

static int magic_match(MMTAB *mtab, char *s, int len)
{
	int	i, v = -1;

	for (i = 0; mtab[i].magic_name; i++) {
		if ((len <= mtab[i].magic_len) && !memcmp(mtab[i].magic, s, len)) {
			v = i;
		}
	}
	return v;	/* return the last matching */
}

static MMTAB *magic_search(MMTAB *mtab, char *s, int len)
{
	int	i, lidx = -1, llen = 0;

	for (i = 0; mtab[i].magic_name; i++) {
		if (!memcmp(mtab[i].magic, s, mtab[i].magic_len)) {
			if (mtab[i].magic_len > llen) {
				llen = mtab[i].magic_len;
				lidx = i;
			}
		}
	}
	return lidx < 0 ? NULL : &mtab[lidx];
}

/* convert UTF/UCS name to ID number
 * bit 0-3: width 0=under 8-bit 1=8-bit 2=16-bit 3=32-bit
 * bit 4-7: endianness 0=autodetect 1=LE 2=BE
 * bit 8+:  name 0=UTF 1=UCS 2=MORE
 */
static int idname(char *s)
{
	int	id = 0;

	if (!StrNCmp(s, "ucs") || !StrNCmp(s, "UCS")) {
		id |= 0x100;
		s += (s[3] == '-') ? 4 : 3;
	} else if (!StrNCmp(s, "utf") || !StrNCmp(s, "UTF")) {
		s += (s[3] == '-') ? 4 : 3;
	} else {
		return -1;	/* unsupported name */
	}
	if (!StrNCmp(s, "8")) {
		id |= 1; s++;		/* utf-8 */
	} else if (!StrNCmp(s, "2")) {
		id |= 2, s++;		/* ucs-2 */
	} else if (!StrNCmp(s, "16")) {
		id |= 2, s += 2;	/* utf-16 */
	} else if (!StrNCmp(s, "4")) {
		id |= 3, s++;		/* ucs-4 */
	} else if (!StrNCmp(s, "32")) {
		id |= 3, s += 2;	/* utf-32 */
	}
	while (isdigit(*s)) s++;	/* skip other numbers, like 7 */

	if (!StrNCmp(s, "le") || !StrNCmp(s, "LE")) {
		id |= 0x10;
	} else if (!StrNCmp(s, "be") || !StrNCmp(s, "BE")) {
		id |= 0x20;
	}
	return id;
}

#ifdef	UTF_MAIN

#define MOREARG(c,v)    {       \
        --(c), ++(v); \
        if (((c) == 0) || (**(v) == '-') || (**(v) == '+')) { \
                fprintf(stderr, "missing parameters\n"); \
                return -1; \
        }\
}

static void dump_utfb(UTFB *utf) 
{
	printf("iconv decoder:          %p\n", utf->cd_dec);
	printf("iconv decode name:      %s\n", utf->na_dec[0] ? utf->na_dec : "unknown");
	printf("iconv decode width:     %d\n", utf->width);
	printf("iconv decode endian:    %d\n", utf->endian);
	printf("iconv encoder:          %p\n", utf->cd_enc);
	printf("iconv encode name:      %s\n", utf->na_enc[0] ? utf->na_enc : "unknown");
	printf("buffer overflow:        %d (0x%x)\n", utf->idx, utf->buffer[0]);
}


static int mock_getc(FILE *stream)
{
	static	unsigned char	*p;
	int	ch = EOF;

	if (stream) {
		p = (unsigned char *) stream;
	} else {
		ch = (int) *p++;
	}
	return ch;
}


static int test_utf_bom_detect(void)
{
	char	*magics[] = {
		"\xEF\xBB\xBF ABC",		/* UTF-8 */
		"\xFE\xFF ABC",			/* UTF-16BE */
		"\xFF\xFE ABC",			/* UTF-16LE */
		"\x00\x00\xFE\xFF ABC",		/* UTF-32BE */
		"\xFF\xFE\x00\x00 ABC",		/* UTF-32LE */
		"\xFF\xFE\x00 ABC",		/* trick to UTF-16LE */
		"\x2B\x2F\x76 ABC",		/* UTF-7 */
		"\xF7\x64\x4C ABC",		/* UTF-1 */
		"\xDD\x73\x66\x73 ABC",		/* UTF-EBCDIC */
		"\x84\x31\x95\x33 ABC",		/* GB18030 */
		NULL
	};
	UTFB	*utf;
	char	display[64];
	int	i, k;

	bom_getc = mock_getc;
	utf = utf_open(stdin, "UTF-8", NULL);	
	for (i = 0; magics[i]; i++) {
		mock_getc((FILE *) magics[i]);
		utf->na_dec[0] = 0;
		utf_bom_detect(utf, NULL);
		memset(display, ' ', sizeof(display));
		for (k = 0; k < utf->idx; k++) {
			sprintf(display + (k * 3), "%02X", (unsigned char) utf->buffer[k]);
			display[k * 3 + 2] = ' ';
		}
		display[24] = 0;
		printf("%s %-12s %d %d\n", display, 
				utf->na_dec[0] ? utf->na_dec : "unknown",
				utf->width, utf->endian);
	}
	utf_close(utf);
	return 0;
}

static int test_utf_write_bom(char *encode)
{
	UTFB	*utf;

	utf = utf_open(stdin, "UTF-8", encode);
	utf_write_bom(utf, stdout);
	utf_close(utf);
	return 0;
}

static int test_utf_gets(char *decode)
{
	UTFB	*utf;
	char	buf[64];

	utf = utf_open(stdin, decode, NULL);
	dump_utfb(utf);
	utf_gets(utf, stdin, buf, sizeof(buf));
	printf("%ld: %s\n", (long)strlen(buf), buf);
	utf_close(utf);
	return 0;
}


#define DBOM_NAME	"UTF-16BE"
#define DBOM_SOUR	"\xfe\xff\xd8\x08\xdf\x45\x00\x3d"
#define DBOM_LEN	2
/*
#define DBOM_NAME	"UTF-8"
#define DBOM_SOUR	"\xef\xbb\xbf"
#define DBOM_LEN	3
*/

static int dump_bom(FILE *fp)
{
	iconv_t	cd;
	char	bom[64], buf[256], *p;
	
	size_t	rc, in_bytes_left, out_bytes_left;
	char	*in_buf, *out_buf, *utfsrc = DBOM_SOUR;

	while (fgets(buf, sizeof(buf), fp)) {
		/* chop off the newline and '/', which come from "iconv -l" */
		for (p = buf + strlen(buf) - 1; !isalnum(*p); *p-- = 0);

		if ((cd = iconv_open(buf, DBOM_NAME)) == (iconv_t) -1) {
			continue;
		}

		in_buf = utfsrc;
		in_bytes_left = DBOM_LEN;
		out_buf = bom;
		out_bytes_left = sizeof(bom);

		rc = iconv(cd, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left);
		if (rc != (size_t) -1) {
			strcat(buf, ": ");
			hexdump(buf, bom, sizeof(bom) - out_bytes_left);	
		}
		iconv_close(cd);
	}
	return 0;
}

static int test_iconv_stream(char *decode, FILE *fin, FILE *fout)
{
	UTFB	*utf;
	size_t	n, rc, prod, inbytes, outbytes;
	char	*inbuf, *outbuf, lbuf[64];

	utf = utf_open(fin, decode, NULL);
	dump_utfb(utf);

	inbytes = utf->idx;
	while ((n = fread(utf->buffer + inbytes, 1, sizeof(utf->buffer) - inbytes, fin)) > 0) {
		inbytes += n;
		inbuf = utf->buffer;
		while (inbytes > 0) {
			outbuf = lbuf;
			outbytes = sizeof(lbuf);
			rc = iconv(utf->cd_dec, &inbuf, &inbytes, &outbuf, &outbytes);
			if ((prod = (sizeof(lbuf) - outbytes)) > 0) {
				fwrite(lbuf, 1, prod, fout);
			}
			if (rc != (size_t) -1) {
				break;
			}

			perror(utf->na_dec);
			switch (errno) {
			case EINVAL:	/* incomplete codepoint; need more fread */
				goto got_EINVAL;
			case E2BIG:	/* output buffer full; need to write out */
				break;	/* it's already been flushed */
			case EILSEQ:	/* illegal char; skip one and try again */
				/* output the replacement unit U+FFFD */
				fwrite("\xEF\xBF\xBD", 1, 3, fout);
				inbuf++;
				inbytes--;
				break;
			default:
				goto got_FATAL;
			}
		}
got_EINVAL:
		/* relocate the unused chars to the head of the buffer */
		memmove(utf->buffer, inbuf, inbytes);
	}
got_FATAL:
	outbuf = lbuf;
	outbytes = sizeof(lbuf);
	iconv(utf->cd_dec, NULL, NULL, &outbuf, &outbytes);
	if ((prod = (sizeof(lbuf) - outbytes)) > 0) {
		fwrite(lbuf, 1, prod, fout);
	}
	if (inbytes > 0) {
		fwrite("\xEF\xBF\xBD", 1, 3, fout);
	}
	utf_close(utf);
	return 0;
}


static int test_utf_iconv(char *decode, char *encode, FILE *fin, FILE *fout)
{
	UTFB	*utf;
	char	buf[256];

	utf = utf_open(fin, decode, encode);
	utf_write_bom(utf, fout);
	while (utf_gets(utf, fin, buf, sizeof(buf))) {
		utf_puts(utf, fout, buf);
	}
	utf_close(utf);
	return 0;
}


int main(int argc, char **argv)
{
	UTFB	*utf;
	char	*dec, *enc;

	dec = enc = NULL;
	while (--argc && ((**++argv == '-') || (**argv == '+'))) {
		if (!strcmp(*argv, "-H") || !strcmp(*argv, "--help")) {
			puts("help");
			return 0;
		} else if (!strcmp(*argv, "--bom-detect")) {
			return test_utf_bom_detect();
		} else if (!strcmp(*argv, "--bom-print")) {
			/* utf --bom-print utf-16be | hexdump -C */
			MOREARG(argc, argv);
			return test_utf_write_bom(*argv);
		} else if (!strcmp(*argv, "--dump-bom")) {
			/* dump all known BOM: "iconv -l | utf --dump-bom" 
			 * Note that UTF-16 and UTF-32 would convert the source BOM
			 * and top up with the target BOM so it will produce
			 *     UTF-16: ff fe ff fe
			 *     UTF-32: ff fe 00 00 ff fe 00 00
			 */
			return dump_bom(stdin);
		} else if (!strcmp(*argv, "-d") || !strcmp(*argv, "--decode")) {
			MOREARG(argc, argv);
			dec = *argv;
		} else if (!strcmp(*argv, "-e") || !strcmp(*argv, "--encode")) {
			MOREARG(argc, argv);
			enc = *argv;
		} else if (!strcmp(*argv, "--id")) {
			MOREARG(argc, argv);
			printf("0x%x\n", idname(*argv));
			return 0;
		} else if (!strcmp(*argv, "-g") || !strcmp(*argv, "--gets")) {
			return test_utf_gets(dec);
		} else if (!strcmp(*argv, "-i") || !strcmp(*argv, "--iconv")) {
			return test_iconv_stream(dec, stdin, stdout);
		} else if (!strcmp(*argv, "-u") || !strcmp(*argv, "--utf")) {
			return test_utf_iconv(dec, enc, stdin, stdout);
		} else {
			fprintf(stderr, "%s: unknown parameter.\n", *argv);
			return -1;
                }
	}
	
	utf = utf_open(stdin, dec, enc);
	dump_utfb(utf);
	utf_close(utf);
	return 0;
}
#endif



