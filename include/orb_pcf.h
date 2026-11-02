#ifndef ORB_PCF_H
#define ORB_PCF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OrbPcf OrbPcf;

typedef struct OrbPcfParams {
    uint8_t frequency;
    uint8_t resonance;
    uint8_t amount;
    uint8_t decay;
    uint8_t pattern;
    uint8_t mode;
    uint8_t bypass;
    uint8_t seq_enable;
    uint8_t seq_length;
    uint8_t seq_resolution;
    uint8_t seq_steps[32];
} OrbPcfParams;

typedef struct OrbPcfPlayhead {
    double bpm;
    double ppq;
    int32_t playing;
} OrbPcfPlayhead;

OrbPcf *orb_pcf_new(void);
void orb_pcf_free(OrbPcf *pcf);
void orb_pcf_reset(OrbPcf *pcf);
int32_t orb_pcf_latency(void);
void orb_pcf_process(OrbPcf *pcf,
                     float *left,
                     float *right,
                     int32_t nframes,
                     const OrbPcfParams *params,
                     const OrbPcfPlayhead *playhead);

#ifdef __cplusplus
}
#endif

#endif
