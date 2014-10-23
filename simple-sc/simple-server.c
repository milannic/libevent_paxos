/*
 * =====================================================================================
 *
 *       Filename:  client.c *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/26/2014 10:19:51 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../src/include/rsm-interface.h"
#include "../src/include/util/common-header.h"
#include <unistd.h>
#include <time.h> /* 

typedef struct request_submit_msg_t{
    sys_msg_header header; 
    char data[0];
}__attribute__((packed))req_sub_msg;
#define REQ_SUB_SIZE(M) (((M)->header->data_size)+sizeof(req_sub_msg))
 */

int main ( int argc, char *argv[] )
{
    char* server_address=NULL;
    int server_port;
    int c;
    int node_id=9;
    int repeat_time=1000;
    int sleep_interval=20;
    int sleep_type = 0;
    while((c=getopt(argc,argv,"s:p:"))!=-1){
        switch(c){
            case 's':
                server_address = optarg;
                break;
            case 'p':
                server_port = atoi(optarg);
                break;
            case '?':
                fprintf(stderr,"Option -%c needs requires an argument\n",optopt);
                break;
            default:
                fprintf(stderr,"Unknown Option %c\n",c);
                goto main_exit_error;
        }
    }

//    fprintf(stderr,"the node id is %d\n",node_id);
//    fprintf(stderr,"the server address is %s\n",server_address);
//    fprintf(stderr,"the server address is %d\n",server_port);

    fd_set sock_set,read_sock_set;
    FD_ZERO(&sock_set);
    struct sockaddr_in server_sock_addr;
    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_port = htons(server_port);
    inet_pton(AF_INET,server_address,&server_sock_addr.sin_addr);

    int my_socket = socket(AF_INET,SOCK_STREAM,0);

    if(bind(my_socket,(struct sockaddr*)&server_sock_addr,sizeof(server_sock_addr))!=0){
        fprintf(stderr,"%s",strerror(errno));
        goto main_exit_error;
    }

    if(listen(my_socket,10)!=0){
        fprintf(stderr,"%s",strerror(errno));
        goto main_exit_error;
    }
    FD_SET(my_socket,&sock_set);
    while(1){
        read_sock_set = sock_set;
        if(select(FD_SETSIZE,&read_sock_set,NULL,NULL,NULL)<0){
            fprintf(stderr,"select error.\n");
        }
        for(int i=0;i<FD_SETSIZE;i++){

        }
        

    }

    return EXIT_SUCCESS;
main_exit_error:
    return EXIT_FAILURE;
}				/* ----------  end of function main  ---------- */
