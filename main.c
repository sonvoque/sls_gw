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
#include "util.h"

#define BUFSIZE 2048
#define MAXBUF  sizeof(cmd_struct_t)
#define SERVICE_PORT	21234	/* hard-coded port number */

#define clear() printf("\033[H\033[J")
#define MAX_TIMEOUT     10   // seconds for a long chain topology 60 nodes
#define TIME_OUT    4      //seconds
#define NUM_RETRANSMISSIONS 3       // for command only


static  struct  sockaddr_in6 rev_sin6;
static  int     rev_sin6len;
static  char    str_port[5];

static  int     rev_bytes;
static  char    rev_buffer[MAXBUF];
static  char    dst_ipv6addr[40];
static  char    cmd[20];
static  char    arg[32];

static node_db_struct_t node_db;
static node_db_struct_t node_db_list[MAX_NUM_OF_NODE]; 

struct  pollfd fd;
int     res;

static  char    dst_ipv6addr_list[MAX_NUM_OF_NODE][40];

static  cmd_struct_t  tx_cmd, rx_reply, emergency_reply;
static  cmd_struct_t *cmdPtr;
static  char *p;

// bien cho server
static  cmd_struct_t  pi_tx_cmd, pi_rx_reply;
static  cmd_struct_t *pi_cmdPtr;
static  char *pi_p; 

static  int node_id, num_of_node, timeout_val;

/*prototype definition */

static void prepare_cmd();

static int  read_node_list();
static void run_node_discovery();
static bool authenticate_node(int node_id, uint32_t challenge_code, uint16_t challenge_res, int *res);
static int  ip6_send_cmd (int nodeid, int port, int retrans, bool encryption_en);
static void init_main();
static bool is_cmd_of_gw(cmd_struct_t cmd);
static void process_gw_cmd(cmd_struct_t cmd, int nodeid);


/* database functions */
static void finish_with_error(MYSQL *con);
static void get_db_row(MYSQL_ROW row, int i);
static int  execute_sql_cmd(char *sql);
static void show_sql_db();
static void show_local_db();
static void update1_sql_db();
static void update2_sql_db();
static void update_sql_db();
static void update1_sql_row(int nodeid);
static void update2_sql_row(int nodeid);
static void update_sql_row(int nodeid);

static int  execute_broadcast_cmd(cmd_struct_t cmd, int val, int mode);
static int  execute_multicast_cmd(cmd_struct_t cmd);
static int  execute_broadcast_general_cmd(cmd_struct_t cmd, int mode);
static bool is_node_valid(int node);
static bool is_node_connected(int node);
static void auto_set_app_key();
static void set_node_app_key (int node_id);
static void run_reload_gw_fw();
static int num_of_active_node();
static void reset_reply_data();
static char* add_ipaddr(char *buf, int nodeid);

static void gen_app_key_for_node(int nodeid);

static void make_packet_for_node(cmd_struct_t *cmd, uint16_t nodeid, bool encryption_en);
static bool check_packet_for_node(cmd_struct_t *cmd, uint16_t nodeid,  bool encryption_en);


struct timeval t0;
struct timeval t1;

time_t rawtime;
struct tm * timeinfo;

MYSQL *con;
char *sql_cmd; 

/* database infor */
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
void gen_app_key_for_node(int nodeid) {
    uint8_t i;
    unsigned char byte_array[16];
    char *result;

    gen_random_key_128(byte_array);
    convert_array2str(byte_array,sizeof(byte_array),&result);
    strcpy(node_db_list[nodeid].app_key, result);
    
    printf(" - Key for node = %d: ", nodeid);
    for (i = 0; i<16; i++) {
        printf("0x%02X,",byte_array[i]);
    }
    printf("\n");
    //printf("String key %s\n", node_db_list[nodeid].app_key);
}

/*------------------------------------------------*/
void init_main() {
    int i;
    timeout_val = TIME_OUT;
    strcpy(node_db_list[0].connected,"Y");
    node_db_list[0].authenticated = TRUE;


    printf("\n - GENERATE RANDOM KEYS (128 bits) FOR %d NODE(S) \n", num_of_node );
    for (i=1;i<num_of_node; i++) {
        gen_app_key_for_node(i);
        node_db_list[i].encryption_phase = FALSE;
    }
    printf("\n");
    update_sql_db();
    //show_sql_db();
}


/*------------------------------------------------*/
void set_node_app_key (int node_id) {
    int res, j, i;
    unsigned char byte_array[16];

    tx_cmd.cmd = CMD_SET_APP_KEY;    
    tx_cmd.type = MSG_TYPE_HELLO;
    tx_cmd.err_code = 0;
    convert_str2array(node_db_list[node_id].app_key, byte_array, 16);

    printf("\n - Set key for node %d, key: ", node_id);
    for (i = 0; i<16; i++) {
        printf("0x%02X,", byte_array[i]);
    }
    printf("\n");


    for (j = 0; j<16; j++) {
        tx_cmd.arg[j] = byte_array[j];
    }

    res = ip6_send_cmd(node_id, SLS_NORMAL_PORT, 1, false);
    if (res == -1) {
        printf(" - ERROR: set_node_app_key process \n");
    }
    else if (res == 0) {
        printf(" - Set App Key for node %d [%s] failed \n", node_id, node_db_list[node_id].ipv6_addr); 
    }  
    else { 
        printf(" - Set App Key for node %d [%s] successful \n", node_id, node_db_list[node_id].ipv6_addr);   
        node_db_list[node_id].encryption_phase = TRUE;
    }
}

