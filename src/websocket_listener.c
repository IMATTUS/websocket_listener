/**
 * @file websocket_listener.c
 * @author ibrahim
 * @date Jan 25, 2018
 * @brief Websocket listener functions, receives a request through an websocket connection, interprets and executes it
 * @see http://www.writesys.com.br
 */

#include "websocket_listener.h"
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <semaphore.h>
#include <string.h>
#include "cJSON.h"
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>


/** @brief pThread pointer */
static pthread_t websocket_listener_id, json_thread_id;

/** @brief Semaphore for json creation*/
static sem_t *json_sem;

static struct lws_context *context;
static struct lws_context_creation_info info;

/** @brief string used to store the json to be sent to nodejs */
static char json_string[JSON_STRING_SIZE];

/** @brief Prototype transport call function */
typedef void cmd_function_t(char *, int *, cJSON *);
/**
 * @brief command available table
 */
typedef struct {
	char *name; /** User printable name */
	cmd_function_t *func; /** Function to call */
} command_t;




static char command_1[16] = {'A'};

static char command_2[16] = {'B'};
static int command_3 = 9;



void * json_websocket_generator();

static void cli_websocket_create_response(char * func, char * value, int code, cJSON *websocketsubitem);

static int websocket_command_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len);

static int websocket_getsettings_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len);

static int websocket_getallsettings_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len);

static void cli_websocket_execute_command(char *line, cJSON *websocketsubitem);
static command_t *cli_websocket_find_command(char * name);

static void cli_websocket_command_1_set(char * arg, int * value, cJSON *websocketsubitem);
static void cli_websocket_command_1_get(char * arg, int * value);

static void cli_websocket_command_2_set(char * arg, int * value, cJSON *websocketsubitem);
static void cli_websocket_command_2_get(char * arg, int * value);

static void cli_websocket_command_3_set(char * arg, int * value, cJSON *websocketsubitem);
static void cli_websocket_command_3_get(char * arg, int * value);

void * websocket_listener(void* arg);
void parse_cJSON_command(cJSON *item, cJSON *websocketsubitem);
void parse_cJSON_settings(cJSON *item, cJSON *all_settings, cJSON *return_json);

/** @brief Structure with all commands. The table must be in alphabetical order */
static command_t commands[] = {
		//{ <String for the command>, function to call}

		{ "command_1", cli_websocket_command_1_set },
		{ "command_2", cli_websocket_command_2_set },
		{ "command_3", cli_websocket_command_3_set },

		{ (char *) NULL, (cmd_function_t *) NULL } };

/**
 * @brief  Check if current command exists in commands table
 * @param  name Command string to be checked
 * @return Command pointer if success or NULL if command not found
 **/
static command_t *cli_websocket_find_command(char *name)
{
	register int i;
	size_t namelen;

	if ((name == NULL) || (*name == '\0'))
		return ((command_t *) NULL);

	namelen = strlen(name);

	for (i = 0; commands[i].name; i++) {
		if (strncmp(name, commands[i].name, namelen) == 0) {
			if (strlen(commands[i].name) == namelen) {
				return (&commands[i]);
			}
			/* make sure the match is unique */
			if ((commands[i + 1].name) && (strncmp(name, commands[i + 1].name, namelen) == 0)) {
				return ((command_t *) NULL);
			} else {
				return (&commands[i]);
			}
		}
	}
	return ((command_t *) NULL);
}

/**
 * @brief  Processes the CLI input to call a function.
 * @details On success execute the right command, an error message will be shown in case of command not found
 * @return void
 **/
static void cli_websocket_execute_command(char *line, cJSON *websocketsubitem)
{
	register int i;
	command_t *command;
	char *word, *arg;

	/* Isolate the command word. */
	i = 0;
	while (line[i] && (((line[i]) == ' ') || ((line[i]) == '\t')))
		i++;

	word = line + i;

	while (line[i] && !(((line[i]) == ' ') || ((line[i]) == '\t')))
		i++;

	if (line[i])
		line[i++] = '\0';
	command = cli_websocket_find_command(word);

	/* Get argument to command, if any. */
	while ((((line[i]) == ' ') || ((line[i]) == '\t')))
		i++;

	arg = line + i;

	if (!command) {
		cli_websocket_create_response(word, arg, ERROR, websocketsubitem);
		return;
	}
	/* invoke the command function. */
	(*command->func)(arg, 0, websocketsubitem);

	word = NULL;
	arg = NULL;
	command = NULL;

}

/**
 * @brief Creates a string in json format to send to the upper level application
 **/
