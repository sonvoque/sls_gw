/*
|-------------------------------------------------------------------|
| HCMC University of Technology                                     |
| Telecommunications Departments                                    |
| Gateway software for controlling the SLS                          |
| Version: 1.0                                                      |
| Author: sonvq@hcmut.edu.vn                                        |
| Date: 01/2017                                                     |
|                                                                   |
| compile: gcc -o main main.c $(mysql_config --libs --cflags)       |
|-------------------------------------------------------------------|*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

//#include <mysql/my_global.h>
#include <mysql/mysql.h>

#include "sls.h"
#include "sls_cli.h"


#define BUFSIZE 2048
#define MAXBUF  sizeof(cmd_struct_t)
#define SERVICE_PORT	21234	/* hard-coded port number */

#define clear() printf("\033[H\033[J")
#define TIME_OUT    1      //seconds
#define NUM_RETRANSMISSIONS 1


static  struct  sockaddr_in6 rev_sin6;
static  int     rev_sin6len;
static  char    str_port[5];

static  int     rev_bytes;
static  char    rev_buffer[MAXBUF];
static  char    dst_ipv6addr[50];
static  char    cmd[20];
static  char    arg[32];

static node_db_struct_t node_db;
static node_db_struct_t node_db_list[100]; 

struct  pollfd fd;
int     res;

static  char    dst_ipv6addr_list[40][50];

static  cmd_struct_t  tx_cmd, rx_reply, emergency_reply;
static  cmd_struct_t *cmdPtr;
static  char *p;

// bien cho server
static  cmd_struct_t  pi_tx_cmd, pi_rx_reply;
static  cmd_struct_t *pi_cmdPtr;
static  char *pi_p; 

static  int node_id, num_of_node, timeout_val;

/*prototype definition */
static void print_cmd();
static void prepare_cmd();

static int read_node_list();
static void run_node_discovery();
static int ip6_send_cmd (int nodeid, int port, int retrans);
static void init_main();
static bool is_cmd_of_gw(cmd_struct_t cmd);
static void process_gw_cmd(cmd_struct_t cmd, int nodeid);
static void finish_with_error(MYSQL *con);
static void get_db_row(MYSQL_ROW row, int i);
static int execute_sql_cmd(char *sql);
static void show_sql_db();
static void show_local_db();
static void update_sql_db();
static void update_sql_row(int nodeid);
static int execute_broadcast_cmd(cmd_struct_t cmd, int val);
static int execute_multicast_cmd(cmd_struct_t cmd);
static int execute_broadcast_general_cmd(cmd_struct_t cmd);
static bool is_node_valid(int node);
static bool is_node_connected(int node);
static void auto_set_app_key();
static int convert_str2array(const char *hex_str, unsigned char *byte_array, int byte_array_max);
static void convert_array2str(unsigned char *bin, unsigned int binsz, char **result);

struct timeval t0;
struct timeval t1;
float elapsed;

time_t rawtime;
struct tm * timeinfo;

MYSQL *con;
char *sql_cmd; 
static char sql_server_ipaddr[20] ="localhost";
static char sql_username[20]= "root";
static char sql_password[20]= "Son100480";
static char sql_db[20] = "sls_db";


/*------------------------------------------------*/
void finish_with_error(MYSQL *con) {
#ifdef USING_SQL_SERVER    
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  //exit(1);        
#endif
}

/*------------------------------------------------*/
void init_main() {
    pid_t pid;
    char prog[10];

    timeout_val = TIME_OUT;
    strcpy(node_db_list[0].connected,"Y");
    ///update_sql_db();
    //show_sql_db();
}


/*------------------------------------------------*/
void get_db_row(MYSQL_ROW row, int i) {
#ifdef USING_SQL_SERVER    
    if (i==0)
        strcpy(node_db_list[i].connected,"Y");

    node_db_list[i].index       = atoi(row[0]);
    node_db_list[i].id          = atoi(row[1]);
    strcpy(node_db_list[i].ipv6_addr,row[2]);
    //strcpy(node_db_list[i].connected,row[3]);
    //node_db_list[i].num_req     = atoi(row[4]);
    //node_db_list[i].num_rep     = atoi(row[5]);
    //node_db_list[i].num_timeout = atoi(row[6]);
    //node_db_list[i].last_cmd    = atoi(row[7]);
    //strcpy(node_db_list[i].last_seen,row[8]);        
    //node_db_list[i].num_of_retrans = atoi(row[9]);
    strcpy(node_db_list[i].app_key,row[15]);
    strcpy(dst_ipv6addr_list[node_db_list[i].id], node_db_list[i].ipv6_addr);

    //printf("%02d | %02d | %s | %s | %02d | %02d | %02d | %02d | \n",nodedb->index , nodedb->id,nodedb->ipv6_addr, 
    //    nodedb->connected, nodedb->rx_cmd, nodedb->tx_rep,nodedb->num_timeout, nodedb->last_cmd );
#endif
}