/*------------------------------------------------*/
void auto_set_app_key() {
    int res, i, j;
    printf("\nIII. AUTO SET APP KEY PROCESS.....\n");
    for (i = 1; i < num_of_node; i++)
        if (is_node_connected(i)) 
            set_node_app_key(i);
}


/*------------------------------------------------*/
void update1_sql_row(int nodeid) {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;
    char *result;
    char buf[MAX_CMD_DATA_LEN];

    memcpy(buf, &node_db_list[nodeid].last_emergency_msg, 20);    
    convert_array2str(buf, sizeof(buf), &result);    
    //printf("\nresult : %s\n", result);

    if (is_node_connected(nodeid)) {
        sprintf(sql,"UPDATE sls_db SET connected='Y', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s'  WHERE node_id=%d;", 
                node_db_list[nodeid].num_req, node_db_list[nodeid].num_rep, node_db_list[nodeid].num_timeout, node_db_list[nodeid].last_cmd, 
                node_db_list[nodeid].last_seen, node_db_list[nodeid].num_of_retrans, node_db_list[nodeid].channel_id, node_db_list[nodeid].rssi, node_db_list[nodeid].lqi, 
                node_db_list[nodeid].pan_id, node_db_list[nodeid].tx_power,node_db_list[nodeid].last_err_code, 
                node_db_list[nodeid].num_emergency_msg, result,
                nodeid);
    }
    else {
        sprintf(sql,"UPDATE sls_db SET connected='N', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s'  WHERE node_id=%d;", 
                node_db_list[nodeid].num_req, node_db_list[nodeid].num_rep, node_db_list[nodeid].num_timeout, node_db_list[nodeid].last_cmd, 
                node_db_list[nodeid].last_seen, node_db_list[nodeid].num_of_retrans, node_db_list[nodeid].channel_id, node_db_list[nodeid].rssi, node_db_list[nodeid].lqi, 
                node_db_list[nodeid].pan_id, node_db_list[nodeid].tx_power,node_db_list[nodeid].last_err_code, 
                node_db_list[nodeid].num_emergency_msg, result, 
                nodeid);
    }    
    if (execute_sql_cmd(sql)==0){
        //printf("sql_cmd = %s\n", sql);
    }    

    free(result);    
#endif    
}

/*------------------------------------------------*/
void update2_sql_row(int nodeid) {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;

    if (is_node_connected(nodeid)) {
        sprintf(sql,"UPDATE sls_db SET connected='Y', next_hop_link_addr='%s',delay=%f,rdr=%f  WHERE node_id=%d;", 
                node_db_list[nodeid].next_hop_link_addr, node_db_list[nodeid].delay, node_db_list[nodeid].rdr,
                nodeid);
    }
    else {
        sprintf(sql,"UPDATE sls_db SET connected='N', next_hop_link_addr='%s',delay=%f,rdr=%f  WHERE node_id=%d;", 
                node_db_list[nodeid].next_hop_link_addr,node_db_list[nodeid].delay, node_db_list[nodeid].rdr,
                nodeid);
    }    
    if (execute_sql_cmd(sql)==0){
        //printf("sql_cmd = %s\n", sql);
    }    
    //free(result);    
#endif    
}

/*------------------------------------------------*/
void update_sql_row(int nodeid) {
    update1_sql_row(nodeid);
    update2_sql_row(nodeid);
}