void * json_websocket_generator() 
{

	cJSON *root, *fld;
	char *out;

	root = cJSON_CreateArray();
	cJSON_AddItemToArray(root, fld = cJSON_CreateObject());

	char * str_command_1 = malloc(sizeof(char) * 16);
	memset(str_command_1, 0, sizeof(char) * 16);
	cli_websocket_command_1_get(str_command_1, NULL);
	cJSON_AddStringToObject(fld, "command_1", str_command_1);
	free(str_command_1);

	char * str_command_2 = malloc(sizeof(char) * 16);
	memset(str_command_2, 0, sizeof(char) * 16);
	cli_websocket_command_2_get(str_command_2, NULL);
	cJSON_AddStringToObject(fld, "command_2", str_command_2);
	free(str_command_2);

	int int_command_3 = 0;
	cli_websocket_command_3_get(NULL, &int_command_3);
	cJSON_AddNumberToObject(fld, "command_3",
			int_command_3);

	out = cJSON_Print(root);

	if (strlen(out) > 0) {
		sem_wait(json_sem);
		strncpy(json_string, out, sizeof(char) * JSON_STRING_SIZE);
		sem_post(json_sem);
	}

	cJSON_Delete(root);

	free(out);
	out = NULL;

	return (void *) 0;
}

void * json_websocket_creator(void* arg)
{
	while (1) {
		sleep(1);
		json_websocket_generator();
	}
	return 0;
}
/**
 * @brief  Initialize websocket_listener functions. Create all necessary threads
 * @return 0 Success
 * @return The error number in case of error
 */
int main(int argc, char **argv) {
	sem_unlink("sem_json");
	json_sem = sem_open("sem_json", O_CREAT, 0664, 1);

	int r;
	r = pthread_create(&websocket_listener_id, NULL, &websocket_listener, NULL);
	if (r != 0)
		printf( "Fail creating WEBSOCKET listener thread. ERR=%u.", r);
	r = pthread_create(&json_thread_id, NULL, &json_websocket_creator, NULL);
	if (r != 0)
		printf( "Fail creating Json creatorthread. ERR=%u.", r);

	char c = getc(stdin);

	while (strcmp(&c, "q") != 0){

		sleep(5);

	}
	return r;
}

/**
 * @brief  Deinitialize websocket_listener functions. Kill all threads
 * @return 0 Success
 */
int websocket_listener_deinit()
{
	int r;
	r = pthread_cancel(json_thread_id);
	if (r != 0) {
		printf( "Fail canceling json creator thread. ERR=%u.", r);
	}
	r = pthread_cancel(websocket_listener_id);
	if (r != 0) {
		printf( "Fail canceling thread. ERR=%u.", r);
	}
	if (context != NULL) {
		/* Close socket */
		lws_cancel_service(context);

		lws_context_destroy(context);
	} else {
		printf( "\n context NULL");
	}
	sem_close(json_sem);
	sem_unlink("sem_json");
	sem_destroy(json_sem);
	printf( "Function deinitialized.");
	return (EXIT_SUCCESS);
}

/**
 * @brief  Main loop thread
 * @details Opens a socket and keeps listening indefinitely
 * @details calls the function to execute the command received through the socket
 * @return void
 */
