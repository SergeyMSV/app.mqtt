#pragma once

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#endif // _WIN32

#define LIB_UTILS_LOG
#define LIB_UTILS_LOG_COLOR

#define LIB_UTILS_SHARE_MQTT_CONNECTION_RECEIVE_BUFFER_SIZE 512
#define LIB_UTILS_SHARE_MQTT_PACKET_ID_START 100
#define LIB_UTILS_SHARE_MQTT_QUEUE_INCOMING_CAPACITY 15
