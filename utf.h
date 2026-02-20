
#ifndef _SUBSYNC_UTF_H_
#define _SUBSYNC_UTF_H_

#include <iconv.h>

#define UTF_MAX_BUF	4096
#define APP_MAX_BUF	(UTF_MAX_BUF / 4)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  _MMTAB  {
	char    *magic;
	int     magic_len;
	char    *magic_name;
} MMTAB;

typedef	struct		_UTFBUF	{
	iconv_t		cd_dec;
	char		na_dec[64];	/* decode by bom_codepage */
	int		dec_err;

	iconv_t		cd_enc;
	char		na_enc[64];	/* like UTF-16BE for iconv */
	int		enc_err;

	int		bin_acc;
	int		bin_err;

	char		ibuffer[UTF_MAX_BUF/4];
	char		*inbuf;
	size_t		inidx;

	char		obuffer[UTF_MAX_BUF];
	char		*outbuf;
	size_t		outidx;

	char		cache[UTF_MAX_BUF/4];
	size_t		ccidx;
} UTFB;

#define UTFBUFF(u)	(sizeof((u)->ibuffer) - (u)->inidx)
#define UTFPROD(u)	(sizeof((u)->obuffer) - (u)->outidx)


UTFB *utf_open(FILE *fp, char *decode, char *encode);
void utf_close(UTFB *utf);
int utf_write_bom(UTFB *utf, FILE *fp);
int utf_cache(UTFB *utf, FILE *fp, char *s, size_t len);
int utf_puts(UTFB *utf, FILE *fp, char *buf);
int utf_write(UTFB *utf, FILE *fp, char *buf, size_t len);
char *utf_gets(UTFB *utf, FILE *fp, char *buf, int len);
void hexdump(char *prompt, char *s, int len);

#ifdef __cplusplus
}
#endif

#ifdef DEBUG
  //#define WARNX(...)    fprintf(stderr, "[%s:%d] " __VA_ARGS__, __FILE__, __LINE__)
  #define WARNX(...)      fprintf(stderr, __VA_ARGS__)
#else
  #define WARNX(...)      ((void)0)
#endif

#define MOREARG(c,v)    {       \
        --(c), ++(v); \
        if (((c) == 0) || (**(v) == '-') || (**(v) == '+')) { \
                fprintf(stderr, "missing parameters\n"); \
                return -1; \
        }\
}

#define StrNCmp(a,b)    strncmp((a),(b),strlen(b))
#define StrNCpy(a,b,c)  strncpy((a),(b),(c)-1)

#endif	/* _SUBSYNC_UTF_H_ */

