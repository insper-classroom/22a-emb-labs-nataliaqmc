#include "asf.h"
#include "main.h"
#include <string.h>
#include "bsp/include/nm_bsp.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "util.h"



#define LED_PIO PIOC
#define LED_PIO_ID ID_PIOC
#define LED_PIO_IDX 8
#define LED_PIO_IDX_MASK (1 << LED_PIO_IDX)

#define BUT_PIO			     PIOA
#define BUT_PIO_ID           ID_PIOA
#define BUT_PIO_IDX		     11
#define BUT_PIO_IDX_MASK     (1u << BUT_PIO_IDX)
/************************************************************************/
/* WIFI                                                                 */
/************************************************************************/

/** IP address of host. */
uint32_t gu32HostIp = 0;

/** TCP client socket handlers. */
static SOCKET tcp_client_socket = -1;

/** Receive buffer definition. */
static uint8_t g_receivedBuffer[MAIN_WIFI_M2M_BUFFER_SIZE] = {0};
static uint8_t g_sendBuffer[MAIN_WIFI_M2M_BUFFER_SIZE] = {0};

/** Wi-Fi status variable. */
static bool gbConnectedWifi = false;

/** Get host IP status variable. */
/** Wi-Fi connection state */
static uint8_t wifi_connected;

/** Instance of HTTP client module. */
static bool gbHostIpByName = false;

/** TCP Connection status variable. */
static bool gbTcpConnection = false;
static bool gbTcpConnected = false;

/** Server host name. */
static char server_host_name[] = MAIN_SERVER_NAME;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_WIFI_STACK_SIZE      (6*4096/sizeof(portSTACK_TYPE))
#define TASK_WIFI_PRIORITY        (1)
#define TASK_PROCESS_STACK_SIZE   (4*4096/sizeof(portSTACK_TYPE))
#define TASK_PROCESS_PRIORITY     (0)

SemaphoreHandle_t xSemaphore;
SemaphoreHandle_t xSemaphoreBotao;
QueueHandle_t xQueueMsg;

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

TaskHandle_t xHandleWifi = NULL;

/************************************************************************/
/* HOOKs                                                                */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName){
  printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
  for (;;) {  }
}

extern void vApplicationIdleHook(void){}

extern void vApplicationTickHook(void){}

extern void vApplicationMallocFailedHook(void){
  configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/
void but_callback(void){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(xSemaphoreBotao, &xHigherPriorityTaskWoken);
	
}


void configure_pio_input(Pio *pio, const pio_type_t ul_type, const uint32_t ul_mask, const uint32_t ul_attribute, uint32_t ul_id){
	pmc_enable_periph_clk(ul_id);
	pio_configure(pio, ul_type, ul_mask, ul_attribute);
	pio_set_debounce_filter(pio, ul_mask, 60);
}

void configure_interruption(Pio *pio, uint32_t ul_id, const uint32_t ul_mask,  uint32_t ul_attr, void (*p_handler) (uint32_t, uint32_t), uint32_t priority){
	pio_handler_set(pio, ul_id, ul_mask , ul_attr, p_handler);
	pio_enable_interrupt(pio, ul_mask);
	pio_get_interrupt_status(pio);
	NVIC_EnableIRQ(ul_id);
	NVIC_SetPriority(ul_id, priority);
}
/************************************************************************/
/* callbacks                                                            */
/************************************************************************/

/**
* \brief Callback function of IP address.
*
* \param[in] hostName Domain name.
* \param[in] hostIp Server IP.
*
* \return None.
*/
static void resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
  gu32HostIp = hostIp;
  gbHostIpByName = true;
  printf("resolve_cb: %s IP address is %d.%d.%d.%d\r\n\r\n", hostName,
  (int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1),
  (int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
}

/**
* \brief Callback function of TCP client socket.
*
* \param[in] sock socket handler.
* \param[in] u8Msg Type of Socket notification
* \param[in] pvMsg A structure contains notification informations.
*
* \return None.
*/
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
  /* Check for socket event on TCP socket. */
  if (sock == tcp_client_socket) {

    switch (u8Msg) {
      case SOCKET_MSG_CONNECT:
      {
        printf("socket_msg_connect\n");
        if (gbTcpConnection) {
          tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
          if (pstrConnect && pstrConnect->s8Error >= SOCK_ERR_NO_ERROR) {
            printf("socket_cb: connect ok \n");
            gbTcpConnected = true;
            } else {
            printf("socket_cb: connect error!\r\n");
            gbTcpConnection = false;
            gbTcpConnected = false;
            close(tcp_client_socket);
            tcp_client_socket = -1;
          }
        }
      }
      break;

      case SOCKET_MSG_RECV:
      {
        char *pcIndxPtr;
        char *pcEndPtr;

        tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
        if (pstrRecv && pstrRecv->s16BufferSize > 0) {
          xQueueSend(xQueueMsg, &pstrRecv, 10);
          xSemaphoreGive( xSemaphore );
          }  else {
          //printf("socket_cb: recv error!\r\n");
          close(tcp_client_socket);
          tcp_client_socket = -1;
        }
      }
      break;

      default:
      break;
    }
  }
}

/**
* \brief Callback to get the Wi-Fi status update.
*
* \param[in] u8MsgType Type of Wi-Fi notification.
* \param[in] pvMsg A pointer to a buffer containing the notification parameters.
*
* \return None.
*/
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
  switch (u8MsgType) {
    case M2M_WIFI_RESP_CON_STATE_CHANGED:
    {
      tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
      if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
        printf("wifi_cb: M2M_WIFI_CONNECTED\r\n");
        m2m_wifi_request_dhcp_client();
        } else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
        printf("wifi_cb: M2M_WIFI_DISCONNECTED\r\n");
        gbConnectedWifi = false;
        wifi_connected = 0;
      }

      break;
    }

    case M2M_WIFI_REQ_DHCP_CONF:
    {
      uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
      printf("wifi_cb: IP address is %u.%u.%u.%u\r\n",
      pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
      wifi_connected = M2M_WIFI_CONNECTED;
      break;
    }

    case M2M_WIFI_RESP_GET_SYS_TIME:
    /*Initial first callback will be provided by the WINC itself on the first communication with NTP */
    {
      tstrSystemTime *strSysTime_now = (tstrSystemTime *)pvMsg;

      /* Print the hour, minute and second.
      * GMT is the time at Greenwich Meridian.
      */
      printf("socket_cb: Year: %d, Month: %d, The GMT time is %u:%02u:%02u\r\n",
      strSysTime_now->u16Year,
      strSysTime_now->u8Month,
      strSysTime_now->u8Hour,    /* hour (86400 equals secs per day) */
      strSysTime_now->u8Minute,  /* minute (3600 equals secs per minute) */
      strSysTime_now->u8Second); /* second */
      break;
    }

    default:
    {
      break;
    }
  }
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/
void get_funcao(uint8_t *g_sendBuffer, char *prefix){
	sprintf(g_sendBuffer, "GET %s HTTP/1.1\r\n Accept: */*\r\n\r\n", prefix);
}