static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{

	// set SO_REUSEADDR on a socket to true (1):
	int optval = 1;
	setsockopt(lws_get_socket_fd(wsi), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	return 0;
}

void parse_cJSON_find_settings(cJSON *all_items, cJSON *item_find, cJSON *return_json)
{

	while (all_items) {

		if (all_items->child) {
			parse_cJSON_find_settings(all_items->child, item_find, return_json);
		}
		if (all_items->string != NULL) {
			if (strcmp(all_items->string, item_find->string) == 0) {
				switch (all_items->type) {
					case cJSON_Number:
						cJSON_AddNumberToObject(return_json, all_items->string, all_items->valueint);

						break;
					case cJSON_String:
						cJSON_AddStringToObject(return_json, all_items->string, all_items->valuestring);

						break;

					default:
						break;
				}
			}

		}
		all_items = all_items->next;

	}
}

void parse_cJSON_settings(cJSON *item, cJSON *all_settings, cJSON *return_json)
{

	while (item) {
		if (item->child) {
			parse_cJSON_settings(item->child, all_settings, return_json);
		}
		if (item->string != NULL) {
			switch (item->type) {
				case cJSON_String:
					parse_cJSON_find_settings(all_settings, item, return_json);
					break;

				default:
					break;
			}

		}
		item = item->next;

	}
}

void parse_cJSON_command(cJSON *item, cJSON *websocketsubitem)
{
	while (item) {
		if (item->child) {
			parse_cJSON_command(item->child, websocketsubitem);
		}
		if (item->string != NULL) {
			char cmd[200] = { 0 };
			switch (item->type) {
				case cJSON_String:
					snprintf(cmd, sizeof(char) * 200, "%s %s", item->string, item->valuestring);

					cli_websocket_execute_command(cmd, websocketsubitem);
					break;

				default:
					break;
			}
		}
		item = item->next;
	}
	return;
}

static int websocket_command_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			printf( "Websocket connection established");

			break;
		case LWS_CALLBACK_RECEIVE: {
			int buf_size_total = LWS_SEND_BUFFER_PRE_PADDING + JSON_STRING_SIZE + LWS_SEND_BUFFER_POST_PADDING;

			int padding_size = LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING;
			char websocketResponse[STR_WEBSOCK_SIZE] = { 0 };
			unsigned char buf[STR_WEBSOCK_SIZE] = { 0 };
			snprintf((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING], sizeof(char) * STR_WEBSOCK_SIZE - LWS_SEND_BUFFER_PRE_PADDING - LWS_SEND_BUFFER_POST_PADDING, "%s ", (char *) in);

			cJSON *root = cJSON_Parse((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING]);

			cJSON * websocketResponseRoot = cJSON_CreateArray();

			cJSON * websocketsubitem;

			cJSON_AddItemToArray(websocketResponseRoot, websocketsubitem = cJSON_CreateObject());
			parse_cJSON_command(root, websocketsubitem);

			if (websocketResponse != NULL) {
				snprintf(&websocketResponse[LWS_SEND_BUFFER_PRE_PADDING], sizeof(char) * STR_WEBSOCK_SIZE - LWS_SEND_BUFFER_PRE_PADDING - LWS_SEND_BUFFER_POST_PADDING, "%s", cJSON_Print(websocketResponseRoot));
			}

			lws_write(wsi, (unsigned char *) &websocketResponse[LWS_SEND_BUFFER_PRE_PADDING],
			strlen(&websocketResponse[LWS_SEND_BUFFER_PRE_PADDING]), LWS_WRITE_TEXT);

			memset(&websocketResponse[LWS_SEND_BUFFER_PRE_PADDING], '\0', buf_size_total - padding_size);

			// release memory back into the wild
			cJSON_Delete(websocketResponseRoot);
			cJSON_Delete(root);
			websocketResponseRoot = NULL;
			root = NULL;

			lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
			lws_cancel_service_pt(wsi);
			return -1;   // return -1 in order to close the connection
			break;
		}
		default:
			break;
	}

	return 0;
}

/**
 * @brief Sends via HTTP GET of the config information on JSON format
 */
static int websocket_getallsettings_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			printf( "Websocket connection established");
			break;
		case LWS_CALLBACK_RECEIVE: {

			sem_wait(json_sem);
			lws_write(wsi, (unsigned char *) json_string, strlen(json_string) * sizeof(char), LWS_WRITE_TEXT);
			sem_post(json_sem);
			lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
			lws_cancel_service_pt(wsi);
			return -1;   // return -1 in order to close the connection
			break;
		}
		case LWS_CALLBACK_CLOSED:
			break;
		default:
			break;
	}

	return 0;

}

/**
 * @brief Sends via HTTP GET of the config information on JSON format
 */
static int websocket_getsettings_interpreter(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
		size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			printf( "Websocket connection established");

			break;
		case LWS_CALLBACK_RECEIVE: {
			unsigned char buf[STR_WEBSOCK_SIZE] = { 0 };
			snprintf((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING], sizeof(char) * STR_WEBSOCK_SIZE - LWS_SEND_BUFFER_PRE_PADDING - LWS_SEND_BUFFER_POST_PADDING, "%s", (char *) in);

			sem_wait(json_sem);
			cJSON *all_settings_root = cJSON_Parse(json_string);
			sem_post(json_sem);

			cJSON *find_settings = cJSON_Parse((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING]);

			cJSON *return_root, *return_json;
			return_root = cJSON_CreateArray();

			cJSON_AddItemToArray(return_root, return_json = cJSON_CreateObject());

			parse_cJSON_settings(find_settings, all_settings_root, return_json);

			snprintf((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING], sizeof(char) * STR_WEBSOCK_SIZE - LWS_SEND_BUFFER_PRE_PADDING - LWS_SEND_BUFFER_POST_PADDING, "%s", cJSON_Print(return_root));
			lws_write(wsi, &buf[LWS_SEND_BUFFER_PRE_PADDING], strlen((char *) &buf[LWS_SEND_BUFFER_PRE_PADDING]),
					LWS_WRITE_TEXT);
			cJSON_Delete(find_settings);
			cJSON_Delete(return_root);
			cJSON_Delete(all_settings_root);

			lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
			lws_cancel_service_pt(wsi);
			return -1;   // return -1 in order to close the connection
			break;
		}
		default:
			break;
	}
	return 0;
}

