/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
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

#include "../src/include/replica-sys/message.h"
#include "../src/include/util/common-header.h"

/* 
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
    while((c=getopt(argc,argv,"n:s:p:"))!=-1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
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
    if(argc<7 || optind>argc){
        fprintf(stderr,"Parameter Error\n");
        goto main_exit_error;
    }
    fprintf(stderr,"the node id is %d\n",node_id);
    fprintf(stderr,"the server address is %s\n",server_address);
    fprintf(stderr,"the server address is %d\n",server_port);
    struct sockaddr_in server_sock_addr;
    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_port = htons(server_port);
    inet_pton(AF_INET,server_address,&server_sock_addr.sin_addr);
    int my_socket = socket(AF_INET,SOCK_STREAM,0);
    if(connect(my_socket,(struct sockaddr*)&server_sock_addr,sizeof(server_sock_addr))!=0){
        fprintf(stderr,"%s",strerror(errno));
        goto main_exit_error;
    }

    int count = 0;
    req_sub_msg* request_1 = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+8);
    request_1->header.type = REQUEST_SUBMIT;
    request_1->header.data_size = 8;
    request_1->data[0] = 'n';
    request_1->data[1] = 'o';
    request_1->data[2] = 'd';
    request_1->data[3] = 'e';
    request_1->data[4] = node_id+'0';
    request_1->data[5] = ':';
    request_1->data[6] = count+'0';
    request_1->data[7] = '\0';
    count++;
    printf("send message %s\n",request_1->data);
    int ret;
    
    if((ret=send(my_socket,request_1,REQ_SUB_SIZE(request_1),0))<0){
        goto main_exit_error;
    }
    req_sub_msg* request_2 = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+8);
    request_2->header.type = REQUEST_SUBMIT;
    request_2->header.data_size = 8;
    request_2->data[0] = 'n';
    request_2->data[1] = 'o';
    request_2->data[2] = 'd';
    request_2->data[3] = 'e';
    request_2->data[4] = node_id+'0';
    request_2->data[5] = ':';
    request_2->data[6] = count+'0';
    request_2->data[7] = '\0';
    count++;
    printf("send message %s\n",request_2->data);
    
    if((ret=send(my_socket,request_2,REQ_SUB_SIZE(request_1),0))<0){
        goto main_exit_error;
    }
    return EXIT_SUCCESS;
main_exit_error:
    return EXIT_FAILURE;
}				/* ----------  end of function main  ---------- */