void send_post_request(char *endpoint, char status){
	char *contentBuffer[6];
	sprintf(contentBuffer, "LED=%d", status);
	sprintf((char *)g_sendBuffer, "POST %s HTTP/1.0\nContent-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", endpoint, strlen(contentBuffer), contentBuffer);
	printf("ESSE ? O G_SEND_BUFFER %s", g_sendBuffer);
	send(tcp_client_socket, g_sendBuffer, strlen((char *)g_sendBuffer), 0);
}
static void task_process(void *pvParameters) {
	int request_status = 0;
	int status = 0;

	printf("task process created \n");
	vTaskDelay(1000);

	uint msg_counter = 0;
	tstrSocketRecvMsg *p_recvMsg;

	enum states {
		WAIT = 0,
		GET,
		POST,
		ACK,
		ACK_POST,
		MSG,
		TIMEOUT,
		DONE,
	};

  enum states state = WAIT;

  while(1){

    switch(state){
      case WAIT:
      // aguarda task_wifi conectar no wifi e socket estar pronto
      printf("STATE: WAIT \n");
      while(gbTcpConnection == false && tcp_client_socket >= 0){
        vTaskDelay(10);
      }
	  if (xSemaphoreTake(xSemaphoreBotao, 100)) {
		  state = POST;
		  } else {
		  state = GET;
	  }
      break;

      case GET:
      printf("STATE: GET \n");
      sprintf((char *)g_sendBuffer, MAIN_PREFIX_BUFFER);
      send(tcp_client_socket, g_sendBuffer, strlen((char *)g_sendBuffer), 0);
      state = ACK;
      break;
	  
	   case POST:
	   printf("STATE: POST \n");
	   request_status = status == 1 ? 0 : 1;
	   printf("ESSE ? O REQUEST STATUS: %d", request_status);
	   send_post_request("/status", request_status);
	   state = ACK_POST;
	   break;

      case ACK:
      printf("STATE: ACK \n");
      memset(g_receivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
      recv(tcp_client_socket, &g_receivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);

      if(xQueueReceive(xQueueMsg, &p_recvMsg, 5000) == pdTRUE){
        printf(STRING_LINE);
        printf(p_recvMsg->pu8Buffer);
        printf(STRING_EOL);  printf(STRING_LINE);
        state = MSG;
      }
      else {
        state = TIMEOUT;
      };
      break;
	  
	  case ACK_POST:
	  printf("STATE: ACK from POST \n");
	  memset(g_receivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
	  recv(tcp_client_socket, &g_receivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);
	  
	  if(xQueueReceive(xQueueMsg, &p_recvMsg, 5000) == pdTRUE){
		  printf(STRING_LINE);
		  printf(p_recvMsg->pu8Buffer);
		  printf(STRING_EOL);  printf(STRING_LINE);
		  state = DONE;
	  }
	  else {
		  state = TIMEOUT;
	  };
	  break;

      case MSG:
      printf("STATE: MSG \n");
      memset(g_receivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
      recv(tcp_client_socket, &g_receivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);

      if(xQueueReceive(xQueueMsg, &p_recvMsg, 5000) == pdTRUE){
        printf(STRING_LINE);
        printf(p_recvMsg->pu8Buffer);
		strstr(p_recvMsg->pu8Buffer, "\"led\":");
		printf("%s", strstr(p_recvMsg->pu8Buffer, "\"led\":"));
		char *status_led = strstr(p_recvMsg->pu8Buffer, "\"led\":")+8;
		printf("STATUS: %s", status_led);
		status = atoi(status_led);
		printf("STATUS CERTO: %d", status);
		
		if (status == 1){
			pio_clear(LED_PIO, LED_PIO_IDX_MASK);
			} else {
			pio_set(LED_PIO, LED_PIO_IDX_MASK);
		}
        printf(STRING_EOL);  printf(STRING_LINE);
        state = DONE;
      }
      else {
        state = TIMEOUT;
      };
      break;

      case DONE:
      printf("STATE: DONE \n");

      state = WAIT;
      break;

      case TIMEOUT:
      state = WAIT;
      break;

      default: state = WAIT;
    }
  }
}

static void task_wifi(void *pvParameters) {
  tstrWifiInitParam param;
  struct sockaddr_in addr_in;

  xSemaphore = xSemaphoreCreateCounting(20,0);
  xQueueMsg = xQueueCreate(10, sizeof(tstrSocketRecvMsg));

  /* Initialize the BSP. */
  nm_bsp_init();

  /* Initialize Wi-Fi parameters structure. */
  memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

  /* Initialize Wi-Fi driver with data and status callbacks. */
  param.pfAppWifiCb = wifi_cb;
  int8_t ret = m2m_wifi_init(&param);
  if (M2M_SUCCESS != ret) {
    printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
    while (1) { }
  }

  /* Initialize socket module. */
  socketInit();

  /* Register socket callback function. */
  registerSocketCallback(socket_cb, resolve_cb);

  /* Connect to router. */
  printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
  m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

  /* formata ip */
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = _htons(MAIN_SERVER_PORT);
  inet_aton(MAIN_SERVER_NAME, &addr_in.sin_addr);

  printf(STRING_LINE);

  while(1){
    vTaskDelay(50);
    m2m_wifi_handle_events(NULL);

    if (wifi_connected == M2M_WIFI_CONNECTED) {
      /* Open client socket. */
      if (tcp_client_socket < 0) {
        printf(STRING_LINE);
        printf("socket init \n");
        if ((tcp_client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
          printf("main: failed to create TCP client socket error!\r\n");
          continue;
        }

        /* Connect server */
        printf("socket connecting\n");
        if (connect(tcp_client_socket, (struct sockaddr *)&addr_in,
        sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR) {
          close(tcp_client_socket);
          tcp_client_socket = -1;
          printf("main: error connect to socket\n");
          }else{
          gbTcpConnection = true;
        }
      }
    }
  }
}


int main(void)
{
	/* Initialize the board. */
	sysclk_init();
	board_init();
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_PIO_IDX_MASK, 0, 0, 0 );
	
	configure_pio_input(BUT_PIO, PIO_INPUT, BUT_PIO_IDX_MASK, PIO_PULLUP|PIO_DEBOUNCE, BUT_PIO_ID);
	configure_interruption(BUT_PIO, BUT_PIO_ID, BUT_PIO_IDX_MASK, PIO_IT_FALL_EDGE, but_callback, 4);

	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);
	
	xSemaphoreBotao = xSemaphoreCreateBinary();
	if (xSemaphoreBotao == NULL)
	printf("falha em criar o semaforo \n");

	xTaskCreate(task_wifi, "Wifi", TASK_WIFI_STACK_SIZE, NULL, TASK_WIFI_PRIORITY, &xHandleWifi);
	xTaskCreate(task_process, "process", TASK_PROCESS_STACK_SIZE, NULL, TASK_PROCESS_PRIORITY,  NULL );

	vTaskStartScheduler();

	while(1) {};
	return 0;
}