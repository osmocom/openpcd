#ifndef PERFORMANCE_H_
#define PERFORMANCE_H_

extern void performance_start(void);
extern void performance_init(void);

typedef struct {
	u_int32_t high; /* 32 bit count of overruns */
	u_int32_t low;  /* 16 bit from T/C running at MCK/2 */
} perf_time_t;

extern perf_time_t performance_get(void);
extern perf_time_t performance_stop(void);

extern void performance_print(perf_time_t time);

struct performance_checkpoint {
	perf_time_t time;
	const char * description;
};

extern void performance_set_checkpoint(const char * const description);
extern void performance_stop_report(void);

#endif /*PERFORMANCE_H_*/
