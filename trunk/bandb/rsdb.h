/* $Id: rsdb.h 22343 2006-04-10 18:56:26Z leeh $ */
#ifndef INCLUDED_rsdb_h
#define INCLUDED_rsdb_h

typedef int (*rsdb_callback) (int, const char **);

typedef enum rsdb_transtype
{
	RSDB_TRANS_START,
	RSDB_TRANS_END
} 
rsdb_transtype;

struct rsdb_table
{
	char ***row;
	int row_count;
	int col_count;
	void *arg;
};

void rsdb_init(void);
void rsdb_shutdown(void);

const char *rsdb_quote(const char *src);

void rsdb_exec(rsdb_callback cb, const char *format, ...);

void rsdb_exec_fetch(struct rsdb_table *data, const char *format, ...);
void rsdb_exec_fetch_end(struct rsdb_table *data);

void rsdb_transaction(rsdb_transtype type);

/* rsdb_snprintf.c */

int rs_vsnprintf(char *dest, const size_t bytes, const char *format, va_list args);
int rs_snprintf(char *dest, const size_t bytes, const char *format, ...);

#endif