/*------------------------------------------------*/
void update1_sql_db() {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;

    char *result;
    char buf[MAX_CMD_DATA_LEN];

    for (i=1; i<num_of_node; i++) {
        //printf("node %d has connected = %s \n",i, node_db_list[i].connected);
        memcpy(buf, &node_db_list[i].last_emergency_msg, 20);    
        convert_array2str(buf, sizeof(buf), &result);    
        //printf("\nresult : %s\n", result);

        if (is_node_connected(i)) {
            //printf("node %d = 'Y' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='Y', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s'  WHERE node_id=%d;", 
                node_db_list[i].num_req, node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, 
                node_db_list[i].last_seen, node_db_list[i].num_of_retrans, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, 
                node_db_list[i].pan_id, node_db_list[i].tx_power,node_db_list[i].last_err_code, node_db_list[i].num_emergency_msg, result, i);
        }
        else {
            //printf("node %d = 'N' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='N', num_req=%d, num_rep=%d, num_timeout=%d, last_cmd=%d, last_seen='%s', num_of_retrans=%d, rf_channel=%d, rssi=%d, lqi=%d, pan_id=%d, tx_power=%d, last_err_code=%d, num_emergency_msg=%d, last_emergency_msg='%s'  WHERE node_id=%d;", 
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
void update2_sql_db() {
#ifdef USING_SQL_SERVER    
    char sql[400];
    int i;

    for (i=1; i<num_of_node; i++) {
        if (is_node_connected(i)) {
            //printf("node %d = 'Y' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='Y', next_hop_link_addr='%s', delay=%f, rdr=%f, app_key='%s'  WHERE node_id=%d;", 
                node_db_list[i].next_hop_link_addr,node_db_list[i].delay, node_db_list[i].rdr, node_db_list[i].app_key, i);
        }
        else {
            //printf("node %d = 'N' \n",i);
            sprintf(sql,"UPDATE sls_db SET connected='N', next_hop_link_addr='%s', delay=%f, rdr=%f, app_key='%s'  WHERE node_id=%d;", 
                node_db_list[i].next_hop_link_addr,node_db_list[i].delay, node_db_list[i].rdr, node_db_list[i].app_key, i);
        }    
        if (execute_sql_cmd(sql)==0){
            //printf("sql_cmd = %s\n", sql);
        }    
    }
#endif    
}

/*------------------------------------------------*/
void update_sql_db() {
    update1_sql_db();
    update2_sql_db();    
}

/*------------------------------------------------*/
void show_local_db() { 
    int i;
    printf("\nLOCAL DATABASE: table_size = %d (bytes); node_size = %d, num_of_nodes = %d \n", sizeof(node_db_list), 
        sizeof(node_db_struct_t), num_of_node);
    printf("|----|--------------------------|----|-----|-----|-----|-----|-----|-------------------|-----|--------------|-----|--------|------|-------|-----|\n");
    printf("|node|       ipv6 address       |con/| req | rep.|time | last|retr.|    last_seen      |chan |RSSI/LQI/power|emger|err_code| next | delay | rdr |\n");
    printf("| id |  (prefix: aaaa::0/64)    |auth| uest| ly  |-out | cmd | ies |       time        | nel |(dBm)/  /(dBm)|gency|  (hex) |  hop |  (ms) |     |\n");
    printf("|----|--------------------------|----|-----|-----|-----|-----|-----|-------------------|-----|--------------|-----|--------|------|-------|-----|\n");
    for(i = 0; i < num_of_node; i++) {
        if (i>0) 
            printf("| %2d | %24s |%2s/%1d|%5d|%5d|%5d| 0x%02X|%5d| %17s |%5d|%4d/%3u/%5X|%5d| 0x%04X | _%02X%02X|%7.02f|%5.1f|\n",node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected,node_db_list[i].authenticated, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, node_db_list[i].num_of_retrans,
                node_db_list[i].last_seen, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, node_db_list[i].tx_power, 
                node_db_list[i].num_emergency_msg, node_db_list[i].last_err_code,
                node_db_list[i].next_hop_addr[14],node_db_list[i].next_hop_addr[15], node_db_list[i].delay, node_db_list[i].rdr);  
        else
            printf("| %2d | %24s |*%1s/%1d|%5d|%5d|%5d| 0x%02X|%5d| %17s |%5d|%4d/%3u/%5X|%5d| 0x%04X | _%02X%02X|%7.02f|%5.1f|\n",node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected,node_db_list[i].authenticated, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, node_db_list[i].num_of_retrans,
                node_db_list[i].last_seen, node_db_list[i].channel_id, node_db_list[i].rssi, node_db_list[i].lqi, node_db_list[i].tx_power, 
                node_db_list[i].num_emergency_msg, node_db_list[i].last_err_code,
                node_db_list[i].next_hop_addr[14],node_db_list[i].next_hop_addr[15], (double)0,node_db_list[i].rdr);
    }
    printf("|----|--------------------------|----|-----|-----|-----|-----|-----|-------------------|-----|--------------|-----|--------|------|-------|-----|\n");
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
        //printf("Reading DB: sls_db......\n");
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
        printf("SQL-DB: successfully reading node infor from DB....\n");    
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
static char * add_ipaddr(char *buf, int nodeid) {
  uint16_t a;
  unsigned int i;
  int f;
  char *p = buf;

    for(i = 0, f = 0; i < 16; i += 2) {
        a = (node_db_list[nodeid].next_hop_addr[i] << 8) + node_db_list[nodeid].next_hop_addr[i + 1];
        if(a == 0 && f >= 0) {
            if(f++ == 0) {
                p += sprintf(p, "::");
            }
        } else {
        if(f > 0) {
            f = -1;
        } else if(i > 0) {
            p += sprintf(p, ":");
        }
        p += sprintf(p, "%04x", a);
        }
    }
    return p;
}



/*------------------------------------------------*/
bool authenticate_node(int node_id, uint32_t challenge_code, uint16_t challenge_res, int *res) {
    bool result;
    int response;
    uint32_t rx_res, code;

    result = false;
    code = challenge_code;
    tx_cmd.type = MSG_TYPE_HELLO;
    tx_cmd.cmd = CMD_RF_AUTHENTICATE;
    tx_cmd.err_code = 0;
    tx_cmd.arg[0]= (code >> 8) & 0xFF;
    tx_cmd.arg[1]= code  & 0xFF;

    response = ip6_send_cmd(node_id, SLS_NORMAL_PORT, 1, false);
    if (response == -1) {
        printf(" - ERROR: authetication node %d \n", node_id);
    }
    else if (response == 0)   {
    }
    else {
        rx_res = (rx_reply.arg[0] <<8) | (rx_reply.arg[1]);
        printf(" - Receiving challenge response = 0x%04X \n", rx_res);
        result = (rx_res==challenge_res);
    }

    update_sql_row(node_id);    
    *res = response;
    return result;
}



/*------------------------------------------------*/
void run_node_discovery(){
    char sql[200];
    int res, i,j;
    uint16_t rssi_rx;
    char *p;
    char buf[100];
    bool node_authenticated = false;
    uint32_t rx_res;


    printf("II. RUNNING DISCOVERY PROCESS.....\n");
    for (i = 1; i < num_of_node; i++) {
        node_db_list[i].challenge_code = gen_random_num();
        node_db_list[i].challenge_code_res = hash(node_db_list[i].challenge_code);
        printf("\n1. Send challenge_code = 0x%04X to node = %d\n",node_db_list[i].challenge_code, i );
        printf(" - Expected challenge_code_res = 0x%04X \n",node_db_list[i].challenge_code_res);

        gettimeofday(&t0, 0);
        node_authenticated = authenticate_node(i, node_db_list[i].challenge_code, node_db_list[i].challenge_code_res, &res);
        gettimeofday(&t1, 0);
        node_db_list[i].delay = timedifference_msec(t0, t1);
        printf(" - Roundtrip delay %.2f (ms)\n", node_db_list[i].delay);

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
            if (node_authenticated) {
                printf(" - Node %d authenticated \n", i);
                node_db_list[i].authenticated = TRUE;
                // rx_reply
                node_db_list[i].channel_id = rx_reply.arg[4];
                rssi_rx = rx_reply.arg[5];
                rssi_rx = (rssi_rx << 8) | rx_reply.arg[6];
                node_db_list[i].rssi = rssi_rx-200;
                node_db_list[i].lqi = rx_reply.arg[7];
                node_db_list[i].tx_power = rx_reply.arg[8];
                node_db_list[i].pan_id = (rx_reply.arg[9] << 8) | (rx_reply.arg[10]);
                for (j=0; j<16; j++) {
                    node_db_list[i].next_hop_addr[j] = rx_reply.arg[11+j];
                } 
                add_ipaddr(buf,i);
                strcpy(node_db_list[i].next_hop_link_addr, buf);
                printf(" - Node %d [%s] available, next-hop link addr = %s\n", i, node_db_list[i].ipv6_addr, node_db_list[i].next_hop_link_addr);
                //printf("Next hop IPv6 link addr = %s \n", buf);
            }
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
bool is_cmd_of_gw(cmd_struct_t cmd) {
    return (cmd.cmd==CMD_GET_GW_STATUS) ||
            (cmd.cmd==CMD_GW_HELLO) ||
            (cmd.cmd==CMD_GW_SHUTDOWN) ||
            (cmd.cmd==CMD_GW_TURN_ON_ALL) ||
            (cmd.cmd==CMD_GW_TURN_OFF_ALL) ||
            (cmd.cmd==CMD_GW_DIM_ALL) ||            
            (cmd.cmd==CMD_GW_SET_TIMEOUT) ||
            (cmd.cmd==CMD_GW_BROADCAST_CMD) ||            
            (cmd.cmd==CMD_GW_MULTICAST_CMD) ||
            (cmd.cmd==CMD_GW_GET_EMER_INFO) ||   

            (cmd.cmd==CMD_GW_TURN_ON_ODD) ||      
            (cmd.cmd==CMD_GW_TURN_ON_EVEN) ||      
            (cmd.cmd==CMD_GW_TURN_ON_ODD) ||      
            (cmd.cmd==CMD_GW_TURN_OFF_ODD) ||      
            (cmd.cmd==CMD_GW_DIM_ODD) ||        
            (cmd.cmd==CMD_GW_DIM_EVEN) ||        
                            
            (cmd.cmd==CMD_GW_RELOAD_FW);
}

/*------------------------------------------------*/
/* mode = 2: broadcast all
   mode = 1: odd
   mode = 0: even */
int execute_broadcast_cmd(cmd_struct_t cmd, int val, int mode) {
    int i, num_timeout, res, num_rep;
    uint16_t err_code;

    num_timeout=0;
    num_rep=0;
    printf(" - Executing broadcast commmand: subcmd = 0x%02X, arg = %d ...\n", cmd.cmd, val);

    err_code = ERR_NORMAL;
    for (i = 1; i < num_of_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd.type = cmd.type;
        tx_cmd.cmd = cmd.cmd;     // CMD_LED_ON;
        tx_cmd.arg[0] = val;
        tx_cmd.err_code = 0;

        if (mode==ALL_NODE) {
            node_db_list[i].num_req++;
            res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
            if (res == -1) {
                printf(" - ERROR: broadcast process \n");
                err_code = ERR_BROADCAST_CMD;
            }
            else if (res == 0)   {
                printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                num_timeout++;
                node_db_list[i].num_timeout++;
                err_code = ERR_TIME_OUT;
            }
            else{
                printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                num_rep++;
                node_db_list[i].num_rep++;
                node_db_list[i].last_cmd = tx_cmd.cmd;            
                node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;                        
            }
        }

        /* odd led */
        else if (mode==ODD_NODE) {
            if ((i % 2)==1) {
                node_db_list[i].num_req++;
                res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
                if (res == -1) {
                    printf(" - ERROR: broadcast process \n");
                    err_code = ERR_BROADCAST_CMD;
                }
                else if (res == 0)   {
                    printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                    num_timeout++;
                    node_db_list[i].num_timeout++;
                    err_code = ERR_TIME_OUT;
                }
                else{
                    printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                    num_rep++;
                    node_db_list[i].num_rep++;
                    node_db_list[i].last_cmd = tx_cmd.cmd;            
                    node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;                        
                }
            }   
        }
        /* even */
        else if (mode==EVEN_NODE) {
            if (((i % 2)==0) && (i!=0)){
                node_db_list[i].num_req++;
                res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
                if (res == -1) {
                    printf(" - ERROR: broadcast process \n");
                    err_code = ERR_BROADCAST_CMD;
                }
                else if (res == 0)   {
                    printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                    num_timeout++;
                    node_db_list[i].num_timeout++;
                    err_code = ERR_TIME_OUT;
                }
                else{
                    printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                    num_rep++;
                    node_db_list[i].num_rep++;
                    node_db_list[i].last_cmd = tx_cmd.cmd;        
                    node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;                        
                }
            }   

        } 
    }

    rx_reply.err_code = err_code;
    memset(&rx_reply.arg,0,MAX_CMD_DATA_LEN);   //reset data of reply
    rx_reply.arg[0] = num_of_node-1; //except node 0
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;

    return 0;
}

/*------------------------------------------------*/
int execute_multicast_cmd(cmd_struct_t cmd) {
    uint8_t     num_multicast_node, max_data_of_cmd;
    uint8_t     i,j, num_timeout, res, num_rep, executed_node;
    uint16_t    err_code;
    uint8_t     multicast_cmd;

    /*  data = 54 bytes
        multicast_cmd = data[0];
        multicast_data = data[1..10]
        multicast_node = data [11..53] */
    max_data_of_cmd = 10;    
    num_multicast_node = cmd.len;
    if (num_multicast_node > (MAX_CMD_DATA_LEN-max_data_of_cmd-1) ) {
        printf("Invalid number of multicast nodes, max = %d....\n", MAX_CMD_DATA_LEN-max_data_of_cmd-1);
        return -1;
    }
    multicast_cmd = cmd.arg[0];
    num_timeout=0;
    num_rep=0;
    printf(" - Executing multicast command: subcmd = 0x%02X...\n", multicast_cmd);

    err_code = ERR_NORMAL;
    for (i = 0; i < num_multicast_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd.type = cmd.type;
        tx_cmd.len = 0xFF;      // multi-cast or broadcast
        tx_cmd.cmd = multicast_cmd;     
        tx_cmd.err_code = 0;
        for (j=0; j<max_data_of_cmd; j++)
            tx_cmd.arg[j] = cmd.arg[j+1];

        executed_node = cmd.arg[i + max_data_of_cmd + 1];               //from arg[7] to...
        if (is_node_valid(executed_node)) {
            node_db_list[executed_node].num_req++;
            res = ip6_send_cmd(executed_node, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
            if (res == -1) {
                printf(" - ERROR: broadcast process \n");
                err_code = ERR_MULTICAST_CMD;
            }
            else if (res == 0)   {
                printf(" - Send CMD to node %d [%s] failed\n", executed_node, node_db_list[executed_node].ipv6_addr);
                num_timeout++;
                node_db_list[executed_node].num_timeout++;
                err_code = ERR_TIME_OUT;
            }
            else {
                printf(" - Send CMD to node %d [%s] successful\n", executed_node, node_db_list[executed_node].ipv6_addr);   
                num_rep++;
                node_db_list[executed_node].num_rep++;
                node_db_list[executed_node].last_cmd = tx_cmd.cmd;        
                node_db_list[executed_node].rdr = 100*node_db_list[executed_node].num_rep/node_db_list[executed_node].num_req;
            }
        }
    }

    rx_reply.err_code = err_code;
    memset(&rx_reply.arg,0,MAX_CMD_DATA_LEN);   //reset data of reply
    rx_reply.arg[0] = num_of_node-1; //except node 0
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;
    return 0;
}

/*------------------------------------------------*/
int execute_broadcast_general_cmd(cmd_struct_t cmd, int mode) {
    int i,j, num_timeout, res, num_rep;
    uint16_t err_code;
    uint8_t broadcast_cmd;

    num_timeout=0;
    num_rep=0;
    broadcast_cmd = cmd.arg[0];
    err_code = ERR_NORMAL;    
    printf(" - Executing broadcast general command: subcmd = 0x%02X ...\n", broadcast_cmd);

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

        if (mode==2) {
            node_db_list[i].num_req++;
            res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
            if (res == -1) {
                printf(" - ERROR: broadcast process \n");
                err_code = ERR_BROADCAST_CMD;
            }
            else if (res == 0)   {
                printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                num_timeout++;
                node_db_list[i].num_timeout++;
                err_code = ERR_TIME_OUT;
            }
            else {
                printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                num_rep++;
                node_db_list[i].num_rep++;
                node_db_list[i].last_cmd = tx_cmd.cmd;            
                node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;
            }
        }

        /* odd */
        else if (mode==1){
            if ((i%2)==1) {
                node_db_list[i].num_req++;
                res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
                if (res == -1) {
                    printf(" - ERROR: broadcast process \n");
                    err_code = ERR_BROADCAST_CMD;
                }
                else if (res == 0)   {
                    printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                    num_timeout++;
                    node_db_list[i].num_timeout++;
                    err_code = ERR_TIME_OUT;
                }
                else {
                    printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                    num_rep++;
                    node_db_list[i].num_rep++;
                    node_db_list[i].last_cmd = tx_cmd.cmd;   
                    node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;                             
                }
            }
        }
        /* even */
        else if (mode==0){
            if (((i % 2)==0) && (i!=0)){
                node_db_list[i].num_req++;
                res = ip6_send_cmd(i, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
                if (res == -1) {
                    printf(" - ERROR: broadcast process \n");
                    err_code = ERR_BROADCAST_CMD;
                }
                else if (res == 0)   {
                    printf(" - Send CMD to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
                    num_timeout++;
                    node_db_list[i].num_timeout++;
                    err_code = ERR_TIME_OUT;
                }
                else {
                    printf(" - Send CMD to node %d [%s] successful\n", i, node_db_list[i].ipv6_addr);   
                    num_rep++;
                    node_db_list[i].num_rep++;
                    node_db_list[i].last_cmd = tx_cmd.cmd;      
                    node_db_list[i].rdr = 100*node_db_list[i].num_rep/node_db_list[i].num_req;                          
                }
            }
        }
    }

    rx_reply.err_code = err_code;
    memset(&rx_reply.arg,0,MAX_CMD_DATA_LEN);   //reset data of reply
    rx_reply.arg[0] = num_of_node-1; //except node 0
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
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, 100, ALL_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_ON,0);
            break;

        case CMD_GW_TURN_ON_ODD:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, 100, ODD_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_ON,170,1);
            break;
        
        case CMD_GW_TURN_ON_EVEN:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, 100, EVEN_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_ON,170,0);
            break;

        case CMD_GW_TURN_OFF_ALL:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, 170, ALL_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_OFF, 0);
            break;
        
        case CMD_GW_TURN_OFF_ODD:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, 170, ODD_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_ON,0);
            break;
        
        case CMD_GW_TURN_OFF_EVEN:
            cmd.cmd = CMD_LED_DIM;            
            execute_broadcast_cmd(cmd, 170, EVEN_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_ON,0);
            break;

        case CMD_GW_DIM_ALL:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, cmd.arg[0], ALL_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_DIM, cmd.arg[0]);
            break;

        case CMD_GW_DIM_ODD:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, cmd.arg[0], ODD_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_DIM, cmd.arg[0]);
            break;

        case CMD_GW_DIM_EVEN:
            cmd.cmd = CMD_LED_DIM;
            execute_broadcast_cmd(cmd, cmd.arg[0], EVEN_NODE);
            rx_reply.type = MSG_TYPE_REP;
            //execute_broadcasd_cmd(CMD_RF_LED_DIM, cmd.arg[0]);
            break;

        case CMD_GW_MULTICAST_CMD:
            execute_multicast_cmd(cmd);
            rx_reply.type = MSG_TYPE_REP;
            break;

        case CMD_GW_BROADCAST_CMD:
            execute_broadcast_general_cmd(cmd, 2);
            rx_reply.type = MSG_TYPE_REP;
            break;

        case CMD_GW_GET_EMER_INFO:
            memcpy(&rx_reply.arg, &node_db_list[nodeid].last_emergency_msg, MAX_CMD_DATA_LEN);
            rx_reply.type = MSG_TYPE_REP;
            break;            

        case CMD_GW_RELOAD_FW:
            run_reload_gw_fw();
            rx_reply.type = MSG_TYPE_REP;
            reset_reply_data();
            rx_reply.arg[0]= num_of_active_node();
            break;            
    }
    //rx_reply.type = MSG_TYPE_REP;
}


