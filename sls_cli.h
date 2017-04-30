/*
|-------------------------------------------------------------------|
| HCMC University of Technology                                     |
| Telecommunications Departments                                    |
| Command Line Interface for Smart Lighting System (SLS)            |
| Version: 1.0                                                      |
| Author: sonvq@hcmut.edu.vn                                        |
| Date: 01/2017                                                     |
|-------------------------------------------------------------------|
*/

#ifndef SLS_CLI_H_
#define SLS_CLI_H_

#define USING_SQL_SERVER_ENABLE	1
#if (USING_SQL_SERVER_ENABLE)
#define USING_SQL_SERVER
#endif


#define SLS_LED_HELLO			"led_hello"
#define SLS_LED_ON				"led_on"
#define SLS_LED_OFF				"led_off"
#define SLS_LED_DIM				"led_dim"
#define SLS_LED_REBOOT			"led_reboot"
#define SLS_GET_LED_STATUS		"get_led_status"

#define SLS_LED_TIMER_START		"node_timer_on"
#define SLS_LED_TIMER_STOP		"node_timer_off" 

#define SLS_GET_NW_STATUS		"get_nw_status"
#define SLS_GET_GW_STATUS		"get_gw_status"
#define SLS_GW_HELLO 			"gw_hello"
#define SLS_REPAIR_ROOT			"nw_repair_root"

#define AUTO_SET_APP_KEY		1
#define SLS_SET_APP_KEY			"set_app_key"
#define SLS_GET_APP_KEY			"get_app_key"
#define SLS_APP_KEY_128 		"CAFEBEAFDEADFEEE0123456789ABCDEF"


struct node_db_struct_t {
	int 		index;
	int			id;			/*0001xxxx xxxxxxxx */
	char    	ipv6_addr[50];						
	char		connected[1];
	int			num_req;
	int			num_rep;
	int 		num_timeout;
	int			last_cmd;
	int 		last_err_code;
	int 		num_of_retrans;
	char		last_seen[50];
	char		app_key[32];
	int 		channel_id;
	int 		rssi;
	int 		lqi;
	int 		pan_id;
	int		    tx_power;
	int 		num_emergency_msg;
	uint8_t 	last_emergency_msg[MAX_CMD_DATA_LEN];
};

typedef struct node_db_struct_t		node_db_struct_t;

#endif /* SLS_CLI_H_ */