static struct lws_protocols protocols[] = {
/* first protocol must always be HTTP handler */
{ "http-only",   // name
		callback_http,   // callback
		0              // per_session_data_size
		},

		{ "param_get_all",   // protocol name - very important!
				websocket_getallsettings_interpreter,   // callback
				0                          // we don't use any per session data
		}, { "param_get_obj",   // protocol name - very important!
				websocket_getsettings_interpreter,   // callback
				0                          // we don't use any per session data
		}, { "param_set",   // protocol name - very important!
				websocket_command_interpreter,   // callback
				0                          // we don't use any per session data
		}, {
		NULL, NULL, 0 /* End of list */
		} };

void * websocket_listener(void* arg)
{
	const char *interface = NULL;
	// we're not using ssl
	const char *cert_path = NULL;
	const char *key_path = NULL;
	// no special options
	int opts = 0;

	memset(&info, '\0', sizeof(info));
	struct lws_extension *libwebsocket_internal_extensions = NULL;
	info.port = WEBSOCKET_LISTENER_PORT;
	info.iface = interface;
	info.protocols = protocols;
	info.extensions = libwebsocket_internal_extensions;
	info.ssl_cert_filepath = cert_path;
	info.ssl_private_key_filepath = key_path;
	info.options = opts;
	info.ka_interval = 1;
	info.ka_probes = 1;
	info.ka_time = 1;
	info.fd_limit_per_thread = 10;
	info.timeout_secs = 1;
	lws_set_log_level(LLL_ERR, NULL);
	context = lws_create_context(&info);

	if (context == NULL) {
		printf( "\nlibwebsocket init failed\n");
	}

	while (1) {
		lws_service(context, 50);

	}
	if (context != NULL) {
		/* Close socket */
		lws_cancel_service(context);
		lws_context_destroy(context);

	}
	return (void *) 0;
}

static void cli_websocket_command_1_set(char * arg, int * value, cJSON *websocketsubitem)
{

	if (*arg) {
		char * addr = malloc(sizeof(char) * 16);
		cli_websocket_command_1_get(addr,NULL);
		if (strcmp(addr, arg) != 0) {


			strncpy(command_1, arg,  sizeof(char) * 16);

			cli_websocket_create_response(
			"command_1", arg, 0, websocketsubitem);

		} else {
			cli_websocket_create_response(
					"command_1", arg, 1, websocketsubitem);
		}
		free(addr);
	} else {
		cli_websocket_create_response("command_1", arg,
		-1, websocketsubitem);
	}
	return;

}

static void cli_websocket_command_1_get(char * arg, int * value)
{
	strncpy(arg, command_1, sizeof(char) * 16);
}


static void cli_websocket_command_2_set(char * arg, int * value, cJSON *websocketsubitem)
{

	if (*arg) {
		char * addr = malloc(sizeof(char) * 16);
		cli_websocket_command_2_get(addr,NULL);

		if (strcmp(addr, arg) != 0) {


			strncpy(command_2, arg,  sizeof(char) * 16);

			cli_websocket_create_response(
			"command_2", arg, 0, websocketsubitem);

		} else {
			cli_websocket_create_response(
					"command_2", arg, 1, websocketsubitem);
		}
		free(addr);
	} else {
		cli_websocket_create_response("command_2", arg,
		-1, websocketsubitem);
	}
	return;

}

static void cli_websocket_command_2_get(char * arg, int * value)
{
	strncpy(arg, command_2, sizeof(char) * 16);
}




static void cli_websocket_command_3_set(char * arg, int * value, cJSON *websocketsubitem)
{

	if (*arg) {

		int ret = 0;
		cli_websocket_command_3_get(NULL,&ret);
		if (ret != atoi(arg)) {
			command_3 = atoi(arg);
			cli_websocket_create_response(
			"command_3", arg, 0, websocketsubitem);

		} else {
			cli_websocket_create_response(
					"command_3", arg, 1, websocketsubitem);
		}
	} else {
		cli_websocket_create_response("command_3", arg,
		-1, websocketsubitem);
	}
	return;

}

static void cli_websocket_command_3_get(char * arg, int * value)
{
	*value = command_3;
}

static void cli_websocket_create_response(char * func, char * value, int code, cJSON *websocketsubitem)
{
	switch (code) {
		case TRUE:
			cJSON_AddNumberToObject(websocketsubitem, func, code);
			break;
		case FALSE:
			cJSON_AddNumberToObject(websocketsubitem, func, code);
			break;
		case ERROR:
			cJSON_AddNumberToObject(websocketsubitem, func, code);
			break;
		default:
			cJSON_AddNumberToObject(websocketsubitem, func, code);
			break;
	}
	return;
}