/*------------------------------------------------*/
void reset_reply_data(){
    int i;
    for (i=0; i<MAX_CMD_DATA_LEN; i++) {
        rx_reply.arg[i]=0;
    }
}

/*------------------------------------------------*/
int num_of_active_node(){    
    int i, num_active_node;

    num_active_node = 0;
    for (i=1; i<num_of_node; i++) {
        if ((char)node_db_list[i].connected[0]=='Y') {
            num_active_node++;
        }
    }
    return num_active_node;
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
    return -1;
}


/*------------------------------------------------*/
static void run_reload_gw_fw(){
    clear();
    read_node_list();
    init_main();
    run_node_discovery();
#ifdef AUTO_SET_APP_KEY
    auto_set_app_key();
#endif
    //run_node_authentication();
    update_sql_db();
    show_local_db();    
}

//-------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int res, i, j, result;
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

    char str_time[80];

    uint16_t rssi_rx, old_seq;
    char buf[100];
    bool node_authenticated = false;
    uint32_t rx_res;



    clear();
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
    init_main();


    /* should wait approximately 5-7s here for a 60-node chain topology */
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
    printf("\nIV. GATEWAY WAITING on PORT %d for COMMANDS, and PORT %d for ASYNC MSG\n", SERVICE_PORT, SLS_EMERGENCY_PORT);
    //printf("\n V. GATEWAY WAITING on PORT %d for ASYNC MSG\n", SLS_EMERGENCY_PORT);
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
                result = find_node(buffer);
                if (result == -1) {
                    printf(" - Got a msg from node [%s]: not found in DB \n", buffer);                    
                }
                else {
                    emergency_node = result;

                    // check crc here
                    check_packet_for_node(&emergency_reply, emergency_node, false);

                   if (emergency_reply.type == MSG_TYPE_ASYNC) {   
                        printf(" - Got an emergency msg [%d bytes] from node %d [%s]\n", emergency_status, emergency_node, buffer);
                        //printf("- Emergency type = 0x%02X, err_code = 0x%04X \n", emergency_reply.type, emergency_reply.err_code);
                        node_db_list[emergency_node].num_emergency_msg++;
                        memcpy(node_db_list[emergency_node].last_emergency_msg,emergency_reply.arg, MAX_CMD_DATA_LEN);
                    
                        time(&rawtime );
                        timeinfo = localtime(&rawtime );
                        strftime(str_time,80,"%x-%H:%M:%S", timeinfo);
                        strcpy (node_db_list[emergency_node].last_seen, str_time);
                        strcpy(node_db_list[emergency_node].connected,"Y");

                        if (emergency_reply.cmd == ASYNC_MSG_SENT) { // read sensor data here
                            printf(" - Emergency data: %d bytes \n",MAX_CMD_DATA_LEN);
                            for (i=0; i<MAX_CMD_DATA_LEN; i++)
                                printf("%d,",emergency_reply.arg[i]);     
                            printf("\n");
                        }

                        //send authentication here
                        if (emergency_reply.cmd == ASYNC_MSG_JOINED) {
                        //if (node_db_list[emergency_node].authenticated==FALSE) {
                            node_db_list[emergency_node].encryption_phase = FALSE;
                            printf(" - Node %d want to join network \n",emergency_node);
                            node_db_list[emergency_node].challenge_code = gen_random_num();
                            node_db_list[emergency_node].challenge_code_res = hash(node_db_list[emergency_node].challenge_code);
                            printf("\n1. Send challenge code = 0x%04X to joined node %d\n",node_db_list[emergency_node].challenge_code,emergency_node );
                            printf(" - Expected challenge response = 0x%04X \n",node_db_list[emergency_node].challenge_code_res);

                            gettimeofday(&t0, 0);
                            node_authenticated = authenticate_node(emergency_node, node_db_list[emergency_node].challenge_code, node_db_list[emergency_node].challenge_code_res, &res);
                            gettimeofday(&t1, 0);
                            node_db_list[emergency_node].delay = timedifference_msec(t0, t1);
                            printf(" - Roundtrip delay %.2f (ms)\n", node_db_list[emergency_node].delay);

                            if (res == -1) {
                                printf(" - ERROR: authetication process \n");
                            }
                            else if (res == 0)   {
                                printf(" - Node %d [%s] unavailable\n", i, node_db_list[emergency_node].ipv6_addr);
                            }
                            else{
                                if (node_authenticated==TRUE) {
                                    printf(" - Node %d authenticated \n", emergency_node);
                                    node_db_list[emergency_node].authenticated = TRUE;
                                    // rx_reply
                                    node_db_list[emergency_node].channel_id = rx_reply.arg[4];
                                    rssi_rx = rx_reply.arg[5];
                                    rssi_rx = (rssi_rx << 8) | rx_reply.arg[6];
                                    node_db_list[emergency_node].rssi = rssi_rx-200;
                                    node_db_list[emergency_node].lqi = rx_reply.arg[7];
                                    node_db_list[emergency_node].tx_power = rx_reply.arg[8];
                                    node_db_list[emergency_node].pan_id = (rx_reply.arg[9] << 8) | (rx_reply.arg[10]);
                                    for (j=0; j<16; j++) {
                                        node_db_list[emergency_node].next_hop_addr[j] = rx_reply.arg[11+j];
                                    } 
                                    add_ipaddr(buf,emergency_node);
                                    strcpy(node_db_list[emergency_node].next_hop_link_addr, buf);
                                    printf(" - Node %d [%s] available, next-hop link addr = %s\n", emergency_node, node_db_list[emergency_node].ipv6_addr, node_db_list[emergency_node].next_hop_link_addr);
                                    set_node_app_key(emergency_node);
                                }
                            }
                        }

                        update_sql_row(emergency_node);
                        show_local_db();
                    }
                }    
            }
        }
        //close(emergency_sock);
        sleep(1);        


        // waiting for command from software
        // for UDP    
        //pi_recvlen = recvfrom(pi_fd, pi_buf, BUFSIZE, 0, (struct sockaddr *)&pi_remaddr, &pi_addrlen);

        //printf("\n2. GATEWAY WAITING on PORT %d for COMMANDS\n", SERVICE_PORT);
        fd.fd = pi_fd;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout*1000);   
        if (res>0) {
            connfd = accept(pi_fd, (struct sockaddr*)NULL, NULL);   //for TCP
            if (connfd >= 0)
                printf("Accept TCP connection....\n");
        }
            
        // read data from client
        fd.fd = connfd;
        fd.events = POLLIN;
        res = poll(&fd, 1, timeout*1000); 
        if (res >0) {
            pi_recvlen = recv(connfd, &pi_buf, MAXBUF, 0);
            if (pi_recvlen > 0) {
                printf("1. Received a COMMAND (%d bytes)\n", pi_recvlen);
               		
                pi_p = (char *) (&pi_buf); 
  		        pi_cmdPtr = (cmd_struct_t *)pi_p;
  		        pi_rx_reply = *pi_cmdPtr;
		
                node_id = pi_cmdPtr->len;
                old_seq = pi_cmdPtr->seq;
                rx_reply = *pi_cmdPtr;

                gettimeofday(&t0, 0);
                if (is_cmd_of_gw(*pi_cmdPtr)==true) {
                    printf(" - Command Analysis: received GW command, cmdID=0x%02X, seq=0x%04X \n", pi_cmdPtr->cmd, pi_cmdPtr->seq);
                    process_gw_cmd(*pi_cmdPtr, node_id);
                    gettimeofday(&t1, 0);
                    printf(" - GW-Cmd execution delay %.2f (ms)\n", timedifference_msec(t0,t1));
                    update_sql_db();
                }
                else {  // not command for GW: send to node and wait for a reply
                    printf(" - Command Analysis: received LED command, cmdID=0x%02X, seq=0x%04X \n", pi_cmdPtr->cmd, pi_cmdPtr->seq);
                    tx_cmd = *pi_cmdPtr;
                    res = ip6_send_cmd(node_id, SLS_NORMAL_PORT, NUM_RETRANSMISSIONS, false);
                    printf(" - GW-Cmd execution delay %.2f (ms)\n", node_db_list[node_id].delay);
            
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
                        node_db_list[node_id].rdr = 100*node_db_list[node_id].num_rep/node_db_list[node_id].num_req;
                    }         
                    update_sql_row(node_id);
                }
                // send REPLY to sender NODE
                // update corresponding sequence
                rx_reply.seq = old_seq;
                //sprintf(pi_buf, "ACK %d", pi_msgcnt++);
                printf("4. Sending RESPONE to user ACK-%d, seq = 0x%04X \n", pi_msgcnt++, old_seq);
                write(connfd, &rx_reply, sizeof(rx_reply)); //TCP
                //if (sendto(pi_fd, &rx_reply, sizeof(rx_reply), 0, (struct sockaddr *)&pi_remaddr, pi_addrlen) < 0)
                //    perror("sendto");

                show_local_db();
                printf("\nIV. GATEWAY WAITING on PORT %d for COMMANDS, and PORT %d for ASYNC MSG\n", SERVICE_PORT, SLS_EMERGENCY_PORT);
            }
        }
        
        close(connfd);
        sleep(1);   
	}
    return 0;
}