/*------------------------------------------------*/
void auto_set_app_key() {
    int res, i, j;
    unsigned char byte_array[16];

    printf("\n\n Auto set app key process.....\n");
    for (i = 1; i < num_of_node; i++) {
        if (is_node_connected(i)) {
            tx_cmd.cmd = CMD_SET_APP_KEY;    
            tx_cmd.type = MSG_TYPE_HELLO;
            tx_cmd.err_code = 0;
            convert_str2array(node_db_list[i].app_key, byte_array, 16);
            //printf("Node %d key = %s \n", i, node_db_list[i].app_key );
            for (j = 0; j<16; j++) {
                tx_cmd.arg[j] = byte_array[j];
            }

            res = ip6_send_cmd(i, SLS_NORMAL_PORT, 1);
            if (res == -1)
                printf(" - ERROR: discovery process \n");
            else if (res == 0)
                printf(" - Set App Key for node %d [%s] failed \n", i, node_db_list[i].ipv6_addr);   
            /*
            else
                printf(" - Set App Key for node %d [%s] succesful \n", i, node_db_list[i].ipv6_addr);   
            */
        }    
    }
}


void update_sql_row(int nodeid) {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;

    char *result;
    char buf[MAX_CMD_DATA_LEN];

    memcpy(buf, &node_db_list[nodeid].last_emergency_msg, 20);    
    convert_array2str(buf, sizeof(buf), &result);    
    //printf("\nresult : %s\n", result);

    if (is_node_connected(nodeid)) {
        sprintf(sql,"UPDATE sls_db SET connected='Y', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s' WHERE node_id=%d;", 
                node_db_list[nodeid].num_req, node_db_list[nodeid].num_rep, node_db_list[nodeid].num_timeout, node_db_list[nodeid].last_cmd, 
                node_db_list[nodeid].last_seen, node_db_list[nodeid].num_of_retrans, node_db_list[nodeid].channel_id, node_db_list[nodeid].rssi, node_db_list[nodeid].lqi, 
                node_db_list[nodeid].pan_id, node_db_list[nodeid].tx_power,node_db_list[nodeid].last_err_code, 
                node_db_list[nodeid].num_emergency_msg, result, nodeid);
    }
    else {
        sprintf(sql,"UPDATE sls_db SET connected='N', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s' WHERE node_id=%d;", 
                node_db_list[nodeid].num_req, node_db_list[nodeid].num_rep, node_db_list[nodeid].num_timeout, node_db_list[nodeid].last_cmd, 
                node_db_list[nodeid].last_seen, node_db_list[nodeid].num_of_retrans, node_db_list[nodeid].channel_id, node_db_list[nodeid].rssi, node_db_list[nodeid].lqi, 
                node_db_list[nodeid].pan_id, node_db_list[nodeid].tx_power,node_db_list[nodeid].last_err_code, 
                node_db_list[nodeid].num_emergency_msg, result, nodeid);
    }    
    if (execute_sql_cmd(sql)==0){
        //printf("sql_cmd = %s\n", sql);
    }    

    free(result);    
#endif    
}

/*------------------------------------------------*/
void update_sql_db() {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;

    char *result;
    char buf[MAX_CMD_DATA_LEN];
    //convert_array2str(buf, sizeof(buf), &result);
    //printf("result : %s\n", result);
    //free(result);    


    for (i=1; i<num_of_node; i++) {
        //printf("node %d has connected = %s \n",i, node_db_list[i].connected);
        memcpy(buf, &node_db_list[i].last_emergency_msg, 20);    
        convert_array2str(buf, sizeof(buf), &result);    
        //printf("\nresult : %s\n", result);

        if (is_node_connected(i)) {
            //printf("node %d = 'Y' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='Y', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s' WHERE node_id=%d;", 
                node_db_list[i].num_req, node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, 
                node_db_list[i].last_seen, node_db_list[i].num_of_retrans, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, 
                node_db_list[i].pan_id, node_db_list[i].tx_power,node_db_list[i].last_err_code, node_db_list[i].num_emergency_msg, result, i);
        }
        else {
            //printf("node %d = 'N' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='N', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s' WHERE node_id=%d;", 
                node_db_list[i].num_req, node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, 
                node_db_list[i].last_seen, node_db_list[i].num_of_retrans, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, 
                node_db_list[i].pan_id, node_db_list[i].tx_power,node_db_list[i].last_err_code, node_db_list[i].num_emergency_msg, result, i);
        }    
        if (execute_sql_cmd(sql)==0){
            //printf("sql_cmd = %s\n", sql);
        }    
    }
#endif    
}

/*------------------------------------------------*/
void show_local_db() { 
    int i;
    printf("\n");
    printf("|--------------------------------------------------LOCAL DATABASE-------------------------------------------------------------|\n");
    printf("|node|       ipv6 address       |con.| req | rep.|time | last|retr.|      last_seen       |chan |RSSI/LQI/power|emger|err_code|\n");
    printf("| id |  (prefix: aaaa::0/64)    |nect| uest| ly  |-out | cmd | ies |         time         | nel |(dBm)/  /(dBm)|gency|  (hex) |\n");
    printf("|----|--------------------------|----|-----|-----|-----|-----|-----|----------------------|-----|--------------|-----|--------|\n");
    for(i = 0; i < num_of_node; i++) {
        if (i>0) 
            printf("| %2d | %24s | %2s |%5d|%5d|%5d| 0x%02X|%5d| %20s |%5d|%4d/%3u/%5X|%5d| 0x%04X |\n",node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, node_db_list[i].num_of_retrans,
                node_db_list[i].last_seen, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, node_db_list[i].tx_power, 
                node_db_list[i].num_emergency_msg, node_db_list[i].last_err_code);  
        else
            printf("| %2d | %24s | *%1s |%5d|%5d|%5d| 0x%02X|%5d| %20s |%5d|%4d/%3u/%5X|%5d| 0x%04X |\n",node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, node_db_list[i].num_of_retrans,
                node_db_list[i].last_seen, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, node_db_list[i].tx_power, 
                node_db_list[i].num_emergency_msg, node_db_list[i].last_err_code);
    }
    printf("|-----------------------------------------------------------------------------------------------------------------------------|\n");
}


/*------------------------------------------------*/
void show_sql_db() {
#ifdef USING_SQL_SERVER        
    int i, num_fields;

    con = mysql_init(NULL);
    if (con == NULL) {
      finish_with_error(con);
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username, sql_password, sql_db, 0, NULL, 0) == NULL) {
        finish_with_error(con);
    }  

    if (mysql_query(con, "SELECT * FROM sls_db")) {
        finish_with_error(con);
    }
  
    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) {
        finish_with_error(con);
    }

    printf("\n");
    printf("|---------------------------------------------SQL DATABASE---------------------------------------------------|\n");
    num_fields = mysql_num_fields(result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))  { 
        for(i = 0; i < num_fields; i++) {
            if (i==0)
                printf("| %3s | ", row[i] ? row[i] : "NULL");
            else if ((i==2) || (i==8))
                printf("%24s | ", row[i] ? row[i] : "NULL");
            else
                printf("%4s | ", row[i] ? row[i] : "NULL");
        }
        printf("\n");
    }
    printf("|------------------------------------------------------------------------------------------------------------|\n");

    mysql_free_result(result);
    mysql_close(con);   
#endif    
}

/*------------------------------------------------*/
int read_node_list(){
    bool sql_db_error;
    int     node;
    int num_fields;
    char    ipv6_addr[50];
   
    FILE *ptr_file;
    char buf[1000];

    num_of_node = 0;

#ifdef USING_SQL_SERVER        
    sql_db_error = false;
    con = mysql_init(NULL);

    printf("DATABASE: sls_db; MySQL client version: %s\n", mysql_get_client_info());
    if (con == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username,sql_password, sql_db,0,NULL,0) == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }  
    if (mysql_query(con, "SELECT * FROM sls_db")) {
        finish_with_error(con);
        sql_db_error = true;
    }
  
    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }
    else {
        printf("Reading DB: sls_db......\n");
    }

    num_fields = mysql_num_fields(result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))  { 
        //for(int i = 0; i < num_fields; i++)
        //    printf("%s ", row[i] ? row[i] : "NULL");
        get_db_row(row, num_of_node);        
        num_of_node++;
    }
    
    mysql_free_result(result);
    mysql_close(con);   
    update_sql_db();
#else    
    sql_db_error = true;
#endif    

    if (sql_db_error==false) {
        printf("SQL-DB succesfully reading node infor from SQL DB....\n");    
    }
    else {
        printf("SQL-DB error: reading node infor from config file....\n");    
        num_of_node =0;
        ptr_file =fopen("node_list.txt","r");
        if (!ptr_file)
            return 1;
        while (fgets(buf,1000, ptr_file)!=NULL) {
            sscanf(buf,"%d %s",&node, ipv6_addr);
            node_db_list[num_of_node].id = node;
            strcpy(node_db_list[node].ipv6_addr, ipv6_addr);
            //printf("node = %d,   ipv6 = %s\n",node, node_db_list[node].ipv6_addr);
            num_of_node++;
            }
        fclose(ptr_file);
    }
    printf("I. READ NODE LIST... DONE. Num of nodes: %d\n",num_of_node);
    show_local_db();
    return 0;
}

