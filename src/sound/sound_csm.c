#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "sound.h"
#include "sound_csm.h"
#include "cpu.h"

void csm_update(csm_t *csm) {
        for (; csm->pos < sound_pos_global; csm->pos++) {
                ayumi_process(&csm->psg.chip);

                ayumi_remove_dc(&csm->psg.chip);

                double pcm_scaled = (csm->pcm_sample - 128) / 128.0;
                csm->buffer[csm->pos << 1] = (csm->psg.chip.left + pcm_scaled) * 16000;
                csm->buffer[(csm->pos << 1) + 1] = (csm->psg.chip.right + pcm_scaled) * 16000;
        }
}

void csm_get_buffer(int32_t *buffer, int len, void *p) {
        csm_t *csm = (csm_t *)p;
        int c;

        csm_update(csm);

        for (c = 0; c < len * 2 ; c++)
                buffer[c] += csm->buffer[c];

        csm->pos = 0;
}

static void csm_mode_bits_changed(csm_t *csm) {
        uint8_t a = (csm->psg.regs[7] & 0x40) ? csm->psg.regs[14] : 0x88;
        uint8_t b = (csm->psg.regs[7] & 0x80) ? csm->psg.regs[15] : 0xf0;

        // IOB4 1:mono, 0:stereo -- for channels A and B
        // IOB5 1:disable IRQ, 0:enable IRQ
        // IOB6 DMA/IRQ?
        // IOB7 1:enable, 0:disable -- audio output of channel C
        double vol_left = (a & 0xf) / 15.0;
        double vol_right = (a >> 4) / 15.0;

        // massage Ayumi's private parts in the appropriate way
        csm->psg.chip.channels[0].pan_left = (b & 0x10) ? vol_left : 0.0;
        csm->psg.chip.channels[0].pan_right = vol_right;
        csm->psg.chip.channels[1].pan_left = vol_left;
        csm->psg.chip.channels[1].pan_right = (b & 0x10) ? vol_right : 0.0;
        csm->psg.chip.channels[2].pan_left = (b & 0x80) ? vol_left : 0.0;
        csm->psg.chip.channels[2].pan_right = (b & 0x80) ? vol_right : 0.0;
}

void csm_write(uint16_t addr, uint8_t data, void *p) {
        csm_t *csm = (csm_t *)p;
        ay_3_89x0_t *ay = &csm->psg;

        csm_update(csm);

        switch (addr & 0x20) {
                case 0:
                        csm->psg.index = data;
                        break;
                case 1:
                        switch (ay->index) {
                                case 0:
                                        ay->regs[0] = data;
                                        ayumi_set_tone(&ay->chip, 0, (ay->regs[1] << 8) | ay->regs[0]);
                                        break;
                                case 1:
                                        ay->regs[1] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 0, (ay->regs[1] << 8) | ay->regs[0]);
                                        break;
                                case 2:
                                        ay->regs[2] = data;
                                        ayumi_set_tone(&ay->chip, 1, (ay->regs[3] << 8) | ay->regs[2]);
                                        break;
                                case 3:
                                        ay->regs[3] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 1, (ay->regs[3] << 8) | ay->regs[2]);
                                        break;
                                case 4:
                                        ay->regs[4] = data;
                                        ayumi_set_tone(&ay->chip, 2, (ay->regs[5] << 8) | ay->regs[4]);
                                        break;
                                case 5:
                                        ay->regs[5] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 2, (ay->regs[5] << 8) | ay->regs[4]);
                                        break;
                                case 6:
                                        ay->regs[6] = data & 0x1f;
                                        ayumi_set_noise(&ay->chip, ay->regs[6]);
                                        break;
                                case 7:
                                        ay->regs[7] = data;
                                        ayumi_set_mixer(&ay->chip, 0, data & 1, (data >> 3) & 1, (ay->regs[8] >> 4) & 1);
                                        ayumi_set_mixer(&ay->chip, 1, (data >> 1) & 1, (data >> 4) & 1, (ay->regs[9] >> 4) & 1);
                                        ayumi_set_mixer(&ay->chip, 2, (data >> 2) & 1, (data >> 5) & 1, (ay->regs[10] >> 4) & 1);
                                        csm_mode_bits_changed(csm);
                                        break;
                                case 8:
                                        ay->regs[8] = data;
                                        ayumi_set_volume(&ay->chip, 0, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 0, ay->regs[7] & 1, (ay->regs[7] >> 3) & 1, (data >> 4) & 1);
                                        break;
                                case 9:
                                        ay->regs[9] = data;
                                        ayumi_set_volume(&ay->chip, 1, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 1, (ay->regs[7] >> 1) & 1, (ay->regs[7] >> 4) & 1, (data >> 4) & 1);
                                        break;
                                case 10:
                                        ay->regs[10] = data;
                                        ayumi_set_volume(&ay->chip, 2, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 2, (ay->regs[7] >> 2) & 1, (ay->regs[7] >> 5) & 1, (data >> 4) & 1);
                                        break;
                                case 11:
                                        ay->regs[11] = data;
                                        ayumi_set_envelope(&ay->chip, (ay->regs[12] >> 8) | ay->regs[11]);
                                        break;
                                case 12:
                                        ay->regs[12] = data;
                                        ayumi_set_envelope(&ay->chip, (ay->regs[12] >> 8) | ay->regs[11]);
                                        break;
                                case 13:
                                        ay->regs[13] = data;
                                        ayumi_set_envelope_shape(&ay->chip, data & 0xf);
                                        break;
                                case 14:
                                        ay->regs[14] = data;
                                        csm_mode_bits_changed(csm);
                                        break;
                                case 15:
                                        ay->regs[15] = data;
                                        csm_mode_bits_changed(csm);
                                        break;
                        }
                        break;
                case 2:
                        csm->pcm_sample = data;
                        break;
                case 3:
                        // TODO: clear IRQ
                        break;
        }
}

