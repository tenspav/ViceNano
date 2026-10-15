namespace rw {

struct Error
{
	uint32 plugin;
	uint32 code;
};

void setError(Error *e);
Error *getError(Error *e);

#define _ERRORCODE(code, ...) code
char *dbgsprint(uint32 code, ...);

/* ecode is supposed to be in format "(errorcode, printf-arguments..)" */
#ifdef RW_NANO
#define RW_ERROR_PRINT_PREFIX() ((void)0)
#define RW_ERROR_PRINT_MESSAGE(ecode) ((void)0)
#else
#define RW_ERROR_PRINT_PREFIX() fprintf(stderr, "%s:%d: ", __FILE__, __LINE__)
#define RW_ERROR_PRINT_MESSAGE(ecode) fprintf(stderr, "%s\n", rw::dbgsprint ecode)
#endif
#define RWERROR(ecode) do{          \
	rw::Error _e;                   \
	_e.plugin = PLUGIN_ID;      \
	_e.code = _ERRORCODE ecode; \
	RW_ERROR_PRINT_PREFIX(); \
	RW_ERROR_PRINT_MESSAGE(ecode); \
	rw::setError(&_e);              \
}while(0)

}
