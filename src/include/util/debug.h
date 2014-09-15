
#ifndef DEBUG_H
#define DEBUG_H

#if DEBUG
#define debug_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:\t",tv.tv_sec * 1000 + tv.tv_usec / 1000); \
    fprintf(stderr,args); \
}while(0);
#else
#define debug_log(args...) do{;}while(0);
#endif


#define paxos_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:\t",tv.tv_sec * 1000 + tv.tv_usec / 1000); \
    fprintf(stderr,args); \
}while(0);

#define ENTER_FUNC {debug_log("entering %s.\n",__PRETTY_FUNCTION__)}
#define LEAVE_FUNC {debug_log("leaving %s.\n",__PRETTY_FUNCTION__)}

#define DEBUG_POINT(n) {debug_log("debug point " #n ".\n")}

#endif
