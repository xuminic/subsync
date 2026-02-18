
#ifndef _SUBSYNC_UTF_H_
#define _SUBSYNC_UTF_H_

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
	iconv_t		ico_decoder;
	char		decode[64];	/* decode by bom_codepage */
	int		width;		/* 1/2/4 */
	int		endian;		

	iconv_t		ico_encoder;
	char		*encode;	/* like UTF-16BE for iconv */

	char		buffer[UTF_MAX_BUF];
	int		idx;
} UTFB;

UTFB *utf_open(FILE *fp, char *decode, char *encode);
void utf_close(UTFB *utf);
char *utf_gets(UTFB *utf, FILE *fp, char *buf, int len);
int utf_puts(UTFB *utf, FILE *fp, char *buf);
int utf_write_bom(UTFB *utf, FILE *fp);


#ifdef __cplusplus
}
#endif

#endif	/* _SUBSYNC_UTF_H_ */

