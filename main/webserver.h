#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__
#include <esp_http_server.h>

//Webserver Functions
httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif
