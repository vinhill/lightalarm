#ifndef WEB_H_
#define WEB_H_

typedef struct {
	uint8_t* intenmin;
	uint8_t* intenmax;
	uint8_t* alarm_duration;
	uint8_t* alarm_forerun;
	uint8_t* active_duration;
	uint8_t* alarm_hour;
	uint8_t* alarm_minute;
	void (*get_freq)(void);
	void (*set_freq)(float freq);
} ConfigVariables;

void init_webserver();

#endif /* WEB_H_ */