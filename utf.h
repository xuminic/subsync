
#ifndef _SUBSYNC_UTF_H_
#define _SUBSYNC_UTF_H_

#include <iconv.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UTF_MAX_BUF	4096
#define APP_MAX_BUF	(UTF_MAX_BUF / 4)

#define UTF_BIG_ENDIAN	0
#define UTF_LIT_ENDIAN	1
#define UTF_TRY_ENDIAN	2

typedef struct  _MMTAB  {
	char    *magic;
	int     magic_len;
	char    *magic_name;
} MMTAB;


typedef	struct		_UTFBUF	{
	iconv_t		cd_dec;
	char		na_dec[64];	/* decode by bom_codepage */

	iconv_t		cd_enc;
	char		na_enc[64];	/* like UTF-16BE for iconv */

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


#ifdef __cplusplus
}
#endif

#endif	/* _SUBSYNC_UTF_H_ */

