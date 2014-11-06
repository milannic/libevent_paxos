
#ifndef DEBUG_H
#define DEBUG_H

#define debug_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);


#define paxos_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define err_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define rec_log(out,args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(out,"%lu:%lu \t",tv.tv_sec,tv.tv_usec); \
    fprintf(out,args); \
}while(0);

#define PROXY_ENTER(x) {if(x->sys_log){rec_log("PROXY : Entering %s.\n",__PRETTY_FUNCTION__)}}
#define PROXY_LEAVE(x) {if(x->sys_log){rec_log("PROXY : Leaving %s.\n",__PRETTY_FUNCTION__)}}
#define PROXY_ERR_LEAVE(x) {if(x->sys_log){rec_log("PROXY : Error Occurred,Before Leaving %s.\n",__PRETTY_FUNCTION__)}}

#define CONSENSUS_ENTER(x) {if(x->sys_log){rec_log("CONSENSUS : Entering %s.\n",__PRETTY_FUNCTION__)}}
#define CONSENSUS_LEAVE(x) {if(x->sys_log){rec_log("CONSENSUS : Leaving %s.\n",__PRETTY_FUNCTION__)}}
#define CONSENSUS_ERR_LEAVE(x) {if(x->sys_log){rec_log("CONSENSUS : Error Occurred,Before Leaving %s.\n",__PRETTY_FUNCTION__)}}

#define DEBUG_POINT(x,n) {if(x->sys_log){rec_log("Debug Point " #n ".\n")}}

#endif
