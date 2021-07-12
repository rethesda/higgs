#ifndef	FPVR_VERSION_INCLUDED
#define FPVR_VERSION_INCLUDED

#define MAKE_STR_HELPER(a_str) #a_str
#define MAKE_STR(a_str) MAKE_STR_HELPER(a_str)

#define FPVR_VERSION_MAJOR	1
#define FPVR_VERSION_MINOR	2
#define FPVR_VERSION_PATCH	2
#define FPVR_VERSION_BETA	1
#define FPVR_VERSION_VERSTRING	MAKE_STR(FPVR_VERSION_MAJOR) "." MAKE_STR(FPVR_VERSION_MINOR) "." MAKE_STR(FPVR_VERSION_PATCH) "." MAKE_STR(FPVR_VERSION_BETA)

#endif