/*------------------------------------------------*/
bool is_broadcast_command(cmd_struct_t cmd) {
    return cmd.len = 0xFF;
}

bool is_multicast_command(cmd_struct_t cmd) {
    return cmd.len = 0xFE;
}

/*------------------------------------------------*/
bool is_node_valid(int node) {
    int i;
    for (i=0; i<num_of_node; i++) {
        if (node==node_db_list[i].id)
            return true;
    }
    return false;
}

/*------------------------------------------------*/
bool is_node_connected(int node) {
    return (char)node_db_list[node].connected[0]=='Y';
}

/*------------------------------------------------*/
int execute_sql_cmd(char *sql) {
#ifdef USING_SQL_SERVER            
    con = mysql_init(NULL);
    if (con == NULL) {
      finish_with_error(con);
      return 1;
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username, sql_password, sql_db, 0, NULL, 0) == NULL) {
        finish_with_error(con);
        return 1;
    }  

    if (mysql_query(con, sql)) {
        finish_with_error(con);
        return 1;
    }

    mysql_close(con);   
#endif
    return 0;
}
/*------------------------------------------------*/
void run_node_discovery(){
    char sql[200];
    int res, i;
    uint16_t rssi_rx;

    printf("II. RUNNING DISCOVERY PROCESS.....\n");
    for (i = 1; i < num_of_node; i++) {
        tx_cmd.type = MSG_TYPE_HELLO;
        tx_cmd.cmd = CMD_RF_HELLO;
        tx_cmd.err_code = 0;
        res = ip6_send_cmd(i, SLS_NORMAL_PORT, 1);
        if (res == -1) {
            printf(" - ERROR: discovery process \n");
        }
        else if (res == 0)   {
            printf(" - Node %d [%s] unavailable\n", i, node_db_list[i].ipv6_addr);
            sprintf(sql,"UPDATE sls_db SET connected='N' WHERE node_id=%d;", i);
            if (execute_sql_cmd(sql)==0) {
                //printf("sql_cmd = %s\n", sql);
            }
        }
        else{
            sprintf(sql,"UPDATE sls_db SET connected='Y' WHERE node_id=%d;", i);
            if (execute_sql_cmd(sql)==0) {
            }
            // rx_reply
            node_db_list[i].channel_id = rx_reply.arg[0];
            rssi_rx = rx_reply.arg[1];
            rssi_rx = (rssi_rx << 8) | rx_reply.arg[2];
            node_db_list[i].rssi = rssi_rx-200;
            node_db_list[i].lqi = rx_reply.arg[3];
            node_db_list[i].tx_power = rx_reply.arg[4];
            node_db_list[i].pan_id = (rx_reply.arg[5] << 8) | (rx_reply.arg[6]);
            printf(" - Node %d [%s] available\n", i, node_db_list[i].ipv6_addr);   
        }
    }
    update_sql_db();    
}
/*------------------------------------------------*/
void prepare_cmd() {
    tx_cmd.sfd = SFD;
    tx_cmd.seq ++;
}


/*------------------------------------------------*/
void print_cmd(cmd_struct_t command) {
    int i;
    printf("\nSFD=0x%02X; ",command.sfd);
    printf("node_id=%02d; ",command.len);
    printf("seq=%02d; ",command.seq);
    printf("type=0x%02X; ",command.type);
    printf("cmd=0x%02X; ",command.cmd);
    printf("err_code=0x%04X; \n",command.err_code); 
    printf("data=[");
    for (i=0;i<MAX_CMD_DATA_LEN;i++) 
        printf("%02X,",command.arg[i]);
    printf("]\n");
}  



/*------------------------------------------------*/
void convert_array2str(unsigned char *bin, unsigned int binsz, char **result) {
    char hex_str[]= "0123456789ABCDEF";
    unsigned int  i;

    *result = (char *)malloc(binsz * 2 + 1);
    (*result)[binsz * 2] = 0;

    if (!binsz)
        return;

    for (i = 0; i < binsz; i++) {
        (*result)[i * 2 + 0] = hex_str[(bin[i] >> 4) & 0x0F];
        (*result)[i * 2 + 1] = hex_str[(bin[i]     ) & 0x0F];
    }  
}

