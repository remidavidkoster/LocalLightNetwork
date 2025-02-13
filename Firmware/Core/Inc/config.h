#ifndef CONFIG_H
#define CONFIG_H

#define MODE_4_CHANNELS 0
#define MODE_CCT 1

#define CCT_CHANNEL_1 0
#define CCT_CHANNEL_2 1

#define MIN_CCT 200
#define MAX_CCT 250

#define LIMIT(min, x, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))


#endif // CONFIG_H
