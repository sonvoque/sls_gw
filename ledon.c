#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "sls.h"
#include "sls_cli.h"

#define MAXBUF  sizeof(cmd_struct_t)
// #define MAX_LENGTH 1024
// #define DELIMS " \t\r\n"

// static    int s_sock;
static  int     rev_bytes;
static  struct  sockaddr_in6 rev_sin6;
static  int     rev_sin6len;
static  char    rev_buffer[MAXBUF];
static  int     port;
static  char    dst_ipv6addr[50];
static  char    str_port[5];
static  char    cmd[20];
static  char    arg[32];

static  cmd_struct_t  tx_cmd, rx_reply;
static  cmd_struct_t *cmdPtr;
static  char *p;


/*prototype definition */
static void print_cmd();
static void prepare_cmd();


/*------------------------------------------------*/
void prepare_cmd() {
  tx_cmd.sfd = SFD;
  tx_cmd.len = sizeof(tx_cmd);
  tx_cmd.seq ++;
  //tx_cmd.type = MSG_TYPE_REQ;
  tx_cmd.err_code = 0;  
}


/*------------------------------------------------*/
void print_cmd(cmd_struct_t command) {
  int i;
  printf("SFD=0x%X; ",command.sfd);
  printf("len=%d; ",command.len);
  printf("seq=%d; ",command.seq);
  printf("type=0x%X; ",command.type);
  printf("cmd=0x%X; ",command.cmd);
  printf("err_code=0x%X; ",command.err_code); 
  printf("data=[");
  for (i=0;i<MAX_CMD_DATA_LEN;i++) 
    printf("0x%02X,",command.arg[i]);
  printf("]\n");
}  


/*------------------------------------------------*/
int convert(const char *hex_str, unsigned char *byte_array, int byte_array_max) {
    int hex_str_len = strlen(hex_str);
    int i = 0, j = 0;
    // The output array size is half the hex_str length (rounded up)
    int byte_array_size = (hex_str_len+1)/2;
    if (byte_array_size > byte_array_max) {
        // Too big for the output array
        return -1;
    }

    if (hex_str_len % 2 == 1){
        // hex_str is an odd length, so assume an implicit "0" prefix
        if (sscanf(&(hex_str[0]), "%1hhx", &(byte_array[0])) != 1){
            return -1;
        }
        i = j = 1;
    }

    for (; i < hex_str_len; i+=2, j++){
        if (sscanf(&(hex_str[i]), "%2hhx", &(byte_array[j])) != 1){
            return -1;
        }
    }
    return byte_array_size;
}


int main(int argc, char* argv[]) {
  int sock;
  int status, i;
  struct addrinfo sainfo, *psinfo;
  struct sockaddr_in6 sin6;
  int sin6len;
  char buffer[MAXBUF];
  char str_app_key[32];
  unsigned char byte_array[16];

  sin6len = sizeof(struct sockaddr_in6);

  sprintf(buffer,"led_off");
	port = 3000;
  sprintf(dst_ipv6addr,"aaaa::212:7401:1:101");
printf(dst_ipv6addr,"%s",argv[1]);      
    strcpy(str_port,argv[2]);
    strcpy(cmd,argv[3]);  
    port = atoi(str_port);
    sprintf(buffer,"%s",cmd);
    tx_cmd.cmd = CMD_LED_ON;
    tx_cmd.type = MSG_TYPE_REQ;	
     port = atoi(str_port);
   	sprintf(buffer,"%s %s",cmd,arg);
		
  prepare_cmd();


  strtok(buffer, "\n");

  sock = socket(PF_INET6, SOCK_DGRAM,0);

  memset(&sin6, 0, sizeof(struct sockaddr_in6));
  sin6.sin6_port = htons(port);
  sin6.sin6_family = AF_INET6;
  sin6.sin6_addr = in6addr_any;

  status = bind(sock, (struct sockaddr *)&sin6, sin6len);

  if(-1 == status)
    perror("bind"), exit(1);

  memset(&sainfo, 0, sizeof(struct addrinfo));
  memset(&sin6, 0, sin6len);

  sainfo.ai_flags = 0;
  sainfo.ai_family = PF_INET6;
  sainfo.ai_socktype = SOCK_DGRAM;
  sainfo.ai_protocol = IPPROTO_UDP;
  status = getaddrinfo(dst_ipv6addr, str_port, &sainfo, &psinfo);

  //status = sendto(sock, buffer, strlen(buffer), 0,
  //                   (struct sockaddr *)psinfo->ai_addr, sin6len);

  status = sendto(sock, &tx_cmd, sizeof(tx_cmd), 0,
                     (struct sockaddr *)psinfo->ai_addr, sin6len);
  printf("Send REQUEST (%d bytes) to [%s]:%s\n",status, dst_ipv6addr,str_port);
  print_cmd(tx_cmd);

  /*wait for a reply */
	rev_bytes = recvfrom((int)sock, rev_buffer, MAXBUF, 0,(struct sockaddr *)(&rev_sin6), (socklen_t *) &rev_sin6len);
	if (rev_bytes<0) {
    perror("Problem in recvfrom \n");
    exit(1);
  }
  else {
    printf("Got REPLY (%d bytes):\n",rev_bytes);   
    p = (char *) (&rev_buffer); 
    cmdPtr = (cmd_struct_t *)p;
    rx_reply = *cmdPtr;
    print_cmd(rx_reply);      
  }



  shutdown(sock, 2);
  close(sock); 

   // free memory
  freeaddrinfo(psinfo);
  psinfo = NULL;
  return 0;
}