/*------------------------------------------------*/
int convert_str2array(const char *hex_str, unsigned char *byte_array, int byte_array_max) {
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



/*------------------------------------------------*/
float timedifference_msec(struct timeval t0, struct timeval t1){
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}


/*------------------------------------------------*/
bool is_cmd_of_gw(cmd_struct_t cmd) {
    return (cmd.cmd==CMD_GET_GW_STATUS) ||
            (cmd.cmd==CMD_GW_HELLO) ||
            (cmd.cmd==CMD_GW_SHUTDOWN) ||
            (cmd.cmd==CMD_GW_TURN_ON_ALL) ||
            (cmd.cmd==CMD_GW_TURN_OFF_ALL) ||
            (cmd.cmd==CMD_GW_DIM_ALL) ||            
            (cmd.cmd==CMD_GW_SET_TIMEOUT) ||
            (cmd.cmd==CMD_GW_BROADCAST_CMD) ||            
            (cmd.cmd==CMD_GW_GET_EMER_INFO) ||            
            (cmd.cmd==CMD_GW_MULTICAST_CMD);
}

/*------------------------------------------------*/
int execute_broadcast_cmd(cmd_struct_t cmd, int val) {
    int i, num_timeout, res, num_rep;
    uint16_t err_code;

    num_timeout=0;
    num_rep=0;
    printf("Executing broadcast CMD: 0x%02X, arg = %d ...\n", cmd.cmd, val);

    err_code = ERR_NORMAL;
    for (i = 1; i < num_of_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd.type = cmd.type;
        tx_cmd.cmd = cmd.cmd;     // CMD_LED_ON;
        tx_cmd.arg[0] = val;
        tx_cmd.err_code = 0;

        node_db_list[i].num_req++;
        res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS);
        if (res == -1) {
            printf(" - ERROR: broadcast process \n");
        }
        else if (res == 0)   {
            printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
            num_timeout++;
            node_db_list[i].num_timeout++;
            err_code = ERR_BROADCAST_CMD;
        }
        else{
            printf(" - Send CMD to node %d [%s] succesful\n", i, node_db_list[i].ipv6_addr);   
            num_rep++;
            node_db_list[i].num_rep++;
            node_db_list[i].last_cmd = tx_cmd.cmd;            
        }
    }
    rx_reply.err_code = err_code;
    rx_reply.arg[0] = num_of_node-1;
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;
}

/*------------------------------------------------*/
int execute_multicast_cmd(cmd_struct_t cmd) {
    uint8_t num_multicast_node, max_data_of_cmd;
    uint8_t i,j, num_timeout, res, num_rep, executed_node;
    uint16_t err_code;
    uint8_t multicast_cmd;


    /*  data = 20 
        multicast_cmd = data[0];
        multicast_data = data[1..6]
        multicast_node = data [7..19] */

    max_data_of_cmd = 6;    
    num_multicast_node = cmd.len;
    if (num_multicast_node > 13) {
        printf("Invalid number of multicast nodes, max = %d....\n", MAX_CMD_DATA_LEN-max_data_of_cmd-1);
        return 1;
    }
    multicast_cmd = cmd.arg[0];
    num_timeout=0;
    num_rep=0;
    printf("Executing multicast command: 0x%02X...\n", multicast_cmd);

    err_code = ERR_NORMAL;
    for (i = 0; i < num_multicast_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd.type = cmd.type;
        tx_cmd.len = 0xFF;      // multi-cast or broadcast
        tx_cmd.cmd = multicast_cmd;     
        for (j=0; j<max_data_of_cmd; j++)
            tx_cmd.arg[j] = cmd.arg[j+1];

        tx_cmd.err_code = 0;
        executed_node = cmd.arg[i+max_data_of_cmd+1];               //from arg[7] to...
        if (is_node_valid(executed_node)) {
            node_db_list[executed_node].num_req++;
            res = ip6_send_cmd(executed_node, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS);
            if (res == -1) {
                printf(" - ERROR: broadcast process \n");
            }
            else if (res == 0)   {
                printf(" - Send cmd to node %d [%s] failed\n", executed_node, node_db_list[executed_node].ipv6_addr);
                num_timeout++;
                node_db_list[executed_node].num_timeout++;
                err_code = ERR_BROADCAST_CMD;
            }
            else {
                printf(" - Send cmd to node %d [%s] succesful\n", executed_node, node_db_list[executed_node].ipv6_addr);   
                num_rep++;
                node_db_list[executed_node].num_rep++;
                node_db_list[executed_node].last_cmd = tx_cmd.cmd;            
            }
        }
    }
    rx_reply.err_code = err_code;
    rx_reply.arg[0] = num_multicast_node;
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;
    return 0;
}