//-------------------------------------------------------------------------------------------
/* calculate CRC
    encrypt 64 byte of cmd
*/
void make_packet_for_node(cmd_struct_t *cmd, uint16_t nodeid, bool encryption_en){
    uint8_t i;
    unsigned char key_arr[16];
    
    convert_str2array(node_db_list[nodeid].app_key, key_arr, 16);

    tx_cmd.crc = 0;
    gen_crc_for_cmd(cmd);
    if (encryption_en==true) {
        encrypt_payload(cmd, key_arr);
    }    
    printf(" - AES process ... %d\n", encryption_en);
    printf(" - Make Tx packet for node %d...done\n", nodeid);
}

//-------------------------------------------------------------------------------------------
bool check_packet_for_node(cmd_struct_t *cmd, uint16_t nodeid, bool encryption_en){
    uint8_t i;
    unsigned char key_arr[16];

    convert_str2array(node_db_list[nodeid].app_key, key_arr, 16);

    printf(" - Check packet for Node %d key = ", nodeid);
    for (i = 0; i<16; i++) {
        printf("0x%02X ",key_arr[i]);
    }
    printf("\n");

    if (encryption_en==true) {
        decrypt_payload(cmd, key_arr);
    }

    printf(" - AES process ... %d\n", encryption_en);
    printf(" - Check RX packet for node %d... done\n", nodeid);
    return check_crc_for_cmd(cmd);
}

