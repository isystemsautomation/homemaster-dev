/**
 * HomeMaster WebConfig — shared SimpleWebSerial framework (DIO pilot).
 * Modules register channel counts + onCfg(ext) callback for module-specific fields.
 */
(function (global) {
  'use strict';

  const LOG_MAX = 500;
  const DATA_TIMEOUT_MS = 3000;

  const HMWebConfig = {
    conn: null,
    channels: { in: 0, relay: 0, btn: 0, led: 0 },
    onCfg: null,
    _logBuf: [],
    _lastDataMs: 0,
    _connTimer: null,
    _suppressCfgSend: false,
  };

  function $(id) { return document.getElementById(id); }

  function toArray(x) {
    if (Array.isArray(x)) return x;
    if (x && typeof x === 'object') {
      return Object.keys(x).sort((a, b) => (+a) - (+b)).map(k => x[k]);
    }
    return [];
  }

  function truthy01(v) { return v ? 1 : 0; }

  function appendLog(line) {
    const el = $('hm-log');
    if (!el) return;
    const t = new Date().toLocaleTimeString();
    const body = (typeof line === 'string') ? line : JSON.stringify(line);
    HMWebConfig._logBuf.push(`[${t}] ${body}`);
    if (HMWebConfig._logBuf.length > LOG_MAX) HMWebConfig._logBuf.shift();
    el.textContent = HMWebConfig._logBuf.join('\n');
    el.scrollTop = el.scrollHeight;
  }

  function logTx(channel, packet) {
    try { appendLog(`TX ${channel}: ${JSON.stringify(packet)}`); }
    catch { appendLog(`TX ${channel}`); }
  }

  function markDataReceived() {
    HMWebConfig._lastDataMs = Date.now();
    const dot = $('connDot');
    const txt = $('connText');
    if (dot) dot.className = 'dot ok';
    if (txt) txt.textContent = 'Connected';
  }

  function updateConnectionStatus() {
    const dot = $('connDot');
    const txt = $('connText');
    if (!dot || !txt) return;
    const since = HMWebConfig._lastDataMs > 0
      ? (Date.now() - HMWebConfig._lastDataMs) : Infinity;
    if (since <= DATA_TIMEOUT_MS) {
      dot.className = 'dot ok';
      txt.textContent = 'Connected';
    } else {
      dot.className = 'dot warn';
      txt.textContent = 'Disconnected';
    }
  }

  function startConnectionMonitoring() {
    if (HMWebConfig._connTimer) clearInterval(HMWebConfig._connTimer);
    HMWebConfig._connTimer = setInterval(updateConnectionStatus, 500);
    updateConnectionStatus();
  }

  function stopConnectionMonitoring() {
    if (HMWebConfig._connTimer) {
      clearInterval(HMWebConfig._connTimer);
      HMWebConfig._connTimer = null;
    }
    HMWebConfig._lastDataMs = 0;
    updateConnectionStatus();
  }

  function setDot(sec, idx, on) {
    const el = $(`state-${sec}${idx}`);
    if (el) el.className = 'state-dot ' + (on ? 'dot-on' : 'dot-off');
  }

  function applyIo(io) {
    if (!io) return;
    markDataReceived();
    const secs = ['in', 'relay', 'btn', 'led'];
    secs.forEach(sec => {
      const n = HMWebConfig.channels[sec] || 0;
      const arr = toArray(io[sec]);
      for (let i = 0; i < n; i++) {
        setDot(sec, i + 1, !!arr[i]);
      }
    });
  }

  function applyStatus(st) {
    if (!st) return;
    markDataReceived();
    const model = $('hm-model');
    const fw = $('hm-fw');
    const addr = $('hm-addr');
    const baud = $('hm-baud');
    if (model && st.model != null) model.textContent = String(st.model);
    if (fw && st.fw != null) fw.textContent = String(st.fw);
    const a = (st.addr != null) ? st.addr : st.address;
    const b = (st.baud != null) ? st.baud : st.baud;
    if (addr && a != null) addr.textContent = String(a);
    if (baud && b != null) baud.textContent = String(b);
    const selAddr = $('modbus-address');
    const selBaud = $('modbus-baud');
    if (selAddr && a != null) selAddr.value = String(a);
    if (selBaud && b != null) selBaud.value = String(b);
  }

  function setCheck(id, v) {
    const el = $(id);
    if (el && el.type === 'checkbox') el.checked = !!v;
  }

  function setSelect(id, v) {
    const el = $(id);
    if (el && el !== document.activeElement) el.value = String(v ?? 0);
  }

  function applyCfg(cfg) {
    if (!cfg) return;
    markDataReceived();
    HMWebConfig._suppressCfgSend = true;
    try {
      const nIn = HMWebConfig.channels.in || 0;
      const inArr = toArray(cfg.in);
      for (let i = 0; i < nIn; i++) {
        const o = inArr[i] || {};
        setCheck(`enable-in${i + 1}`, o.enabled);
        setCheck(`invert-in${i + 1}`, o.invert);
        setSelect(`action-in${i + 1}`, o.action);
        setSelect(`target-in${i + 1}`, o.target);
      }

      const nRly = HMWebConfig.channels.relay || 0;
      const rlyArr = toArray(cfg.relay);
      for (let i = 0; i < nRly; i++) {
        const o = rlyArr[i] || {};
        setCheck(`enable-relay${i + 1}`, o.enabled);
        setCheck(`invert-relay${i + 1}`, o.invert);
        if (o.powerOn != null) setSelect(`powerOn-relay${i + 1}`, o.powerOn);
      }

      const nBtn = HMWebConfig.channels.btn || 0;
      const btnArr = toArray(cfg.btn);
      for (let i = 0; i < nBtn; i++) {
        const o = btnArr[i];
        const act = (o && typeof o === 'object') ? o.action : o;
        setSelect(`action-btn${i + 1}`, act);
      }

      const nLed = HMWebConfig.channels.led || 0;
      const ledArr = toArray(cfg.led);
      for (let i = 0; i < nLed; i++) {
        const o = ledArr[i] || {};
        setSelect(`mode-led${i + 1}`, o.mode);
        setSelect(`source-led${i + 1}`, o.source);
      }

      if (typeof HMWebConfig.onCfg === 'function') {
        HMWebConfig.onCfg(cfg.ext || {}, cfg);
      }
    } finally {
      HMWebConfig._suppressCfgSend = false;
    }
  }

  function bindIncoming(conn) {
    conn.on('status', applyStatus);
    conn.on('io', applyIo);
    conn.on('cfg', applyCfg);
    conn.on('log', msg => { markDataReceived(); appendLog(msg); });

    conn.on('open', () => {
      appendLog('port: open');
      HMWebConfig._lastDataMs = 0;
      startConnectionMonitoring();
    });
    conn.on('close', () => {
      appendLog('port: close');
      stopConnectionMonitoring();
    });

    conn.on('message', m => {
      markDataReceived();
      appendLog(m);
    });
  }

  HMWebConfig.register = function register(opts) {
    if (opts && opts.channels) {
      Object.assign(HMWebConfig.channels, opts.channels);
    }
    if (opts && typeof opts.onCfg === 'function') {
      HMWebConfig.onCfg = opts.onCfg;
    }
  };

  HMWebConfig.connect = function connect(opts) {
    const requestElement = (opts && opts.requestElement) || 'connect-button';
    if (!('serial' in navigator)) {
      appendLog('This browser does not support Web Serial API. Use Chrome/Edge over HTTPS.');
    }

    let conn;
    try {
      conn = SimpleWebSerial.setupSerialConnection({
        requestAccessOnPageLoad: false,
        requestElement,
      });
    } catch (e) {
      appendLog('SimpleWebSerial init failed: ' + (e?.message || e));
      conn = {
        on: () => {},
        isOpen: () => false,
        send: () => Promise.reject(new Error('SWS not ready')),
      };
    }

    HMWebConfig.conn = conn;
    bindIncoming(conn);

    window.addEventListener('error', e => appendLog('JS Error: ' + (e.message || e)));
    window.addEventListener('unhandledrejection', e => {
      const errMsg = e.reason?.message || String(e.reason || '');
      if (errMsg.includes('NetworkError') || errMsg.includes('device has been lost')) {
        HMWebConfig._lastDataMs = 0;
        updateConnectionStatus();
      }
      appendLog('Unhandled Rejection: ' + errMsg);
    });

    return conn;
  };

  HMWebConfig.sendConfig = function sendConfig(t, list) {
    if (HMWebConfig._suppressCfgSend) return Promise.resolve();
    const packet = { t, list };
    logTx('Config', packet);
    return HMWebConfig.conn.send('Config', packet).catch(err => {
      appendLog('Config send failed: ' + (err?.message || err));
    });
  };

  HMWebConfig.sendValues = function sendValues(addr, baud) {
    const packet = { mb_address: addr, mb_baud: baud };
    logTx('values', packet);
    return HMWebConfig.conn.send('values', packet).catch(() => {
      appendLog('Failed to send Modbus values');
    });
  };

  HMWebConfig.sendCommand = function sendCommand(action) {
    const packet = { action };
    logTx('command', packet);
    return HMWebConfig.conn.send('command', packet).catch(err => {
      appendLog('Command failed: ' + (err?.message || err));
    });
  };

  HMWebConfig.appendLog = appendLog;
  HMWebConfig.toArray = toArray;
  HMWebConfig.truthy01 = truthy01;

  global.HMWebConfig = HMWebConfig;
})(typeof window !== 'undefined' ? window : globalThis);