/*------------------------------------------------*/
int execute_broadcast_general_cmd(cmd_struct_t cmd) {
    int i,j, num_timeout, res, num_rep;
    uint16_t err_code;
    uint8_t broadcast_cmd;

    num_timeout=0;
    num_rep=0;
    broadcast_cmd = cmd.arg[0];
    err_code = ERR_NORMAL;    
    printf("Executing broadcast general command: 0x%02X ...\n", broadcast_cmd);

    for (i = 1; i < num_of_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd = cmd;
        tx_cmd.type = MSG_TYPE_REQ;
        tx_cmd.len = 0xFF;      // multi-cast or broadcast
        tx_cmd.cmd = broadcast_cmd;     
        tx_cmd.err_code = 0;
        for (j = 0; j < (MAX_CMD_DATA_LEN-1); j++) {    
            tx_cmd.arg[j] = cmd.arg[j+1];
        }

        node_db_list[i].num_req++;
        res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS);
        if (res == -1) {
            printf(" - ERROR: broadcast process \n");
        }
        else if (res == 0)   {
            printf(" - Send cmd to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
            num_timeout++;
            node_db_list[i].num_timeout++;
            err_code = ERR_BROADCAST_CMD;
        }
        else {
            printf(" - Send cmd to node %d [%s] succesful\n", i, node_db_list[i].ipv6_addr);   
            num_rep++;
            node_db_list[i].num_rep++;
            node_db_list[i].last_cmd = tx_cmd.cmd;            
        }
    }
    rx_reply.err_code = err_code;
    rx_reply.arg[0] = num_of_node;
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;
    return 0;
}

/*------------------------------------------------*/
void process_gw_cmd(cmd_struct_t cmd, int nodeid) {
    switch (cmd.cmd) {
        case CMD_GW_HELLO:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_SET_TIMEOUT:
            rx_reply.type = MSG_TYPE_REP;
            timeout_val = (*pi_cmdPtr).arg[0];
            break;
        
        case CMD_GET_GW_STATUS:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_SHUTDOWN:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_TURN_ON_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcast_cmd(cmd, 170);
            //execute_broadcasd_cmd(CMD_RF_LED_ON,0);
            break;
        
        case CMD_GW_TURN_OFF_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcast_cmd(cmd, 0);
            //execute_broadcasd_cmd(CMD_RF_LED_OFF, 0);
            break;
        
        case CMD_GW_DIM_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcast_cmd(cmd, cmd.arg[0]);
            //execute_broadcasd_cmd(CMD_RF_LED_DIM, cmd.arg[0]);
            break;

        case CMD_GW_MULTICAST_CMD:
            rx_reply.type = MSG_TYPE_REP;
            execute_multicast_cmd(cmd);
            break;
        case CMD_GW_BROADCAST_CMD:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcast_general_cmd(cmd);
            break;

        case CMD_GW_GET_EMER_INFO:
            rx_reply.type = MSG_TYPE_REP;
            memcpy(&rx_reply.arg, &node_db_list[nodeid].last_emergency_msg, MAX_CMD_DATA_LEN);
            break;            
    }
}

/*------------------------------------------------*/
int find_node(char *ip_addr) {
    int i;
    for (i=1; i<num_of_node; i++) {
        if (strcmp(node_db_list[i].ipv6_addr,ip_addr)==0) {
            //printf("i= %d; node_id = %d \n",i, node_db_list[i].id);
            return node_db_list[i].id;
        }
    }
    return 0;
}

