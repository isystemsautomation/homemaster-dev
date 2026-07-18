/**
 * HomeMaster WebConfig — shared SimpleWebSerial framework (DIO pilot).
 * Modules register channel counts + onCfg(ext) callback for module-specific fields.
 */
(function (global) {
  'use strict';

  const LOG_MAX = 500;
  const DATA_TIMEOUT_MS = 3000;
  const STATUS_IDENTITY_TIMEOUT_MS = 3000;

  const MODEL_NAMES = {
    1: 'ALM-173-R1',
    2: 'ENM-223-R1',
    3: 'DIM-420-R1',
    4: 'AIO-422-R1',
    5: 'DIO-430-R1',
    6: 'WLD-521-R1',
    7: 'RGB-621-R1',
    8: 'STR-3221-R1',
  };

  const HMWebConfig = {
    conn: null,
    channels: { in: 0, relay: 0, btn: 0, led: 0 },
    expected: { model: null, modelName: '', fw: '' },
    about: { version: '', readmeUrl: '', firmwareUrl: '' },
    onCfg: null,
    _logBuf: [],
    _lastDataMs: 0,
    _connTimer: null,
    _suppressCfgSend: false,
    _compat: { level: 'idle', blocked: false, factoryBlocked: false },
    _statusIdentityTimer: null,
    _toolsIds: { identify: null, factory: null, reboot: null },
    _localLogic: false,
    _portOpen: false,
    _helloSent: false,
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

  function identityExpected() {
    const e = HMWebConfig.expected;
    return e.model != null && !!e.fw;
  }

  function modelLabel(id) {
    const n = Number(id);
    const name = MODEL_NAMES[n];
    return name ? ` (${name})` : '';
  }

  function identityField(st, key) {
    if (!st || st[key] == null || st[key] === '') return null;
    return st[key];
  }

  function hasCompleteIdentity(st) {
    return identityField(st, 'model') != null
      && identityField(st, 'fw') != null;
  }

  function ensureCompatEl() {
    let el = $('hm-compat');
    if (el) return el;
    el = document.createElement('div');
    el.id = 'hm-compat';
    const tools = $('hm-tools');
    if (tools && tools.parentNode) {
      tools.parentNode.insertBefore(el, tools);
    } else {
      const header = document.querySelector('header.appbar');
      if (header && header.parentNode) {
        header.parentNode.insertBefore(el, header.nextSibling);
      }
    }
    return el;
  }

  function parseFwTuple(s) {
    const parts = String(s).replace(/-.*$/, '').split('.').map((x) => parseInt(x, 10) || 0);
    while (parts.length < 3) parts.push(0);
    return parts;
  }

  function compareFw(got, expected) {
    const A = parseFwTuple(got);
    const B = parseFwTuple(expected);
    for (let i = 0; i < 3; i++) {
      if (A[i] !== B[i]) return A[i] - B[i];
    }
    return 0;
  }

  function evaluateCompat(st) {
    const exp = HMWebConfig.expected;
    if (!hasCompleteIdentity(st)) {
      return {
        level: 'no-data',
        blocked: true,
        factoryBlocked: true,
        message: "Couldn't read module model/firmware — incompatible or old firmware, or the module isn't responding.",
      };
    }
    const gotModel = Number(st.model);
    const gotFw = String(st.fw);
    if (gotModel !== Number(exp.model)) {
      return {
        level: 'model',
        blocked: true,
        factoryBlocked: true,
        message: `Wrong module: this configurator is for ${exp.modelName} (model ${exp.model}), module reports model ${gotModel}${modelLabel(gotModel)}.`,
      };
    }
    if (gotFw !== String(exp.fw)) {
      const cmp = compareFw(gotFw, exp.fw);
      const message = cmp > 0
        ? `Module firmware is ${gotFw} (newer than this WebConfig page, built for ${exp.fw}). Open the matching configurator version or continue — setup usually still works.`
        : `Module firmware is ${gotFw}; this page expects ${exp.fw}. Flash newer firmware on the module.`;
      return {
        level: 'fw',
        blocked: cmp < 0,
        factoryBlocked: false,
        message,
      };
    }
    return { level: 'ok', blocked: false, factoryBlocked: false, message: '' };
  }

  function renderCompatBanner(result) {
    const el = ensureCompatEl();
    if (!el) return;
    if (!identityExpected()) {
      el.innerHTML = '';
      el.style.display = 'none';
      return;
    }
    if (result.level === 'ok') {
      el.innerHTML = '';
      el.style.display = 'none';
      return;
    }
    const cls = result.level === 'fw' ? 'hm-compat-warn-fw' : 'hm-compat-error';
    el.className = 'hm-compat-banner ' + cls;
    el.style.display = 'block';
    el.textContent = result.message;
  }

  function applyCompatUI(result) {
    HMWebConfig._compat.level = result.level;
    HMWebConfig._compat.blocked = !!result.blocked;
    HMWebConfig._compat.factoryBlocked = !!result.factoryBlocked;
    document.body.classList.toggle('hm-compat-blocked', HMWebConfig._compat.blocked);
    renderCompatBanner(result);
    const factoryBtn = HMWebConfig._toolsIds.factory && $(HMWebConfig._toolsIds.factory);
    if (factoryBtn) factoryBtn.disabled = HMWebConfig._compat.factoryBlocked;
  }

  /** Visible notice when a write is refused because of compat gate (banner + log). */
  function notifyWriteBlocked(detail) {
    const msg = detail || 'Setting was NOT applied: firmware/model compatibility issue.';
    appendLog(msg);
    const el = ensureCompatEl();
    if (!el) return;
    el.className = 'hm-compat-banner hm-compat-error';
    el.style.display = 'block';
    el.textContent = msg;
  }

  function resetCompatState() {
    if (HMWebConfig._statusIdentityTimer) {
      clearTimeout(HMWebConfig._statusIdentityTimer);
      HMWebConfig._statusIdentityTimer = null;
    }
    applyCompatUI({ level: 'idle', blocked: false, factoryBlocked: false, message: '' });
  }

  function scheduleIdentityTimeout() {
    if (!identityExpected()) return;
    if (HMWebConfig._statusIdentityTimer) clearTimeout(HMWebConfig._statusIdentityTimer);
    HMWebConfig._statusIdentityTimer = setTimeout(() => {
      HMWebConfig._statusIdentityTimer = null;
      applyCompatUI(evaluateCompat({}));
    }, STATUS_IDENTITY_TIMEOUT_MS);
  }

  function checkCompatFromStatus(st) {
    if (!identityExpected()) return;
    if (HMWebConfig._statusIdentityTimer) {
      clearTimeout(HMWebConfig._statusIdentityTimer);
      HMWebConfig._statusIdentityTimer = null;
    }
    applyCompatUI(evaluateCompat(st));
  }

  function resetModuleHeaderFields() {
    const linkDot = $('linkDot');
    const linkText = $('linkText');
    if (linkDot) linkDot.className = 'dot warn';
    if (linkText) linkText.textContent = '—';
    ['hm-model', 'hm-fw', 'hm-addr', 'hm-baud'].forEach((id) => {
      const el = $(id);
      if (el) el.textContent = '—';
    });
    const ll = $('local-logic-toggle');
    if (ll && document.activeElement !== ll) ll.checked = false;
    clearIoDots();
  }

  function clearIoDots() {
    ['in', 'relay', 'btn', 'led'].forEach((sec) => {
      const n = HMWebConfig.channels[sec] || 0;
      for (let i = 1; i < n + 1; i++) setDot(sec, i, false);
    });
  }

  function setDisconnectedUI() {
    const dot = $('connDot');
    const txt = $('connText');
    if (dot) dot.className = 'dot warn';
    if (txt) txt.textContent = 'Disconnected';
    resetModuleHeaderFields();
    resetCompatState();
    HMWebConfig._compat.blocked = true;
    HMWebConfig._compat.factoryBlocked = true;
    document.body.classList.add('hm-compat-blocked');
    const factoryBtn = HMWebConfig._toolsIds.factory && $(HMWebConfig._toolsIds.factory);
    if (factoryBtn) factoryBtn.disabled = true;
    HMWebConfig._helloSent = false;
  }

  function applyLinkStatus(st) {
    const linkDot = $('linkDot');
    const linkText = $('linkText');
    if (!linkDot && !linkText) return;
    if (st.linkOk == null) return;
    const linkOk = !!st.linkOk;
    if (linkDot) linkDot.className = 'dot ' + (linkOk ? 'ok' : 'warn');
    if (linkText) linkText.textContent = linkOk ? 'Link OK' : 'No poll';
  }

  function applyLocalLogicStatus(st) {
    const el = $('local-logic-toggle');
    if (!el || st.localLogic == null) return;
    if (document.activeElement !== el) el.checked = !!st.localLogic;
  }

  function wireLocalLogicToggle() {
    if (!HMWebConfig._localLogic) return;
    const el = $('local-logic-toggle');
    if (!el || el.dataset.hmWired) return;
    el.dataset.hmWired = '1';
    el.addEventListener('change', () => {
      if (HMWebConfig._compat.blocked) return;
      HMWebConfig.sendConfig('global', { localLogic: el.checked });
    });
  }

  function appendLog(line) {
    const el = $('hm-log');
    if (!el) return;
    const t = new Date().toLocaleTimeString();
    let body = (typeof line === 'string') ? line : JSON.stringify(line);
    if (body.length > 240) body = body.slice(0, 240) + '…';
    HMWebConfig._logBuf.push(`[${t}] ${body}`);
    if (HMWebConfig._logBuf.length > LOG_MAX) HMWebConfig._logBuf.shift();
    el.textContent = HMWebConfig._logBuf.join('\n');
    el.scrollTop = el.scrollHeight;
  }

  function fallbackCopyText(text) {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.setAttribute('readonly', '');
    ta.style.position = 'fixed';
    ta.style.left = '-9999px';
    document.body.appendChild(ta);
    ta.select();
    let ok = false;
    try { ok = document.execCommand('copy'); } catch (_) { ok = false; }
    document.body.removeChild(ta);
    return ok;
  }

  function copyLog() {
    const text = (HMWebConfig._logBuf || []).join('\n');
    const btn = $('btn-copy-log');
    const flash = (label) => {
      if (!btn) return;
      const prev = btn.textContent;
      btn.textContent = label;
      setTimeout(() => { btn.textContent = prev; }, 1500);
    };
    if (!text) {
      flash('Empty');
      return;
    }
    const onOk = () => flash('Copied');
    const onFail = () => flash('Copy failed');
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(onOk).catch(() => {
        if (fallbackCopyText(text)) onOk(); else onFail();
      });
    } else if (fallbackCopyText(text)) {
      onOk();
    } else {
      onFail();
    }
  }

  function ensureCopyLogButton() {
    if ($('btn-copy-log')) {
      const existing = $('btn-copy-log');
      if (!existing.dataset.hmWired) {
        existing.dataset.hmWired = '1';
        existing.addEventListener('click', copyLog);
      }
      return;
    }
    const log = $('hm-log');
    if (!log) return;
    const box = $('hm-log-box') || log.parentElement;
    const header = box && box.closest ? box.closest('section.card')?.querySelector('.cardheader') : null;
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'btn';
    btn.id = 'btn-copy-log';
    btn.textContent = 'Copy log';
    btn.dataset.hmWired = '1';
    btn.addEventListener('click', copyLog);
    if (header) header.appendChild(btn);
    else if (box && box.parentElement) box.parentElement.insertBefore(btn, box);
  }

  function logTx(channel, packet) {
    try { appendLog(`TX ${channel}: ${JSON.stringify(packet)}`); }
    catch { appendLog(`TX ${channel}`); }
  }

  function markDataReceived() {
    HMWebConfig._portOpen = true;
    HMWebConfig._lastDataMs = Date.now();
    const dot = $('connDot');
    const txt = $('connText');
    if (dot) dot.className = 'dot ok';
    if (txt) txt.textContent = 'Connected';
    if (!HMWebConfig._helloSent && HMWebConfig.conn) {
      HMWebConfig._helloSent = true;
      const packet = { action: 'hello' };
      logTx('command', packet);
      HMWebConfig.conn.send('command', packet).catch(err => {
        HMWebConfig._helloSent = false;
        appendLog('Config hello failed: ' + (err?.message || err));
      });
    }
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
      setDisconnectedUI();
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
    if (model && st.model != null) {
      const mn = Number(st.model);
      model.textContent = MODEL_NAMES[mn] || String(st.model);
    }
    if (fw && st.fw != null) fw.textContent = String(st.fw);
    const a = (st.addr != null) ? st.addr : st.address;
    const b = (st.baud != null) ? st.baud : st.baudRate;
    if (addr && a != null) addr.textContent = String(a);
    if (baud && b != null) baud.textContent = String(b);
    const selAddr = $('modbus-address');
    const selBaud = $('modbus-baud');
    if (selAddr && a != null) selAddr.value = String(a);
    if (selBaud && b != null) selBaud.value = String(b);
    applyLinkStatus(st);
    applyLocalLogicStatus(st);
    checkCompatFromStatus(st);
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
      HMWebConfig._portOpen = true;
      HMWebConfig._lastDataMs = 0;
      HMWebConfig._helloSent = false;
      HMWebConfig._compat.blocked = true;
      document.body.classList.add('hm-compat-blocked');
      resetModuleHeaderFields();
      startConnectionMonitoring();
      scheduleIdentityTimeout();
    });
    conn.on('close', () => {
      appendLog('port: close');
      HMWebConfig._portOpen = false;
      stopConnectionMonitoring();
      setDisconnectedUI();
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
    if (opts) {
      if (opts.model != null) HMWebConfig.expected.model = Number(opts.model);
      if (opts.modelName) HMWebConfig.expected.modelName = String(opts.modelName);
      if (opts.fw) HMWebConfig.expected.fw = String(opts.fw);
      if (opts.localLogic) HMWebConfig._localLogic = true;

      const version = (opts.webconfigVersion != null) ? String(opts.webconfigVersion) : '';
      const readmeUrl = (opts.readmeUrl != null) ? String(opts.readmeUrl) : '';
      const firmwareUrl = (opts.firmwareUrl != null) ? String(opts.firmwareUrl) : '';
      HMWebConfig.about = { version, readmeUrl, firmwareUrl };
    }
    if (identityExpected()) ensureCompatEl();
    wireLocalLogicToggle();

    const wc = $('hm-wc');
    if (wc && HMWebConfig.about.version) wc.textContent = HMWebConfig.about.version;

    const a = HMWebConfig.about || {};
    const ver = a.version || '';
    const fw = (HMWebConfig.expected && HMWebConfig.expected.fw) || '';
    const links = $('hm-links');
    if (links) {
      links.textContent = '';
      if (ver || fw) {
        const lab = document.createElement('span');
        lab.className = 'hm-doc-label';
        lab.textContent = 'WebConfig v' + ver + (fw ? (' · built for firmware v' + fw) : '');
        links.appendChild(lab);
      }
      if (a.readmeUrl) {
        if (links.childNodes.length) links.appendChild(document.createTextNode(' '));
        const ra = document.createElement('a');
        ra.className = 'tag hm-doc';
        ra.setAttribute('href', a.readmeUrl);
        ra.setAttribute('target', '_blank');
        ra.setAttribute('rel', 'noopener');
        ra.textContent = 'README';
        links.appendChild(ra);
      }
      if (a.firmwareUrl) {
        if (links.childNodes.length) links.appendChild(document.createTextNode(' '));
        const fa = document.createElement('a');
        fa.className = 'tag hm-doc';
        fa.setAttribute('href', a.firmwareUrl);
        fa.setAttribute('target', '_blank');
        fa.setAttribute('rel', 'noopener');
        fa.textContent = 'Firmware (UF2)';
        links.appendChild(fa);
      }
    }
  };

  HMWebConfig.connect = function connect(opts) {
    ensureCopyLogButton();
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
    startConnectionMonitoring();

    window.addEventListener('error', e => appendLog('JS Error: ' + (e.message || e)));
    window.addEventListener('unhandledrejection', e => {
      const errMsg = e.reason?.message || String(e.reason || '');
      if (errMsg.includes('NetworkError') || errMsg.includes('device has been lost')) {
        HMWebConfig._lastDataMs = 0;
        setDisconnectedUI();
      }
      appendLog('Unhandled Rejection: ' + errMsg);
    });

    return conn;
  };

  HMWebConfig.sendConfig = function sendConfig(t, list) {
    if (HMWebConfig._compat.blocked) {
      notifyWriteBlocked('Config write blocked: setting was NOT applied (firmware/model compatibility).');
      return Promise.resolve();
    }
    if (HMWebConfig._suppressCfgSend) return Promise.resolve();
    const packet = { t, list };
    logTx('Config', packet);
    return HMWebConfig.conn.send('Config', packet).catch(err => {
      appendLog('Config send failed: ' + (err?.message || err));
    });
  };

  HMWebConfig.sendValues = function sendValues(addr, baud) {
    if (HMWebConfig._compat.blocked) {
      notifyWriteBlocked('Modbus values write blocked: setting was NOT applied (firmware/model compatibility).');
      return Promise.resolve();
    }
    const packet = { mb_address: addr, mb_baud: baud };
    logTx('values', packet);
    return HMWebConfig.conn.send('values', packet).catch(() => {
      appendLog('Failed to send Modbus values');
    });
  };

  HMWebConfig.sendCommand = function sendCommand(action) {
    const act = String(action || '').toLowerCase();
    if (act !== 'identify' && HMWebConfig._compat.blocked) {
      notifyWriteBlocked('Command blocked: "' + act + '" was NOT sent (firmware/model compatibility).');
      return Promise.resolve();
    }
    if (act === 'factory' && HMWebConfig._compat.factoryBlocked) {
      notifyWriteBlocked('Factory reset blocked: firmware/model compatibility issue.');
      return Promise.resolve();
    }
    const packet = { action };
    logTx('command', packet);
    return HMWebConfig.conn.send('command', packet).catch(err => {
      appendLog('Command failed: ' + (err?.message || err));
    });
  };

  /**
   * Mount standard Tools panel (Identify / Factory reset / Reboot).
   * @param {string} targetId - container element id (e.g. 'hm-tools')
   * @param {{ identify?: boolean }} [opts] - show Identify when module has user LEDs
   */
  HMWebConfig.mountTools = function mountTools(targetId, opts) {
    const el = $(targetId);
    if (!el) return;
    const identify = !!(opts && opts.identify);
    const uid = targetId.replace(/[^a-zA-Z0-9_-]/g, '_');
    const idIdentify = uid + '_btn_identify';
    const idFactory = uid + '_btn_factory';
    const idReboot = uid + '_btn_reboot';
    el.innerHTML =
      '<div class="sectiontitle"><h3>Tools</h3></div>' +
      '<section class="card">' +
      '<div class="cardbody" style="display:flex;flex-wrap:wrap;gap:10px;align-items:center">' +
      (identify ? '<button type="button" class="btn hm-tools-allow" id="' + idIdentify + '">Identify (~5 s)</button>' : '') +
      '<button type="button" class="btn hm-tools-allow" id="' + idFactory + '">Factory reset</button>' +
      '<button type="button" class="btn danger hm-tools-allow" id="' + idReboot + '">Reboot</button>' +
      '<span class="hint" style="flex:1 1 100%;margin:0">Changes are saved automatically.</span>' +
      '</div></section>';
    HMWebConfig._toolsIds = { identify: idIdentify, factory: idFactory, reboot: idReboot };
    $(idIdentify)?.addEventListener('click', () => HMWebConfig.sendCommand('identify'));
    $(idFactory)?.addEventListener('click', () => {
      if (confirm('Factory reset?')) HMWebConfig.sendCommand('factory');
    });
    $(idReboot)?.addEventListener('click', () => {
      if (confirm('Reboot module?')) HMWebConfig.sendCommand('reboot');
    });
  };

  HMWebConfig.appendLog = appendLog;
  HMWebConfig.copyLog = copyLog;
  HMWebConfig.toArray = toArray;
  HMWebConfig.truthy01 = truthy01;

  global.HMWebConfig = HMWebConfig;
})(typeof window !== 'undefined' ? window : globalThis);
