
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>

#include "utf.h"

static	MMTAB	bom_codepage[] = {
	{ "\xEF\xBB\xBF",	3,	"UTF-8" },
	{ "\xFE\xFF",		2,	"UTF-16BE" },
	{ "\xFF\xFE",		2,	"UTF-16LE" },
	{ "\x00\x00\xFE\xFF",	4,	"UTF-32BE" },
	{ "\xFF\xFE\x00\x00",	4,	"UTF-32LE" },
	{ NULL, 0, NULL }
};

static size_t utf_pump(UTFB *utf, FILE *fp);
static size_t utf_flush(UTFB *utf, char *buf, size_t len);
static int utf_bom_detect(UTFB *utf, FILE *fp);
static int utf_bin_detect(UTFB *utf, char *s, size_t len);
static int magic_length(MMTAB *mtab);
static int magic_match(MMTAB *mtab, char *s, int len);
static MMTAB *magic_search(MMTAB *mtab, char *s, int len);
static int idname(char *s);

#ifdef UTF_MAIN
typedef int (*getchar_t)(FILE *);
static getchar_t	bom_getc = fgetc;
#endif


UTFB *utf_open(FILE *fp, char *decode, char *encode)
{
	UTFB	*utf;

	if ((utf = malloc(sizeof(UTFB))) == NULL) {
		return NULL;
	}
	memset(utf, 0, sizeof(UTFB));
	utf->cd_dec = utf->cd_enc = (iconv_t) -1;
	utf->inbuf  = utf->ibuffer;
	utf->inidx  = 0;
	utf->outbuf = utf->obuffer;
	utf->outidx = sizeof(utf->obuffer);
	utf->ccidx  = 0;

	if (!decode || !*decode) {
		utf_bom_detect(utf, fp);
	} else {
		StrNCpy(utf->na_dec, decode, sizeof(utf->na_dec));
		/* if specified the input coding, by default the output coding
		 * should be the same, unless overriden by 'encode' option */
		StrNCpy(utf->na_enc, decode, sizeof(utf->na_enc));
	}

	/* if the input source is not UTF-8, we need the iconv to decode
	 * the source stream to UTF-8 */
	if (utf->na_dec[0] && (idname(utf->na_dec) != idname("utf8"))) {
		utf->cd_dec = iconv_open("UTF-8", utf->na_dec);
		if (utf->cd_dec == (iconv_t) -1) {
			utf_close(utf);
			fprintf(stderr, "utf_open: decoding %s\n", utf->na_dec);
			return NULL;
		}
		utf->outbuf = utf->obuffer;
		utf->outidx = sizeof(utf->obuffer);
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

int utf_cache(UTFB *utf, FILE *fp, char *s, size_t len)
{
	if (!s || !len) {	/* flush the cache */
		if (utf->ccidx) {
			len = utf->ccidx;
			utf->ccidx = 0;
			return utf_write(utf, fp, utf->cache, len);
		}
		return 0;
	}
	if (utf->ccidx + len >= sizeof(utf->cache)) {
		utf_write(utf, fp, utf->cache, utf->ccidx);
		utf->ccidx = 0;
	}
	if (utf->ccidx + len >= sizeof(utf->cache)) {
		return utf_write(utf, fp, s, len);
	}
	memcpy(utf->cache + utf->ccidx, s, len);
	utf->ccidx += len;
	//utf->cache[utf->ccidx] = 0;
	return utf->ccidx;
}


int utf_puts(UTFB *utf, FILE *fp, char *buf)
{
	return utf_write(utf, fp, buf, strlen(buf));
}

int utf_write(UTFB *utf, FILE *fp, char *buf, size_t len)
{
	size_t	n, rc, inleft, outleft;
	char	*inbuf, *outbuf, lbuf[64];

	if (utf->cd_enc == (iconv_t) -1) {
		return fwrite(buf, 1, len, fp);
	}

	inbuf = buf;
	inleft = len;
	while (inleft > 0) {
		outleft = sizeof(lbuf);
		outbuf = lbuf;
		rc = iconv(utf->cd_enc, &inbuf, &inleft, &outbuf, &outleft);
		if ((n = sizeof(lbuf) - outleft) > 0) {
			fwrite(lbuf, 1, n, fp);
		}
		if (rc == (size_t) -1) {
			if (errno == EILSEQ) {	/* illegal character */
				inleft--;	/* skip one char and try again */
				inbuf++;
				utf->enc_err++;
			} else if ((errno != EINVAL) && (errno != E2BIG)) {
				break;	/* fatal error */
			}
		}
	}
	return (int)(len - inleft);
}

char *utf_gets(UTFB *utf, FILE *fp, char *buf, int len)
{
	char	*obuf = buf;
	size_t	n = 0, curr, rc;

	if (utf->cd_dec == (iconv_t) -1) {	/* default or utf-8 */
		if (utf->inidx > 0) {	/* buffered BOM reading */
			/* transfer the buffered BOM reading to the output
			 * buffer so utf_flush() can flush them */
			memcpy(utf->obuffer, utf->ibuffer, utf->inidx);
			utf->outidx = sizeof(utf->obuffer) - utf->inidx;
			utf->inidx = 0;
			
			n = utf_flush(utf, buf, len);
			if (n && (*(buf-1) == 0xa)) {
				return buf;
			}
		}
		curr = ftell(fp);
		obuf = fgets(buf + n, len - n, fp);
		curr = ftell(fp) - curr;
		if ((rc = utf_bin_detect(utf, buf, curr)) > 0) {
			//WARNX("utf_gets: binary detected %ld (%ld)\n", rc, curr);
			return NULL;
		}
		return obuf;
	}

	while ((n = utf_flush(utf, buf, len)) >= 0) {
		if (n > 0) {
			buf += n;
			len -= n;
			if ((*(buf-1) == 0xa) || (len < 2)) {
				break;
			}
		}
		if (utf_pump(utf, fp) <= 0) {
			break;
		}
	}
	if (obuf == buf) {
		return NULL;
	}
	return obuf;
}

void hexdump(char *prompt, char *s, int len)
{
	printf("%s", prompt ? prompt : "");
	while (len--) printf("%02x ", (unsigned char) *s++);
	puts("");
}


static size_t utf_pump(UTFB *utf, FILE *fp)
{
	size_t	n, rc;

	n = fread(utf->ibuffer + utf->inidx, 1, UTFBUFF(utf), fp);
	WARNX("utf_pump: input=%ld (+%ld) output=%ld\n", utf->inidx, n, UTFPROD(utf));
	if (n <= 0) {
		return 0;	/* the remains in the iconv buffer cannot decode anyway */
	}

	utf->inidx += n;
	if ((rc = utf_bin_detect(utf, utf->ibuffer, utf->inidx)) > 0) {
		WARNX("utf_pump: binary detected %ld (%ld)\n", rc, n);
		return -1;
	}
	
	utf->inbuf = utf->ibuffer;
	while (utf->inidx > 0) {
		rc = iconv(utf->cd_dec, &utf->inbuf, &utf->inidx, &utf->outbuf, &utf->outidx);
		if (rc != (size_t) -1) {
			break;
		}
		if (errno != EILSEQ) {
			break;	/* ignore EINVAL and E2BIG */
		} else {	/* illegal char */
			//perror("utf_pump");
			utf->dec_err++;
			if (utf->outidx > 3) {
				memcpy(utf->outbuf, "\xEF\xBF\xBD", 3);
				utf->outbuf += 3;
				utf->outidx -= 3;
			}
			utf->inbuf++;
			utf->inidx--;
		}
	}
	if (utf->inidx) {
		/* relocate the unused chars to the head of the buffer */
		memmove(utf->ibuffer, utf->inbuf, utf->inidx);
	}
	WARNX("utf_pump: input=%ld  output=%ld\n", utf->inidx, UTFPROD(utf));
	return UTFPROD(utf);
}

static size_t utf_flush(UTFB *utf, char *buf, size_t len)
{
	size_t	i, prod;

	len--;	/* leave space for EOL */
	prod = UTFPROD(utf);
	for (i = 0; prod && (i < len); ) {
		utf->outidx++;
		utf->outbuf--;
		prod--;
		if ((*buf++ = utf->obuffer[i++]) == 0xa) {
			break;	/* find the line break */
		}
	}
	*buf = 0;
	if (prod) {
		memmove(utf->obuffer, utf->obuffer + i, prod);
	}
	WARNX("utf_flush: %ld (-%ld) transferred\n", i, prod);
	return i;
}

static int utf_bom_detect(UTFB *utf, FILE *fp)
{
	MMTAB	*mtab;
	char	*p;

	if (fp == NULL) {
		return -1;	/* ignore detection: UTF-8 */
	}
	for (utf->inidx = 0; utf->inidx < magic_length(bom_codepage); ) {
#ifdef	UTF_MAIN
		utf->ibuffer[utf->inidx++] = (char) bom_getc(fp);
#else
		utf->ibuffer[utf->inidx++] = (char) fgetc(fp);
#endif
		if (magic_match(bom_codepage, utf->ibuffer, utf->inidx) < 0) {
			break;	/* the last reading does not match */
		}
	}
		
	/* Note that the utf->ibuffer have the number of 'utf->inidx' 
	 * bytes avaible for the next reading */
	mtab = magic_search(bom_codepage, utf->ibuffer, utf->inidx);
	if (mtab == NULL) {
		return -1;	/* BOM not found */
	}
	
	StrNCpy(utf->na_dec, mtab->magic_name, sizeof(utf->na_dec));

	/* assume the input and output the same coding */
	StrNCpy(utf->na_enc, mtab->magic_name, sizeof(utf->na_enc));

	/* since the BOM is included in the input stream,
	 * we must NOT specify the endianness in the iconv name,
	 * otherwise iconv will produce double BOM output */
	p = utf->na_dec + strlen(utf->na_dec) - 2;
	if (!strcmp(p, "le") || !strcmp(p, "LE") || !strcmp(p, "be")
			|| !strcmp(p, "BE")) {
		*p = 0;
	}
	return 0;
}


static int utf_bin_detect(UTFB *utf, char *s, size_t len)
{
	int	i, acc;

	/* only detecting the first 1Kb */
	if (utf) {
		if (utf->bin_acc > 1024) return 0;
		utf->bin_acc += len;
	}

	for (i = acc = 0; len; len--, s++, i++) {
		if (*s == 0) {
			acc++;
		} else {
			acc = 0;
		}
		if (acc > 4) {
			if (utf) utf->bin_err = 1;
			return i;
		}
	}
	return 0;
}

static int magic_length(MMTAB *mtab)
{
	int	i, n;

	for (i = n = 0; mtab[i].magic_name; i++) {
		n = (mtab[i].magic_len > n) ? mtab[i].magic_len : n;
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

static void dump_utfb(UTFB *utf) 
{
	fprintf(stderr, "iconv decoder:          %p\n", utf->cd_dec);
	fprintf(stderr, "iconv decode name:      %s\n", utf->na_dec[0] ? utf->na_dec : "unknown");
	fprintf(stderr, "iconv encoder:          %p\n", utf->cd_enc);
	fprintf(stderr, "iconv encode name:      %s\n", utf->na_enc[0] ? utf->na_enc : "unknown");
	fprintf(stderr, "input buffer:           %ld (%d)\n", utf->inidx, (int)(utf->inbuf - utf->ibuffer));
	fprintf(stderr, "output buffer:          %ld (%d)\n", utf->outidx, (int)(utf->outbuf - utf->obuffer));
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
		for (k = 0; k < utf->inidx; k++) {
			sprintf(display + (k * 3), "%02X", (unsigned char) utf->ibuffer[k]);
			display[k * 3 + 2] = ' ';
		}
		display[24] = 0;
		printf("%s %-12s\n", display, utf->na_dec[0] ? utf->na_dec : "unknown");
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
	size_t	n, rc, prod, outleft;
	char	*outbuf, lbuf[64];

	utf = utf_open(fin, decode, NULL);
	if (utf->cd_dec == (iconv_t) -1) {
		utf->cd_dec = iconv_open("utf-8", "utf-8");
	}
	dump_utfb(utf);

	while ((n = fread(utf->ibuffer + utf->inidx, 1, UTFBUFF(utf), fin)) > 0) {
		utf->inidx += n;
		utf->inbuf = utf->ibuffer;
		while (utf->inidx > 0) {
			outbuf  = lbuf;
			outleft = sizeof(lbuf);
			rc = iconv(utf->cd_dec, &utf->inbuf, &utf->inidx, &outbuf, &outleft);
			if ((prod = (sizeof(lbuf) - outleft)) > 0) {
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
				utf->inbuf++;
				utf->inidx--;
				break;
			default:
				goto got_FATAL;
			}
		}
got_EINVAL:
		/* relocate the unused chars to the head of the buffer */
		memmove(utf->ibuffer, utf->inbuf, utf->inidx);
	}
got_FATAL:
	outbuf  = lbuf;
	outleft = sizeof(lbuf);
	iconv(utf->cd_dec, NULL, NULL, &outbuf, &outleft);
	if ((prod = (sizeof(lbuf) - outleft)) > 0) {
		fwrite(lbuf, 1, prod, fout);
	}
	if (utf->inidx > 0) {
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

static int test_utf_cache(char *decode, char *encode)
{
	UTFB	*utf; 

	utf = utf_open(stdin, decode, encode);
	dump_utfb(utf);
	utf_cache(utf, stdout, "H", 1);
	utf_cache(utf, stdout, "Hello", 3);
	utf_cache(utf, stdout, NULL, 0);
	utf_close(utf);
	return 0;
}

int main(int argc, char **argv)
{
	UTFB	*utf;
	char	*dec, *enc, buf[1024];
	int	n;

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
		} else if (!strcmp(*argv, "--bin-detec")) {
			n = fread(buf, 1, sizeof(buf), stdin);
			printf("%s\n", utf_bin_detect(NULL, buf, n) ? "Binary" : "Text");
			return 0;
		} else if (!strcmp(*argv, "--cache")) {
			test_utf_cache(NULL, NULL);
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