//-------------------------------------------------------------------------------------------
int ip6_send_cmd(int nodeid, int port, int retrans, bool encryption_en) {
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
    //printf("NUMBER OF RETRIALS = %d \n", retrans);        
    while (num_of_retrans < retrans) {
        gettimeofday(&t0, 0);

        /* encrypt payload here */
        make_packet_for_node(&tx_cmd, nodeid, encryption_en);

        //for testing CRC:
        //tx_cmd.seq = random();

        status = sendto(sock, &tx_cmd, sizeof(tx_cmd), 0,(struct sockaddr *)psinfo->ai_addr, sin6len);
        if (status > 0)     {
            printf("\n2. Forward REQUEST (%d bytes) to [%s]:%s, retry=%d  ....done\n",status, dst_ipv6addr,str_port, num_of_retrans);        
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
            strftime(str_time,80,"%x-%H:%M:%S", timeinfo);
            strcpy (node_db_list[nodeid].last_seen, str_time);
            
            /* get the execution delay */
            gettimeofday(&t1, 0);
            node_db_list[nodeid].delay = timedifference_msec(t0, t1);

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
                check_packet_for_node(&rx_reply, nodeid, encryption_en);    

                strcpy(node_db_list[nodeid].connected,"Y");
                node_db_list[nodeid].last_err_code = rx_reply.err_code;
                node_db_list[nodeid].num_of_retrans = num_of_retrans;

                time(&rawtime );
                timeinfo = localtime ( &rawtime );
                strftime(str_time,80,"%x-%H:%M:%S", timeinfo);
                strcpy (node_db_list[nodeid].last_seen, str_time);

                /*get the execution delay */    
                gettimeofday(&t1, 0);
                node_db_list[nodeid].delay  = timedifference_msec(t0, t1);

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