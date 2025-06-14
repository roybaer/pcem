#ifndef _SOUND_CSM_H_
#define _SOUND_CSM_H_
#include "sound.h"
#include "ayumi/ayumi.h"

extern device_t csm_device;

typedef struct ay_3_89x0_t {
        int type;
        int last_written;
        uint8_t index;
        uint8_t regs[16];
        struct ayumi chip;
} ay_3_89x0_t;

typedef struct csm_t {
        ay_3_89x0_t psg;
        uint8_t pcm_sample;

        int16_t buffer[MAXSOUNDBUFLEN * 2];
        int pos;
} csm_t;

void csm_init(csm_t *csm, uint16_t base, uint16_t size, uint8_t dma, uint8_t irq, int freq, int psg_type);

#endif /* _SOUND_CSM_H_ */
