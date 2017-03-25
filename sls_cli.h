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
#define SLS_REPAIR_ROOT			"nw_repair_root"

#define SLS_SET_APP_KEY			"set_app_key"
#define SLS_GET_APP_KEY			"get_app_key"
#define SLS_APP_KEY_128 		"CAFEBEAFDEADFEEE0123456789ABCDEF"

#define SLS_GW_HELLO 			"gw_hello"
#define SLS_GET_GW_STATUS		"get_gw_status"



#endif /* SLS_CLI_H_ */