#pragma once

#define LENGTH(A) (sizeof((A))/sizeof(*(A)))
#define TASK_DELAY_MS(A) vTaskDelay(A / portTICK_PERIOD_MS)
#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))

#define BRUSHED_MOTOR_REGULATOR_PIN      23
#define SDA_PIN                          32
#define SCL_PIN                          33
#define SDA2_PIN                         34
#define SCL2_PIN                         35
#define RX_PIN                           16
#define TX_PIN                           17
#define CAN_RX_PIN                       16
#define CAN_TX_PIN                       17
#define LOADCELL_SDA_PIN                 27
#define LOADCELL_SCL_PIN                 26
#define PWM_OUT_TACHOMETER_FAKER_PIN      5
#define PWM_OUT_ESC_CONTROL_PIN          18
#define RASHODOMER_PIN                   25

#define WS_STREAMER_PORT  222
#define HTTP_TUNER_PORT   333
#define HTTP_TUNER_CTRL   3333

bool http_tuner_start(void);
void http_tuner_stop(void);
void http_tuner_response(const char *format, ...);

void set_engine_id_command(const char *value);
void uart_send_command(const char *value);
void set_esc_pwm_command(const char *value);
void enable_ecu_telemetry_command(const char *value);
void disable_ecu_telemetry_command(const char *value);
