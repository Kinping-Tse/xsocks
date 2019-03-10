
#ifndef __TIME_H
#define __TIME_H

#define SECOND_UNIT 1
#define MILLISECOND_UNIT 1000
#define MICROSECOND_UNIT 1000000
#define NANOSECOND_UNIT 1000000000

#define SECOND_UNIT_F (double)1.0
#define MILLISECOND_UNIT_F (double)1000.0
#define MICROSECOND_UNIT_F (double)1000000.0
#define NANOSECOND_UNIT_F (double)1000000000.0

uint64_t timerStart();
double timerStop(uint64_t start_time, int unit, uint64_t *stop_time);

#endif /* __TIME_H */
