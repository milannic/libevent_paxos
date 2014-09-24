/*
 * =====================================================================================
 *
 *       Filename:  proxy.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/14/2014 11:19:25 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "../include/proxy/proxy.h"
#include "../include/config-comp/config-proxy.h"




static void* shared_mem=NULL; 
static rec_no_t cur_id=0;
static rec_no_t highest_id=0;
static pthread_t con_subt=9999999;
static struct timeval reconnect_timeval = {2,0};

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    char is_closed;
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))


//helper function
static hk_t gen_key(nid_t,nc_t,sec_t);

// consensus callback
static void update_state(int,void*);
static void usage();

static void consensus_on_event(struct bufferevent* bev,short ev,void* arg);
static void consensus_on_read(struct bufferevent*,void*);
static void connect_consensus(proxy_node*);
static void reconnect_on_timeout(int fd,short what,void* arg);


//implementation

static hk_t gen_key(nid_t node_id,nc_t node_count,sec_t time){
    hk_t key = time;
    key |= ((hk_t)node_id<<52);
    key |= ((hk_t)node_count<<36);
    return key;
}

//static void update_state(int data_size,void* data){
//    ENTER_FUNC
//    paxos_log("the newly delivered request is %lu.\n",*(hk_t*)data);
//    void* dest = ((flag_t*)shared_mem)+1; 
//    dest = ((rec_no_t*)dest)+1; 
//    memcpy(dest,data,data_size);
//    LEAVE_FUNC
//    return;
//}

//fake update state, we take out the data directly without re-
//visit the database
static void fake_update_state(int data_size,void* data){
    ENTER_FUNC
    proxy_msg_header* header = data;
    switch(header->action){
        case CONNECT:
        case SEND:
        case CLOSE:
            break;
    }
    LEAVE_FUNC
    return;
}


static void* t_consensus(void *arg){
    ENTER_FUNC
    struct node_t* my_node = arg;
    system_run(my_node);
    system_exit(my_node);
    LEAVE_FUNC
    return NULL;
}

//public interface
//

static void proxy_singnal_handler(int sig){
    ENTER_FUNC
    switch(sig){
        case SIGTERM:
            paxos_log("the sig value is %d .\n",sig);
            paxos_log("the sub value is %lu .\n",con_subt);
            if(con_subt!=9999999){
                pthread_kill(con_subt,SIGQUIT);
                paxos_log("wating consensus comp to quit.\n");
                pthread_join(con_subt,NULL);
            }
            LEAVE_FUNC
            exit(sig);
    }
}

proxy_node* proxy_init(int argc,char** argv){
    ENTER_FUNC
    signal(SIGINT,proxy_singnal_handler);
    signal(SIGQUIT,proxy_singnal_handler);
    signal(SIGTERM,proxy_singnal_handler);
    signal(SIGHUP,proxy_singnal_handler);
    char* start_mode= NULL;
    char* config_path = NULL;
    int node_id = -1;
    int fake_mode = 1;
    int c;

    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        goto exit_error;
    }

    memset(proxy,0,sizeof(proxy_node));

    while((c = getopt (argc,argv,"rc:n:m:")) != -1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 'c':
                config_path = optarg;
                break;
            case 'm':
                start_mode= optarg;
                if(*start_mode!='s' && *start_mode!='r'){
                    paxos_log("Unknown Start Mode\n");
                    usage();
                    goto exit_error;
                }
                break;
            case 'r':
                paxos_log("real mode is opened\n");
                fake_mode = 0;
            case '?':
                if(optopt == 'n'){
                    paxos_log("Option -n requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'm'){
                    paxos_log("Option -m requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'c'){
                    paxos_log("Option -c requires an argument\n");
                    usage();
                    goto exit_error;
                }
                break;
            default:
                paxos_log("Unknown Option %d \n",c);
                goto exit_error;
        }
    }

    if(argc<7 || optind>argc){
        usage();
        goto exit_error;
    }

    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto exit_error;
    }

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    proxy->fake = fake_mode;
    proxy->base = base;
    proxy->node_id = node_id;

    if(proxy_read_config(proxy,config_path)){
        goto exit_error;
    }
    // ensure the value is NULL at first
    proxy->hash_map=NULL;
    
    proxy->con_node = system_initialize(node_id,start_mode,config_path,update_state);

    if(NULL==proxy->con_node){
        paxos_log("cannot initialize node\n");
        goto exit_error;
    }
    pthread_create(&con_subt,NULL,t_consensus,proxy->con_node);
//    proxy->listener =
//        evconnlistener_new_bind(base,NULL,
//                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
//                (struct sockaddr*)&proxy->sys_addr.p_addr,sizeof(proxy->sys_addr.p_addr));
//
//    if(!proxy->listener){
//        paxos_log("cannot set up the listener\n");
//        goto exit_error;
//    }

    LEAVE_FUNC
	return proxy;

exit_error:
    if(NULL!=proxy){
        if(NULL!=proxy->con_node){
            // to do
        }
        free(proxy);
    }
    LEAVE_FUNC
    return NULL;
}

void consensus_on_event(struct bufferevent* bev,short ev,void* arg){
    proxy_node* proxy = arg;
    if(ev&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Consensus.\n");
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s.\n",evutil_socket_error_to_string(err));
        bufferevent_free(bev);
        proxy->con_conn = NULL;
        event_add(proxy->re_con,&reconnect_timeval);
    }

}
//void consensus_on_read(struct bufferevent* bev,void*);
void connect_consensus(proxy_node* proxy){
    proxy->con_conn = bufferevent_socket_new(proxy->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(proxy->con_conn,NULL,NULL,consensus_on_event,proxy);
    bufferevent_enable(proxy->con_conn,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(proxy->con_conn,(struct sockaddr*)&proxy->sys_addr.c_addr,proxy->sys_addr.c_sock_len);
}

void reconnect_on_timeout(int fd,short what,void* arg){
    // this function should also test whether it should re-set 
    // a thread to run the consensus module but currently 
    // ignore this
    connect_consensus(arg);
}

static void proxy_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg){

    proxy_node* proxy = arg;
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(proxy->con_conn==NULL){
        paxos_log("We have lost the connection to consensus component now.\n");
        close(fd);
    }else{
        socket_pair* new_conn = malloc(sizeof(socket_pair));
        struct timeval cur;
        gettimeofday(&cur,NULL);
        new_conn->key = gen_key(proxy->node_id,proxy->pair_count++,
                cur.tv_sec);
        int ret;
        MY_HASH_SET(new_conn,proxy->hash_map);
//        struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
//        bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
//        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
//        current_connection++;
    }
    return;
}


void proxy_run(proxy_node* proxy){
    proxy->re_con = evtimer_new(proxy->base,
            reconnect_on_timeout,proxy);
    connect_consensus(proxy);
    proxy->listener =
        evconnlistener_new_bind(proxy->base,NULL,
                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                -1,(struct sockaddr*)&proxy->sys_addr.p_addr,
                sizeof(proxy->sys_addr.p_sock_len));
    return;
}
void proxy_exit(proxy_node* proxy){
    return;
}


static void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -m [sr] Start Mode seed|recovery\n");
    paxos_log("        -c path path to configuration file\n");
}

