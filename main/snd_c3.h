#include <stdint.h>

typedef void (snd_cb_t)(int16_t *buf, int buflen);
void snd_init(int samprate, snd_cb_t *cb);
