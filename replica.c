#include "common-header.h"
#include "node.h"
#include "config.h"
#include "replica.h"

int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static unsigned current_connection = 3;
struct timeval reconnect_timeval = {2,0};
struct timeval ping_timeval = {10,0};
int heart_beat_threshold = 4;

void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -m [sr] Start Mode seed|recovery\n");
    paxos_log("        -c path path to configuration file\n");
}


static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};

static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg){
    peer* peer_node = (peer*)arg;
    if(ev&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
        bufferevent_free(bev);
        peer_node->active = 0;
        event_add(peer_node->reconnect,&reconnect_timeval);
    }
};


static void connect_peer(peer* peer_node){
    peer_node->my_buff_event = bufferevent_socket_new(peer_node->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(peer_node->my_buff_event,peer_node_on_read,NULL,peer_node_on_event,peer_node);
    bufferevent_enable(peer_node->my_buff_event,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(peer_node->my_buff_event,(struct sockaddr*)peer_node->peer_address,peer_node->sock_len);
};

static void peer_node_on_timeout(int fd,short what,void* arg){
    connect_peer((peer*)arg);
};

static void ping_leader_timeout(int fd,short what,void* arg){
    return;
};

void connect_peers(node* my_node){
    for(int i=0;i<my_node->group_size;i++){
        if(i!=my_node->node_id){
            peer* peer_node = my_node->peer_pool+i;
            peer_node->reconnect = evtimer_new(peer_node->base,peer_node_on_timeout,peer_node);
            connect_peer(peer_node);
        }
    }
}

int initialize_node(node* my_node){
    int flag = 0;
    connect_peers(my_node);
    goto initialize_node_exit;
initialize_node_exit:
        return flag;
}




static void free_peer(peer* peer_node){
    if(NULL!=peer_node){
        if(NULL!=peer_node->my_buff_event){
            bufferevent_free(peer_node->my_buff_event);
        }
        if(NULL!=peer_node->reconnect){
            event_free(peer_node->reconnect);
        }
        if(NULL!=peer_node->peer_address){
            free(peer_node->peer_address);
        }
        free(peer_node);
    }
}

static void free_peers(node* my_node){
    for(int i=0;i<my_node->group_size;i++){
        free_peer(my_node->peer_pool+i);
    }
}


static int free_node(node* my_node){
    //to-do
    return 0;
}

static void replica_on_read(struct bufferevent* bev,void* arg);
static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg);
static void replica_on_error_cb(struct bufferevent* bev,short ev,void* arg);

static void replica_on_error_cb(struct bufferevent* bev,short ev,void *arg){
    if(ev&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    return;
}

static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    node* my_node = arg;
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        paxos_log("it has exceeded the predefined maximal concurrent connections\n");
        close(fd);
    }else{
        struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
        current_connection++;
    }
};

static void replica_on_read(struct bufferevent* bev,void* arg){
//	connection* my_con = arg;
    char buf[1024];
    int n;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    paxos_log("there is %u bytes data in the buffer in total\n",(unsigned)len);
    while((n=evbuffer_remove(input,buf,sizeof(buf)))>0){
        printf("%s\n",buf);
    }
}




node* system_initialize(int argc,char** argv,void(*user_cb)(int data_size,void* data)){
    char* start_mode= NULL;
    char* config_path = NULL;
    int node_id = -1;
    int c;

    while((c = getopt (argc,argv,"c:n:m:")) != -1){
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

    node* my_node = (node*)malloc(sizeof(node));
    my_node->base = base;
    my_node->node_id = node_id;

    //seed, currently the node is the leader
    if(*start_mode=='s'){
        my_node->cur_view.view_id = 0;
        my_node->cur_view.leader_id = my_node->node_id;
        my_node->ev_ping_leader = NULL;
    }else{
        my_node->cur_view.view_id = -1;
        my_node->cur_view.leader_id = -1;
        my_node->ev_ping_leader = NULL;
    }

    if(read_configuration_file(my_node,config_path)){
        goto exit_error;
    }
    
#if DEBUG
    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&my_node->my_address.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current node's ip address is %s:%d\n",ipv4_address,ntohs(my_node->my_address.sin_port));
    debug_log("current node's db name is %s\n",my_node->db_name);
#endif


//    if(initialize_node(&my_node)){
//        paxos_log("cannot initialize node\n");
//        goto exit_error;
//    }

    my_node->listener =
        evconnlistener_new_bind(base,replica_on_accept,
                (void*)my_node,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
                (struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));

    if(!my_node->listener){
        paxos_log("cannot set up the listener\n");
        goto exit_error;
    }

	return my_node;

exit_error:
    free_node(my_node);
    return NULL;
}


void system_run(struct node_t* replica){
    event_base_dispatch(replica->base);
}


void system_exit(struct node_t* replica){
    event_base_loopexit(replica->base,NULL);
    event_base_free(replica->base);
    free_node(replica);
}
