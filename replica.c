#include "common-header.h"
#include "node.h"
#include "config.h"
#include "replica.h"

int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static connection* peers_pool[MAX_ACCEPT_CONNECTIONS];
static unsigned current_connection = 3;
struct timeval reconnect_timeval = {2,0};
struct timeval ping_timeval = {10,0};
int heart_beat_threshold = 4;

void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -s [rb] Start Option bootstrap or recovery\n");
    paxos_log("        -c path path to configuration file\n");
}


void connection_free(connection* con){
    if(NULL!=con){
        if(NULL!=con->my_buff_event){
            bufferevent_disable(con->my_buff_event,EV_READ|EV_WRITE);
            bufferevent_free(con->my_buff_event);
        }
        if(NULL!=con->peer_address){
            free(con->peer_address);
        }
        close(con->sock_id);
        free(con);
    }
}

static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};

static void peer_node_on_event(struct bufferevent* bev,short what,void* arg){
    peer* peer_node = (peer*)arg;
    if(what&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if(what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR){
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
        bufferevent_free(bev);
        peer_node->active = 0;
        event_add(peer_node->reconnect,&reconnect_timeval);
    }
};


static void connect_peer(peer* peer_node){
    connection* peer_connection = peer_node->peer_connection;
    peer_connection->my_buff_event = bufferevent_socket_new(peer_node->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(peer_connection->my_buff_event,peer_node_on_read,NULL,peer_node_on_event,peer_node);
    bufferevent_enable(peer_connection->my_buff_event,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(peer_connection->my_buff_event,(struct sockaddr*)peer_connection->peer_address,peer_connection->sock_len);
};

static void peer_node_on_timeout(int fd,short what,void* arg){
    connect_peer((peer*)arg);
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

int free_node(node* my_node){
    //to-do
    return 0;
}


void replica_on_read(struct bufferevent* bev,void* arg);
void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg);
void replica_on_error_cb(struct bufferevent* bev,short error,void* arg);

void replica_on_error_cb(struct bufferevent* bev,short error,void *arg){
    if(error&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    return;
}

void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    struct event_base* my_base = arg;
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        paxos_log("it has exceeded the predefined maximal concurrent connections\n");
        close(fd);
    }else{
        connection* new_connection = (connection*)malloc(sizeof(connection));
        new_connection->sock_id = fd;
        new_connection->sock_len= socklen;
        new_connection->peer_address =(struct sockaddr_in*) address;
        new_connection->base = my_base;
        struct bufferevent* new_buff_event = bufferevent_socket_new(my_base,new_connection->sock_id,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(new_buff_event,replica_on_read,NULL,NULL,(void*)new_connection);
        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
        new_connection->my_buff_event = new_buff_event;
        peers_pool[current_connection++] = new_connection;
    }
};


void replica_on_read(struct bufferevent* bev,void* arg){
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



node* system_initialize(int argc,char** argv,l_cb user_call_back){
    char* start_option = NULL;
    char* config_path = NULL;
    int node_id = -1;
    int c;
    while((c = getopt (argc,argv,"c:n:s:")) != -1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 'c':
                config_path = optarg;
                break;
            case 's':
                start_option = optarg;
                if(*start_option!='b' && *start_option!='r'){
                    paxos_log("Unknown Start Option\n");
                    usage();
                    goto exit_error;
                }
                break;
            case '?':
                if(optopt == 'n'){
                    paxos_log("Option -n requires an argument");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 's'){
                    paxos_log("Option -s requires an argument");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'c'){
                    paxos_log("Option -c requires an argument");
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
    //bootstrap, currently the node is the leader
    if(*start_option=='b'){
        my_node->cur_view.view_id = 0;
        my_node->cur_view.leader_id = my_node->node_id;
    }else{
        my_node->cur_view.view_id = -1;
        my_node->cur_view.leader_id = -1;
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
    struct evconnlistener* listener = evconnlistener_new_bind(base,replica_on_accept,(void*)base,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,(struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));

    if(!listener){
        paxos_log("cannot set up the listener\n");
        goto exit_error;
    }

	event_base_dispatch(base);
	event_base_free(base);

	return my_node;
exit_error:
    return NULL;

}

