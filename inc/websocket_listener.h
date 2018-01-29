/**
 * @file url_listener.h
 * @author ibrahim
 * @date Mar 9, 2015
 * @brief Header of surl_listener.c
 * @see http://www.writesys.com.br
 */

#ifndef WEBSOCKET_LISTENER_H_
#define WEBSOCKET_LISTENER_H_

#define JSON_STRING_SIZE 			8000
#define STR_WEBSOCK_SIZE 			(JSON_STRING_SIZE+16)
#define WEBSOCKET_LISTENER_PORT 	9000

#define ERROR		-1
#define TRUE		0
#define FALSE		1

int websocket_listener_deinit();

#endif /* WEBSOCKET_LISTENER_H_ */
