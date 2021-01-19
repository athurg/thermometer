#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__
#include <esp_http_server.h>

//Webserver Functions
esp_err_t start_webserver(void);
void stop_webserver(void);

#endif