//-------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int res;
    int option = 1;

    struct sockaddr_in pi_myaddr;	                      /* our address */
	struct sockaddr_in pi_remaddr;	                    /* remote address */
	socklen_t pi_addrlen = sizeof(pi_remaddr);		   /* length of addresses */
	int pi_recvlen;			                            /* # bytes received */
	int pi_fd = 0, connfd = 0;				                         /* our socket */
	int pi_msgcnt = 0;			                          /* count # of messages we received */
	unsigned char pi_buf[BUFSIZE];	                        /* receive buffer */
    char buffer[MAXBUF];

    int emergency_node;
    int emergency_sock;
    int emergency_status;
    struct sockaddr_in6 sin6;
    int sin6len;
    int timeout = 0.2;      //200ms



    clear();
    init_main();
	/* create a UDP socket */
    //if ((pi_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	if ((pi_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("cannot create socket\n");
		//return 0;
	}
    setsockopt(pi_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	/* bind the socket to any valid IP address and a specific port */
	memset((char *)&pi_myaddr, 0, sizeof(pi_myaddr));
	pi_myaddr.sin_family = AF_INET;
	pi_myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	pi_myaddr.sin_port = htons(SERVICE_PORT);

	if (bind(pi_fd, (struct sockaddr *)&pi_myaddr, sizeof(pi_myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}
    
    listen(pi_fd, 3); 
    
    /* read node list*/
    read_node_list();

    /* running discovery */
    run_node_discovery();

#ifdef AUTO_SET_APP_KEY
    auto_set_app_key();
#endif

    update_sql_db();
    show_local_db();



    // setting UDP/IPv6 socket for emergency msg
    emergency_sock = socket(PF_INET6, SOCK_DGRAM,0);
    sin6len = sizeof(struct sockaddr_in6);
    memset(&sin6, 0, sin6len);
    /* just use the first address returned in the structure */
    sin6.sin6_port = htons(SLS_EMERGENCY_PORT);
    sin6.sin6_family = AF_INET6;
    sin6.sin6_addr = in6addr_any;
    emergency_status = bind(emergency_sock, (struct sockaddr *)&sin6, sin6len);
    if (-1 == emergency_status)
        perror("bind"), exit(1);
    emergency_status = getsockname(emergency_sock, (struct sockaddr *)&sin6, &sin6len);
    //printf("Gateway emergency_sock port for emergency: %d\n", ntohs(sin6.sin6_port));


    // main loop
    printf("\nIII. GATEWAY WAITING on PORT %d for COMMANDS\n", SERVICE_PORT);
    printf("\nIV. GATEWAY WAITING on PORT %d for EMERGENCY MSG\n", SLS_EMERGENCY_PORT);
    for (;;) {    
        // waiting for EMERGENCY msg
        //printf("1. GATEWAY WAITING on PORT %d for emergency msg\n", SLS_EMERGENCY_PORT);
        fd.fd = emergency_sock;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout*1000);
        if (res >0) {
            emergency_status = recvfrom(emergency_sock, buffer, MAXBUF, 0,(struct sockaddr *)&sin6, &sin6len);
            if (emergency_status>0) {
                p = (char *) (&buffer); 
                cmdPtr = (cmd_struct_t *)p;
                emergency_reply = *cmdPtr;
                inet_ntop(AF_INET6,&sin6.sin6_addr, buffer, sizeof(buffer));
                emergency_node = find_node(buffer);
                if (emergency_reply.err_code = ERR_EMERGENCY) {
                    printf("- Got an emergency msg [%d bytes] from node %d [%s]\n", emergency_status, emergency_node, buffer);
                    //printf("- Emergency type = 0x%02X, err_code = 0x%04X \n", emergency_reply.type, emergency_reply.err_code);                
                    node_db_list[emergency_node].num_emergency_msg++;
                    memcpy(node_db_list[emergency_node].last_emergency_msg,emergency_reply.arg, MAX_CMD_DATA_LEN);
                    
                    update_sql_row(emergency_node);
                    show_local_db();
                }
            }
        }
        //close(emergency_sock);
        //sleep(1);        


        // waiting for command from software
        // for UDP    
        //pi_recvlen = recvfrom(pi_fd, pi_buf, BUFSIZE, 0, (struct sockaddr *)&pi_remaddr, &pi_addrlen);

        //printf("\n2. GATEWAY WAITING on PORT %d for COMMANDS\n", SERVICE_PORT);
        fd.fd = pi_fd;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout*1000);   
        if (res>0) {
            connfd = accept(pi_fd, (struct sockaddr*)NULL, NULL);   //TCP
            if (connfd >= 0)
                printf("Accept TCP connection....\n");
        }
            
        // read data from client
        fd.fd = connfd;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout*1000); 
        if (res >0) {
            pi_recvlen = recv(connfd, &pi_buf, 1023, 0);
            if (pi_recvlen > 0) {
                printf("1. Received a COMMAND (%d bytes)\n", pi_recvlen);
               		
                pi_p = (char *) (&pi_buf); 
  		        pi_cmdPtr = (cmd_struct_t *)pi_p;
  		        pi_rx_reply = *pi_cmdPtr;
		
                node_id = pi_cmdPtr->len;
                rx_reply = *pi_cmdPtr;

                gettimeofday(&t0, 0);
                if (is_cmd_of_gw(*pi_cmdPtr)==true) {
                    printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
                    process_gw_cmd(*pi_cmdPtr, node_id);
                }
                else {  // not command for GW: send to node and wait for a reply
                    printf(" - Command Analysis: received LED command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
                    tx_cmd = *pi_cmdPtr;
                    gettimeofday(&t0, 0);
                    res = ip6_send_cmd(node_id, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS);
                    gettimeofday(&t1, 0);
                    elapsed = timedifference_msec(t0, t1);
                    printf(" - GW-Cmd execution delay %.2f (ms)\n", elapsed);
            
                    //update local DB
                    node_db_list[node_id].num_req++;
                    if (res == -1) {
                        printf(" - ERROR: sending process \n");
                    }
                    else if (res == 0)   {
                        //printf(" - Node %d [%s] unavailable\n", node_id, node_db_list[node_id].ipv6_addr);
                        node_db_list[node_id].num_timeout++;
                        rx_reply.err_code = ERR_TIME_OUT;
                        rx_reply.type = MSG_TYPE_REP;
                    }
                    else {
                        //printf(" - Node %d [%s] available\n", node_id, node_db_list[node_id].ipv6_addr);   
                        node_db_list[node_id].num_rep++;
                        node_db_list[node_id].last_cmd = tx_cmd.cmd;
                    }            
                }
                // send REPLY to sender NODE
                sprintf(pi_buf, "ACK %d", pi_msgcnt++);
                printf("4. Sending RESPONE to user \"%s\"\n", pi_buf);
                write(connfd, &rx_reply, sizeof(rx_reply)); //TCP
                show_local_db();
                update_sql_row(node_id);
                //if (sendto(pi_fd, &rx_reply, sizeof(rx_reply), 0, (struct sockaddr *)&pi_remaddr, pi_addrlen) < 0)
                //    perror("sendto");
                printf("\nIII. GATEWAY WAITING on PORT %d for COMMANDS\n", SERVICE_PORT);
            }
        }
        
        close(connfd);
        sleep(1);   
	}
    return 0;
}

//-------------------------------------------------------------------------------------------
int ip6_send_cmd(int nodeid, int port, int retrans) {
    int sock;
    int status, i;
    struct addrinfo sainfo, *psinfo;
    struct sockaddr_in6 sin6;
    int sin6len;
    char buffer[MAXBUF];
    char str_app_key[32];
    unsigned char byte_array[16];
    char str_time[80];
    int num_of_retrans;

    sin6len = sizeof(struct sockaddr_in6);
    strcpy(dst_ipv6addr,node_db_list[nodeid].ipv6_addr);
    sprintf(str_port,"%d",port);

    //print_cmd(tx_cmd);
    //printf("ipv6_send: node = %d, ipv6= %s\n",nodeid, dst_ipv6addr);  
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


    num_of_retrans = 0;
    while (num_of_retrans < retrans) {
        status = sendto(sock, &tx_cmd, sizeof(tx_cmd), 0,(struct sockaddr *)psinfo->ai_addr, sin6len);
        if (status > 0)     {
            printf("\n2. Forward REQUEST (%d bytes) to [%s]:%s, retry=%d  ....done\n",status, dst_ipv6addr,str_port, num_of_retrans);        
            //print_cmd(tx_cmd);
        }
        else {
            printf("\n2. Forward REQUEST to [%s]:%s, retry=%d  ....ERROR\n",dst_ipv6addr,str_port,num_of_retrans);  
        }    

        /*wait for a reply */
        fd.fd = sock;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout_val*1000); // timeout
        if (res == -1) {
            printf(" - ERROR: GW forwarding command \n");
        }
        else if (res == 0)   {
            printf(" - Time-out: GW forwarding command \n");
            rx_reply = tx_cmd;

            num_of_retrans++;
            /*update local DB*/
            //node_db_list[nodeid].num_timeout++;
            strcpy(node_db_list[nodeid].connected,"N");
            node_db_list[nodeid].num_of_retrans = num_of_retrans;
            time(&rawtime );
            timeinfo = localtime ( &rawtime );
            strftime(str_time,80,"%x-%I:%M:%S %p", timeinfo);
            strcpy (node_db_list[nodeid].last_seen, str_time);
            update_sql_row(nodeid);
        }
        else{
            // implies (fd.revents & POLLIN) != 0
            rev_bytes = recvfrom((int)sock, rev_buffer, MAXBUF, 0,(struct sockaddr *)(&rev_sin6), (socklen_t *) &rev_sin6len);
            if (rev_bytes>=0) {
                printf("3. Got REPLY (%d bytes):\n",rev_bytes);   
                p = (char *) (&rev_buffer); 
                cmdPtr = (cmd_struct_t *)p;
                rx_reply = *cmdPtr;

                //print_cmd(rx_reply);
                strcpy(node_db_list[nodeid].connected,"Y");
                node_db_list[nodeid].last_err_code = rx_reply.err_code;
                node_db_list[nodeid].num_of_retrans = num_of_retrans;

                time(&rawtime );
                timeinfo = localtime ( &rawtime );
                strftime(str_time,80,"%x-%I:%M:%S %p", timeinfo);
                strcpy (node_db_list[nodeid].last_seen, str_time);

                num_of_retrans = retrans;   // exit while loop
                update_sql_row(nodeid);
                break;
            }
        } /* while */    
    }

    shutdown(sock, 2);
    close(sock); 
    
    // free memory
    freeaddrinfo(psinfo);
    psinfo = NULL;
    return res;
}