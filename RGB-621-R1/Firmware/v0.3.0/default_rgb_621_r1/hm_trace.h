#pragma once
// RGB-621-R1: ring buffer of PWM target / duty changes.
// Writes RAM only. Nothing goes to USB until an explicit dump command.

#include <Arduino.h>

#ifndef HM_TRACE_CAP
#define HM_TRACE_CAP 1024      // records; 1024 * 14 B ≈ 14 KB
#endif

// Source of the change. Values are stable — the dump parser keys off them.
enum HmTraceSrc : uint8_t {
  HMT_NONE        = 0,
  HMT_WEBCFG_RGB  = 1,   // handleValues(), payload "rgb"
  HMT_WEBCFG_CCT  = 2,   // handleValues(), payload "cct"
  HMT_MB_DETECT   = 3,   // loop() detector saw an HR mismatch
  HMT_MB_APPLY    = 4,   // applyPwmModbusChange()
  HMT_BULK        = 5,   // applyPwmFromHoldingRegs()
  HMT_WRITE_CH    = 6,   // writePwmCh(): scenes, groups, toggle
  HMT_HOLD_ARM    = 7,   // holdDimArmChannel()
  HMT_HOLD_END    = 8,   // hmHoldDimEnd()
  HMT_POWERON     = 9,   // applyPowerOnOutputs()
  HMT_DUTY        = 10,  // pwmWriteHardware(): what went to analogWrite
};

struct HmTraceRec {
  uint32_t t;      // millis()
  uint8_t  src;
  uint8_t  ch;
  uint16_t a, b, c, d;   // meaning depends on src
} __attribute__((packed));

extern bool        g_traceOn;
extern HmTraceRec  g_traceBuf[HM_TRACE_CAP];
extern uint16_t    g_traceHead;   // next write index
extern uint32_t    g_traceCount;  // total written since last enable
extern uint32_t    g_traceDropped;

inline void hmTraceAdd(uint8_t src, uint8_t ch,
                       uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  if (!g_traceOn) return;
  HmTraceRec& r = g_traceBuf[g_traceHead];
  r.t = millis(); r.src = src; r.ch = ch;
  r.a = a; r.b = b; r.c = c; r.d = d;
  g_traceHead = (uint16_t)((g_traceHead + 1) % HM_TRACE_CAP);
  if (g_traceCount >= HM_TRACE_CAP) g_traceDropped++;
  g_traceCount++;
}

inline void hmTraceReset() {
  g_traceHead = 0; g_traceCount = 0; g_traceDropped = 0;
}