uint8_t csm_read(uint16_t addr, void *p) {
        csm_t *csm = (csm_t *)p;
        ay_3_89x0_t *ay = &csm->psg;

        switch (addr & 0x1f) {
                case 1:
                        switch (ay->index) {
                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                case 6:
                                case 7:
                                case 8:
                                case 9:
                                case 10:
                                case 11:
                                case 12:
                                case 13:
                                        return ay->regs[ay->index];
                                case 14:
                                        return (ay->regs[7] & 0x40) ? ay->regs[14] : 0;
                                case 15:
                                        // there are pull-up resistors on IOB7..IOB4
                                        return (ay->regs[7] & 0x80) ? ay->regs[15] : 0xf0;
                        }
                        break;
                case 4:
                        // TODO: read joystick 2?
                        return 0;
                case 5:
                        // TODO: read joystick 1?
                        return 0;
                default:
                        return 0;
        }
}

void csm_init(csm_t *csm, uint16_t base, uint16_t size, uint8_t dma, uint8_t irq, int freq, int psg_type) {
        sound_add_handler(csm_get_buffer, csm);

        ayumi_configure(&csm->psg.chip, psg_type, freq, 48000);
        csm_mode_bits_changed(csm);

        io_sethandler(base, size, csm_read, NULL, NULL, csm_write, NULL, NULL, csm);
}

void *csm_device_init() {
        csm_t *csm = malloc(sizeof(csm_t));
        uint16_t base_addr = device_get_config_int("addr");
        uint8_t dma_channel = device_get_config_int("dma");
        uint8_t irq_channel = device_get_config_int("irq");
        int psg_type = device_get_config_int("psg");
        memset(csm, 0, sizeof(csm_t));

        csm_init(csm, base_addr, 0x20, dma_channel, irq_channel, 1789773, psg_type);

        return csm;
}

void csm_device_close(void *p) {
        csm_t *csm = (csm_t *)p;

        free(csm);
}

static device_config_t csm_config[] = {
        {.name = "addr",
         .description = "Base address",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "220h", .value = 0x220},
                       {.description = "240h", .value = 0x240},
                       {.description = "280h", .value = 0x280},
                       {.description = "2C0h", .value = 0x2c0},
                       {.description = ""}},
         .default_int = 0x220},
        {.name = "dma",
         .description = "DMA channel",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "none", .value = 0},
                       // {.description = "1", .value = 1},
                       // {.description = "3", .value = 3},
                       {.description = ""}},
         .default_int = 0},
        {.name = "irq",
         .description = "IRQ channel",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "none", .value = 0},
                       // {.description = "3", .value = 3},
                       // {.description = "7", .value = 7},
                       {.description = ""}},
         .default_int = 0},
        {.name = "psg",
         .description = "Synthesizer",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "AY-3-8910", .value = 0},
                       {.description = "YM2149", .value = 1},
                       // {.description = "AY8930", .value = 2},
                       {.description = ""}},
         .default_int = 0},
        {.type = -1}};

device_t csm_device = {"Covox Sound Master", 0, csm_device_init, csm_device_close, NULL, NULL, NULL, NULL, csm_config};
