
#ifndef DEBUG_H
#define DEBUG_H

#if DEBUG
#define debug_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);
#else
#define debug_log(args...) do{;}while(0);
#endif


#define paxos_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define ENTER_FUNC {debug_log("Entering %s.\n",__PRETTY_FUNCTION__)}
#define LEAVE_FUNC {debug_log("Leaving %s.\n",__PRETTY_FUNCTION__)}
#define ERR_LEAVE_FUNC {debug_log("Error Occured, Before Leaving %s.\n",__PRETTY_FUNCTION__)}

#define DEBUG_POINT(n) {debug_log("Debug Point " #n ".\n")}

#endif
