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

#define SLS_SET_APP_KEY			"set_app_key"
#define SLS_GET_APP_KEY			"get_app_key"
#define SLS_APP_KEY_128 		"CAFEBEAFDEADFEEE0123456789ABCDEF"


struct node_db_struct_t {
	int 		index;
	int			id;			/*0001xxxx xxxxxxxx */
	char    	ipv6_addr[50];						
	char		connected[1];
	int			rx_cmd;
	int			tx_rep;
	int 		num_timeout;
	int			last_cmd;
};

typedef struct node_db_struct_t		node_db_struct_t;

#endif /* SLS_CLI_H_ */