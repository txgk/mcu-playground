idf_component_register(
	SRCS
	"main.c"
	"tasks.c"
	"streamer-websocket.c"
	"streamer-serial.c"
	"http-tuner.c"
	"common-i2c.c"
	"system-info.c"
	"driver-tpa626.c"
	"driver-pwm-reader.c"
	"driver-thermistors.c"
	INCLUDE_DIRS
	"."
)
