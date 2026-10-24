// MESH ANALYZER: a small ring of the packets the radio hears and sends, streamed to the dashboard.
// Header only. Pure logic (no radio calls) so it is tested on the PC; MyMesh feeds it.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace meshscope {

struct Rec {
  uint32_t ms;          // when
  int16_t  rssi;        // dBm (0 for something we sent)
  int8_t   snr4;        // SNR in quarter dB
  uint8_t  type;        // payload type 0..15
  uint8_t  hops;        // path length
  uint8_t  len;         // packet length, bytes (capped at 255)
  uint8_t  rt;          // route type 0..3
  uint8_t  tx;          // 1 = we sent it
  uint8_t  idn;         // bytes in id[]
  uint8_t  id[3];       // who we heard it from: last hop or the advertising node
  uint8_t  src;         // origin hash byte when the packet carries one
  uint8_t  has_src;
};

struct Scope {
  static const int N = 64;
  Rec      ring[N];
  uint32_t head = 0;       // total records ever written
  uint32_t sent = 0;       // records already streamed
  uint32_t rx = 0, tx = 0;
  uint32_t by_type[16] = {0};

  void add(const Rec& r) {
    ring[head % N] = r; head++;
    if (r.tx) tx++; else rx++;
    by_type[r.type & 15]++;
  }
  // from what the radio hands us (rx)
  void onRx(uint32_t now, float rssi, float snr, uint8_t type, uint8_t route, uint8_t hops, uint8_t hash_size, const uint8_t* path,
            const uint8_t* payload, int payload_len, int total_len) {
    Rec r; memset(&r, 0, sizeof(r));
    r.ms = now; r.rssi = (int16_t)(rssi < -32000 ? -32000 : rssi > 32000 ? 32000 : rssi);
    int s4 = (int)(snr * 4.0f + (snr < 0 ? -0.5f : 0.5f)); r.snr4 = (int8_t)(s4 < -128 ? -128 : s4 > 127 ? 127 : s4);
    r.type = type & 15; r.rt = route & 3; r.hops = hops; r.len = (uint8_t)(total_len > 255 ? 255 : total_len < 0 ? 0 : total_len);
    if (hops > 0 && path && hash_size >= 1) {
      int n = hash_size > 3 ? 3 : hash_size;
      memcpy(r.id, path + (hops - 1) * hash_size, n); r.idn = (uint8_t)n;
    } else if (type == 4 && payload && payload_len >= 32) {      // a zero-hop advert: the sender's public key
      memcpy(r.id, payload, 3); r.idn = 3;
    }
    if (payload && payload_len >= 2) {
      if (type == 4) { r.src = payload[0]; r.has_src = 1; }               // advert: first key byte
      else if (type == 0 || type == 1 || type == 2 || type == 8) { r.src = payload[1]; r.has_src = 1; }   // dest hash, source hash
    }
    add(r);
  }
  void onTx(uint32_t now, uint8_t type, uint8_t route, int total_len) {
    Rec r; memset(&r, 0, sizeof(r));
    r.ms = now; r.type = type & 15; r.rt = route & 3; r.tx = 1; r.len = (uint8_t)(total_len > 255 ? 255 : total_len < 0 ? 0 : total_len);
    add(r);
  }

  // One line of JSON with the records not streamed yet. Returns the length, 0 if nothing to say.
  // sf, bw_khz, cr describe the radio settings so the page can work out airtime.
  size_t line(uint32_t now, char* out, size_t cap, int noise_floor, int sf, float bw_khz, int cr, bool force) {
    uint32_t first = sent;
    if (head - first > (uint32_t)N) first = head - N;                // fell behind: skip the oldest
    if (head == first && !force) return 0;
    size_t n = 0;
    int w = snprintf(out, cap, "{\"mesh\":{\"nf\":%d,\"sf\":%d,\"bw\":%.1f,\"cr\":%d,\"rx\":%lu,\"tx\":%lu,\"ty\":[",
                     noise_floor, sf, (double)bw_khz, cr, (unsigned long)rx, (unsigned long)tx);
    if (w < 0 || (size_t)w >= cap) return 0;
    n = (size_t)w;
    for (int i = 0; i < 16; i++) {
      w = snprintf(out + n, cap - n, "%s%lu", i ? "," : "", (unsigned long)by_type[i]);
      if (w < 0 || n + (size_t)w >= cap) return 0;
      n += (size_t)w;
    }
    w = snprintf(out + n, cap - n, "],\"p\":[");
    if (w < 0 || n + (size_t)w >= cap) return 0;
    n += (size_t)w;
    uint32_t i = first; int cnt = 0;
    for (; i < head; i++) {
      const Rec& r = ring[i % N];
      char id[8] = ""; if (r.idn) snprintf(id, sizeof(id), r.idn == 1 ? "%02x" : r.idn == 2 ? "%02x%02x" : "%02x%02x%02x", r.id[0], r.id[1], r.id[2]);
      char sr[4] = ""; if (r.has_src) snprintf(sr, sizeof(sr), "%02x", r.src);
      char one[96];
      int m = snprintf(one, sizeof(one), "%s[%lu,%d,%d,%u,%u,%u,%u,%u,\"%s\",\"%s\"]", cnt ? "," : "",
                       (unsigned long)(now - r.ms), (int)r.rssi, (int)r.snr4, r.type, r.hops, r.len, r.rt, r.tx, id, sr);
      if (m < 0 || n + (size_t)m + 4 >= cap) break;               // out of room: the rest goes in the next line
      memcpy(out + n, one, (size_t)m); n += (size_t)m; cnt++;
    }
    sent = i;
    w = snprintf(out + n, cap - n, "]}}");
    if (w < 0 || n + (size_t)w >= cap) return 0;
    return n + (size_t)w;
  }
};

}  // namespace meshscope
