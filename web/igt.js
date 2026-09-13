(() => {
  'use strict';

  const STORAGE_KEY = 'oxypulse.igt.protocols';
  const CHART_MAX_SEC = 4500;
  const O2_MIN = 9;
  const O2_MAX = 19;
  const MIN_MIN = 1;
  const MIN_MAX = 15;
  const CYCLES_MIN = 1;
  const CYCLES_MAX = 12;
  const FETCH_TIMEOUT_MS = 8000;
  const HYPOXIA_COLOR = '#fb923c';
  const NORMOXIA_COLOR = '#86efac';

  const BUILTIN = {
    id: 'daily',
    name: 'Гипоксия/Нормоксия на каждый день',
    o2_target: 15,
    hypoxia_min: 6,
    normoxia_min: 5,
    cycles: 6,
    builtin: true,
  };

  const SESSION_SOURCES = { o2: true, pressure: true, flow: true };

  const deps = () => window.__oxy || {};

  const igtModal = document.getElementById('igtModal');
  const igtModalBackdrop = document.getElementById('igtModalBackdrop');
  const igtModalTitle = document.getElementById('igtModalTitle');
  const btnIgtClose = document.getElementById('btnIgtClose');
  const igtPhaseBar = document.getElementById('igtPhaseBar');
  const igtPhaseBadge = document.getElementById('igtPhaseBadge');
  const igtCountdown = document.getElementById('igtCountdown');
  const igtStatusText = document.getElementById('igtStatusText');
  const igtStepSetup = document.getElementById('igtStepSetup');
  const igtStepSession = document.getElementById('igtStepSession');
  const igtReview = document.getElementById('igtReview');
  const igtReviewText = document.getElementById('igtReviewText');
  const igtComment = document.getElementById('igtComment');
  const igtConfirmAbort = document.getElementById('igtConfirmAbort');
  const igtProtocolSelect = document.getElementById('igtProtocolSelect');
  const igtProtocolMeta = document.getElementById('igtProtocolMeta');
  const igtCustomForm = document.getElementById('igtCustomForm');
  const igtCustomName = document.getElementById('igtCustomName');
  const igtCustomO2 = document.getElementById('igtCustomO2');
  const igtCustomHypoxia = document.getElementById('igtCustomHypoxia');
  const igtCustomNormoxia = document.getElementById('igtCustomNormoxia');
  const igtCustomCycles = document.getElementById('igtCustomCycles');
  const btnIgtCustomNew = document.getElementById('btnIgtCustomNew');
  const btnIgtCustomSave = document.getElementById('btnIgtCustomSave');
  const btnIgtCustomDelete = document.getElementById('btnIgtCustomDelete');
  const igtWarnings = document.getElementById('igtWarnings');
  const igtBlockers = document.getElementById('igtBlockers');
  const igtEmptyHint = document.getElementById('igtEmptyHint');
  const btnIgtPrepare = document.getElementById('btnIgtPrepare');
  const btnIgtStart = document.getElementById('btnIgtStart');
  const btnIgtAbort = document.getElementById('btnIgtAbort');
  const btnIgtSave = document.getElementById('btnIgtSave');
  const btnIgtAgain = document.getElementById('btnIgtAgain');
  const btnIgtAbortYes = document.getElementById('btnIgtAbortYes');
  const btnIgtAbortNo = document.getElementById('btnIgtAbortNo');
  const btnIgtMeasure = document.getElementById('btnIgtMeasure');

  const panels = {
    o2: { el: document.getElementById('igtPanelO2'), canvas: document.getElementById('igtChartO2'), now: document.getElementById('igtO2Now') },
    pressure: { el: document.getElementById('igtPanelPressure'), canvas: document.getElementById('igtChartPressure'), now: document.getElementById('igtPressureNow') },
    flow: { el: document.getElementById('igtPanelFlow'), canvas: document.getElementById('igtChartFlow'), now: document.getElementById('igtFlowNow') },
    flowDyn: { el: document.getElementById('igtPanelFlowDyn'), canvas: document.getElementById('igtChartFlowDyn'), now: document.getElementById('igtFlowDynNow') },
    workO2: { el: document.getElementById('igtPanelWorkO2'), canvas: document.getElementById('igtChartWorkO2'), now: document.getElementById('igtWorkO2Now') },
    workPressure: { el: document.getElementById('igtPanelWorkPressure'), canvas: document.getElementById('igtChartWorkPressure'), now: document.getElementById('igtWorkPressureNow') },
    co2: { el: document.getElementById('igtPanelCo2'), canvas: document.getElementById('igtChartCo2'), now: document.getElementById('igtCo2Now') },
    spo2: { el: document.getElementById('igtPanelSpo2'), canvas: document.getElementById('igtChartSpo2'), now: document.getElementById('igtSpo2Now') },
    hr: { el: document.getElementById('igtPanelHr'), canvas: document.getElementById('igtChartHr'), now: document.getElementById('igtHrNow') },
    rr: { el: document.getElementById('igtPanelRr'), canvas: document.getElementById('igtChartRr'), now: document.getElementById('igtRrNow') },
  };

  let inited = false;
  let uiStep = 'setup';
  let lastIgt = null;
  let selectedId = BUILTIN.id;
  let creatingCustom = false;
  let shown = emptyShown();
  let history = emptyHistory();
  let windowSec = 300;
  let practiceStartedAt = null;
  let sessionEndedAt = null;
  let sourcesOn = false;

  function emptyShown() {
    return {
      o2: true,
      pressure: true,
      flow: true,
      flowDyn: false,
      workO2: false,
      workPressure: false,
      co2: false,
      spo2: false,
      hr: false,
      rr: false,
    };
  }

  function emptyHistory() {
    return {
      t: [],
      o2: [],
      pressure: [],
      flow: [],
      slm: [],
      workO2: [],
      workPressure: [],
      co2: [],
      spo2: [],
      hr: [],
      rr: [],
    };
  }

  function apiUrl(path) {
    return deps().getApiUrl?.(path) || path;
  }

  function fetchWithTimeout(url, opts, ms) {
    if (deps().fetchWithTimeout) return deps().fetchWithTimeout(url, opts, ms);
    return fetch(url, opts);
  }

  function pad2(n) {
    return String(n).padStart(2, '0');
  }

  function formatMmSs(sec) {
    const s = Math.max(0, Math.floor(Number(sec) || 0));
    return `${pad2(Math.floor(s / 60))}:${pad2(s % 60)}`;
  }

  function finiteOrNull(ok, value) {
    if (!ok) return null;
    const n = Number(value);
    return Number.isFinite(n) ? n : null;
  }

  function newId() {
    return `p_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 8)}`;
  }

  function loadCustoms() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      const list = raw ? JSON.parse(raw) : [];
      return Array.isArray(list) ? list.filter((p) => p && p.id && !p.builtin) : [];
    } catch (e) {
      return [];
    }
  }

  function saveCustoms(list) {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
  }

  function allProtocols() {
    return [BUILTIN, ...loadCustoms()];
  }

  function selectedProtocol() {
    return allProtocols().find((p) => p.id === selectedId) || BUILTIN;
  }

  function protocolDurationSec(p) {
    return (Number(p.hypoxia_min) + Number(p.normoxia_min)) * 60 * Number(p.cycles);
  }

  function protocolMeta(p) {
    const total = protocolDurationSec(p);
    return `${p.o2_target}% O₂ · ${p.hypoxia_min} мин гипоксия / ${p.normoxia_min} мин нормоксия × ${p.cycles} = ${formatMmSs(total)}`;
  }

  function validateCustom() {
    const name = (igtCustomName?.value || '').trim();
    const o2 = Number(igtCustomO2?.value);
    const hyp = Number(igtCustomHypoxia?.value);
    const nor = Number(igtCustomNormoxia?.value);
    const cycles = Number(igtCustomCycles?.value);
    if (!name) return 'Укажите название';
    if (!Number.isFinite(o2) || o2 < O2_MIN || o2 > O2_MAX) return `O₂ ${O2_MIN}–${O2_MAX}%`;
    if (!Number.isInteger(hyp) || hyp < MIN_MIN || hyp > MIN_MAX) return `Гипоксия ${MIN_MIN}–${MIN_MAX} мин`;
    if (!Number.isInteger(nor) || nor < MIN_MIN || nor > MIN_MAX) return `Нормоксия ${MIN_MIN}–${MIN_MAX} мин`;
    if (!Number.isInteger(cycles) || cycles < CYCLES_MIN || cycles > CYCLES_MAX) {
      return `Циклы ${CYCLES_MIN}–${CYCLES_MAX}`;
    }
    return null;
  }

  function fillProtocolSelect() {
    if (!igtProtocolSelect) return;
    const list = allProtocols();
    igtProtocolSelect.innerHTML = '';
    for (const p of list) {
      const opt = document.createElement('option');
      opt.value = p.id;
      opt.textContent = p.builtin ? p.name : `${p.name} (свой)`;
      igtProtocolSelect.appendChild(opt);
    }
    if (!list.some((p) => p.id === selectedId)) selectedId = BUILTIN.id;
    igtProtocolSelect.value = selectedId;
    syncProtocolForm();
  }

  function syncProtocolForm() {
    const p = selectedProtocol();
    creatingCustom = !p.builtin && !!igtCustomForm && !igtCustomForm.hidden
      ? creatingCustom
      : false;
    if (igtProtocolMeta) igtProtocolMeta.textContent = protocolMeta(p);
    const showForm = !p.builtin || creatingCustom;
    if (igtCustomForm) igtCustomForm.hidden = !showForm;
    if (showForm) {
      if (igtCustomName) igtCustomName.value = p.builtin ? '' : p.name;
      if (igtCustomO2) igtCustomO2.value = String(p.o2_target);
      if (igtCustomHypoxia) igtCustomHypoxia.value = String(p.hypoxia_min);
      if (igtCustomNormoxia) igtCustomNormoxia.value = String(p.normoxia_min);
      if (igtCustomCycles) igtCustomCycles.value = String(p.cycles);
    }
    if (btnIgtCustomDelete) btnIgtCustomDelete.hidden = p.builtin;
  }

  function sensorsOf(data) {
    const s = data?.sensors || {};
    return {
      hypoxiaO2: (s.hypoxia && s.hypoxia.o2) || s.o2 || {},
      hypoxiaP: (s.hypoxia && s.hypoxia.pressure) || s.pressure || {},
      flow: (s.working && s.working.flow) || s.flow || {},
      workO2: (s.working && s.working.o2) || {},
      workP: (s.working && s.working.pressure) || {},
      co2: (s.working && s.working.co2) || s.co2 || {},
      wellue: s.wellue || {},
      coospo: s.coospo || {},
      bridge: s.bridge || {},
    };
  }

  function blockingErrors(data) {
    const s = sensorsOf(data);
    const errs = [];
    if (!s.hypoxiaO2.ok) errs.push('нет O₂ буфера гипоксии');
    if (!s.hypoxiaP.ok) errs.push('нет давления буфера гипоксии');
    if (!s.flow.ok) errs.push('нет потока рабочего контура');
    return errs;
  }

  function warningItems(data) {
    const s = sensorsOf(data);
    const items = [];
    if (!s.bridge.ok) items.push('UART-мост BLE не на связи');
    if (!s.wellue.ok || !s.wellue.contact) items.push('кольцо Wellue не подключено');
    if (!s.coospo.ok || s.coospo.contact === false) items.push('нагрудный ремень не подключён');
    if (!s.workO2.ok) items.push('нет O₂ рабочего контура');
    if (!s.workP.ok) items.push('нет давления рабочего контура');
    if (!s.co2.ok) items.push('нет CO₂ рабочего контура');
    return items;
  }

  function renderSetupHints(data) {
    const warns = warningItems(data);
    const blocks = blockingErrors(data);
    if (igtWarnings) {
      igtWarnings.innerHTML = '';
      for (const w of warns) {
        const li = document.createElement('li');
        li.textContent = w;
        igtWarnings.appendChild(li);
      }
    }
    if (igtBlockers) {
      igtBlockers.hidden = blocks.length === 0;
      igtBlockers.textContent = blocks.length ? `Нельзя начать подготовку: ${blocks.join(', ')}` : '';
    }
    if (btnIgtPrepare) btnIgtPrepare.disabled = blocks.length > 0 || !deps().getBaseUrl?.();
  }

  function firmwareIdle(igt) {
    return !igt || !igt.state || igt.state === 'idle';
  }

  function firmwarePrep(igt) {
    return ['check_sensors', 'prime', 'primary_fill', 'recirculate', 'ready'].includes(igt?.state);
  }

  function firmwareRun(igt) {
    return igt?.state === 'hypoxia' || igt?.state === 'normoxia';
  }

  function firmwareReview(igt) {
    return ['done', 'aborted', 'error'].includes(igt?.state);
  }

  function isOpen() {
    return !!(igtModal && !igtModal.hidden);
  }

  function isLive() {
    return firmwarePrep(lastIgt) || firmwareRun(lastIgt);
  }

  function captureShown() {
    const src = deps().getTrendSources?.() || {};
    shown = {
      o2: true,
      pressure: true,
      flow: true,
      flowDyn: !!deps().isFlowDynamicsActive?.(),
      workO2: !!src.work_o2,
      workPressure: !!src.work_pressure,
      co2: !!src.co2,
      spo2: !!src.wellue,
      hr: !!src.hr || !!src.wellue,
      rr: !!src.rr,
    };
  }

  function protocolWindowSec(p, igt) {
    const fromIgt = igt && igt.hypoxia_sec && igt.normoxia_sec && igt.cycles
      ? (Number(igt.hypoxia_sec) + Number(igt.normoxia_sec)) * Number(igt.cycles)
      : 0;
    const fromP = p ? protocolDurationSec(p) : 0;
    const total = Math.max(fromIgt, fromP, 300);
    return Math.min(CHART_MAX_SEC, Math.max(300, total + 60));
  }

  function ensureSources() {
    if (sourcesOn) return;
    sourcesOn = true;
    const extra = { ...SESSION_SOURCES };
    const src = deps().getTrendSources?.() || {};
    for (const k of Object.keys(src)) {
      if (src[k]) extra[k] = true;
    }
    deps().enableSessionTrendSources?.(extra);
  }

  function maybeRestoreSources() {
    if (sourcesOn && !isLive()) {
      sourcesOn = false;
      deps().restoreSessionTrendSources?.();
    }
  }

  function pushSample(data) {
    if (!data) return;
    const s = sensorsOf(data);
    const now = Date.now();
    history.t.push(now);
    history.o2.push(finiteOrNull(!!s.hypoxiaO2.ok, s.hypoxiaO2.percent));
    history.pressure.push(finiteOrNull(!!s.hypoxiaP.ok, s.hypoxiaP.hpa));
    history.flow.push(finiteOrNull(!!s.flow.ok, s.flow.ve_lpm));
    history.slm.push(finiteOrNull(!!s.flow.ok, s.flow.slm));
    history.workO2.push(finiteOrNull(!!s.workO2.ok, s.workO2.percent));
    history.workPressure.push(finiteOrNull(!!s.workP.ok, s.workP.hpa));
    const co2Pct = Number.isFinite(Number(s.co2.percent))
      ? Number(s.co2.percent)
      : (Number.isFinite(Number(s.co2.ppm)) ? Number(s.co2.ppm) / 10000 : null);
    history.co2.push(s.co2.ok ? co2Pct : null);
    history.spo2.push(finiteOrNull(!!s.wellue.ok && !!s.wellue.contact, s.wellue.spo2));
    const hr = s.coospo.ok && s.coospo.contact !== false
      ? finiteOrNull(true, s.coospo.bpm)
      : finiteOrNull(!!s.wellue.ok && !!s.wellue.contact, s.wellue.hr);
    history.hr.push(hr);
    history.rr.push(finiteOrNull(!!s.coospo.ok && s.coospo.contact !== false, s.coospo.rr_ms));
    const maxPts = CHART_MAX_SEC + 30;
    if (history.t.length > maxPts) {
      const drop = history.t.length - maxPts;
      for (const key of Object.keys(history)) {
        history[key].splice(0, drop);
      }
    }
  }

  function ptsOf(values) {
    const now = Date.now();
    const from = now - windowSec * 1000;
    const out = [];
    for (let i = 0; i < history.t.length; i += 1) {
      if (history.t[i] >= from) out.push({ t: history.t[i], v: values[i] });
    }
    return out;
  }

  function lastTxt(values, fmt) {
    for (let i = values.length - 1; i >= 0; i -= 1) {
      if (values[i] != null && Number.isFinite(values[i])) return fmt(values[i]);
    }
    return '—';
  }

  function phaseMarkers() {
    const igt = lastIgt;
    if (!igt || !firmwareRun(igt) && !firmwareReview(igt)) return [];
    const nowEsp = Number(deps().getLastStatus?.()?.uptime_ms);
    const marks = [];
    if (Array.isArray(igt.phases) && igt.phases.length && Number.isFinite(nowEsp)) {
      for (const ph of igt.phases) {
        const t = deps().wallFromEsp?.(Number(ph.start_esp_ms), nowEsp);
        if (!Number.isFinite(t)) continue;
        marks.push({
          t,
          color: ph.kind === 'normoxia' ? NORMOXIA_COLOR : HYPOXIA_COLOR,
          width: 1.5,
        });
      }
      return marks;
    }
    const startEsp = Number(igt.practice_started_esp_ms);
    if (!Number.isFinite(startEsp) || startEsp <= 0 || !Number.isFinite(nowEsp)) return marks;
    const startWall = deps().wallFromEsp?.(startEsp, nowEsp);
    const hypMs = Number(igt.hypoxia_sec) * 1000;
    const norMs = Number(igt.normoxia_sec) * 1000;
    const cycles = Number(igt.cycles) || 0;
    for (let i = 0; i < cycles; i += 1) {
      const hypT = startWall + i * (hypMs + norMs);
      const norT = hypT + hypMs;
      if (hypT <= Date.now() + 1000) {
        marks.push({ t: hypT, color: HYPOXIA_COLOR, width: 1.5 });
      }
      if (norT <= Date.now() + 1000) {
        marks.push({ t: norT, color: NORMOXIA_COLOR, width: 1.5 });
      }
    }
    return marks;
  }

  function chartOpts() {
    const origin = practiceStartedAt != null
      ? practiceStartedAt
      : (history.t[0] || Date.now());
    return {
      windowSec,
      now: Date.now(),
      timeGridSec: Math.max(30, Math.round(windowSec / 12)),
      timeOrigin: origin,
      markers: phaseMarkers(),
    };
  }

  function drawOne(key, series, fallback, emptyText, extraOpts) {
    const panel = panels[key];
    if (!panel?.el || !shown[key]) {
      if (panel?.el) panel.el.hidden = true;
      return false;
    }
    panel.el.hidden = false;
    deps().drawTrendChart?.(panel.canvas, series, fallback, emptyText, {
      ...chartOpts(),
      ...(extraOpts || {}),
    });
    return true;
  }

  function drawCharts() {
    if (!isOpen() || uiStep === 'setup') return;
    let any = false;
    any = drawOne('o2', [{ pts: ptsOf(history.o2), color: '#22d3ee' }],
      { min: 12, max: 22, format: (v) => v.toFixed(1) }, 'нет O₂') || any;
    if (panels.o2.now) panels.o2.now.textContent = lastTxt(history.o2, (v) => `${v.toFixed(1)} %`);

    const pBase = deps().getPressureBaseline?.();
    any = drawOne('pressure', [{ pts: ptsOf(history.pressure), color: '#e879f9' }],
      { min: 980, max: 1040, format: (v) => v.toFixed(1) }, 'нет давления',
      Number.isFinite(pBase) ? { refLine: pBase } : {}) || any;
    if (panels.pressure.now) {
      panels.pressure.now.textContent = lastTxt(history.pressure, (v) => `${v.toFixed(1)} hPa`);
    }

    any = drawOne('flow', [{ pts: ptsOf(history.flow), color: '#a78bfa' }],
      { min: 0, max: 30, format: (v) => v.toFixed(1) }, 'нет данных') || any;
    if (panels.flow.now) panels.flow.now.textContent = lastTxt(history.flow, (v) => `${v.toFixed(1)} л/мин`);

    any = drawOne('flowDyn', [{ pts: ptsOf(history.slm), color: '#a78bfa' }],
      { min: -15, max: 15, format: (v) => v.toFixed(1) }, 'нет потока', { zeroLine: true }) || any;
    if (panels.flowDyn.now) panels.flowDyn.now.textContent = lastTxt(history.slm, (v) => `${v.toFixed(1)} л/мин`);

    any = drawOne('workO2', [{ pts: ptsOf(history.workO2), color: '#67e8f9' }],
      { min: 18, max: 22, format: (v) => v.toFixed(1) }, 'нет O₂') || any;
    if (panels.workO2.now) panels.workO2.now.textContent = lastTxt(history.workO2, (v) => `${v.toFixed(1)} %`);

    const wpBase = deps().getWorkPressureBaseline?.();
    any = drawOne('workPressure', [{ pts: ptsOf(history.workPressure), color: '#f0abfc' }],
      { min: 980, max: 1040, format: (v) => v.toFixed(1) }, 'нет давления',
      Number.isFinite(wpBase) ? { refLine: wpBase } : {}) || any;
    if (panels.workPressure.now) {
      panels.workPressure.now.textContent = lastTxt(history.workPressure, (v) => `${v.toFixed(1)} hPa`);
    }

    any = drawOne('co2', [{ pts: ptsOf(history.co2), color: '#60a5fa' }],
      { min: 0.04, max: 0.2, format: (v) => v.toFixed(2) }, 'нет CO₂') || any;
    if (panels.co2.now) panels.co2.now.textContent = lastTxt(history.co2, (v) => `${v.toFixed(2)} %`);

    any = drawOne('spo2', [{ pts: ptsOf(history.spo2), color: '#4ade80' }],
      { min: 90, max: 100, format: (v) => v.toFixed(0) }, 'нет SpO₂') || any;
    if (panels.spo2.now) panels.spo2.now.textContent = lastTxt(history.spo2, (v) => `${v.toFixed(0)} %`);

    any = drawOne('hr', [{ pts: ptsOf(history.hr), color: '#f87171' }],
      { min: 50, max: 140, format: (v) => v.toFixed(0) }, 'нет ЧСС') || any;
    if (panels.hr.now) panels.hr.now.textContent = lastTxt(history.hr, (v) => `${v.toFixed(0)}`);

    any = drawOne('rr', [{ pts: ptsOf(history.rr), color: '#f87171' }],
      { min: 400, max: 1200, format: (v) => v.toFixed(0) }, 'нет R-R') || any;
    if (panels.rr.now) panels.rr.now.textContent = lastTxt(history.rr, (v) => `${v.toFixed(0)} мс`);

    if (igtEmptyHint) igtEmptyHint.hidden = any;
  }

  function wrapTextLines(ctx, text, maxW) {
    const words = String(text).split(/\s+/).filter(Boolean);
    const lines = [];
    let line = '';
    for (const word of words) {
      const next = line ? `${line} ${word}` : word;
      if (ctx.measureText(next).width > maxW && line) {
        lines.push(line);
        line = word;
      } else {
        line = next;
      }
    }
    if (line) lines.push(line);
    return lines;
  }

  function saveImage() {
    if (history.t.length === 0) {
      if (igtReviewText) igtReviewText.textContent = 'Нечего сохранять — нет точек';
      return;
    }
    const igt = lastIgt || {};
    const p = selectedProtocol();
    const name = igt.name || p.name;
    const stampDate = new Date();
    const pad = 16;
    const headerH = 96;
    const metaH = 26;
    const plotH = 112;
    const panelGap = 12;
    const dpr = 2;
    const exportW = 720;
    const contentW = exportW - pad * 2;
    const comment = igtComment?.value?.trim() || '';
    const measure = document.createElement('canvas').getContext('2d');
    measure.font = '15px system-ui, sans-serif';
    const commentLines = comment ? wrapTextLines(measure, comment, contentW) : [];
    const commentH = commentLines.length ? 16 + commentLines.length * 22 : 0;

    const specs = [];
    const add = (key, title, nowText, series, fallback, emptyText, extraOpts) => {
      if (!shown[key]) return;
      specs.push({ title, nowText, series, fallback, emptyText, extraOpts });
    };
    add('o2', 'O₂ · гипоксия', panels.o2.now?.textContent || '—',
      [{ pts: ptsOf(history.o2), color: '#22d3ee' }],
      { min: 12, max: 22, format: (v) => v.toFixed(1) }, 'нет O₂');
    add('pressure', 'Давление · гипоксия', panels.pressure.now?.textContent || '—',
      [{ pts: ptsOf(history.pressure), color: '#e879f9' }],
      { min: 980, max: 1040, format: (v) => v.toFixed(1) }, 'нет давления');
    add('flow', 'Минутная вентиляция', panels.flow.now?.textContent || '—',
      [{ pts: ptsOf(history.flow), color: '#a78bfa' }],
      { min: 0, max: 30, format: (v) => v.toFixed(1) }, 'нет данных');
    add('flowDyn', 'Поток', panels.flowDyn.now?.textContent || '—',
      [{ pts: ptsOf(history.slm), color: '#a78bfa' }],
      { min: -15, max: 15, format: (v) => v.toFixed(1) }, 'нет потока', { zeroLine: true });
    add('workO2', 'O₂ · рабочий', panels.workO2.now?.textContent || '—',
      [{ pts: ptsOf(history.workO2), color: '#67e8f9' }],
      { min: 18, max: 22, format: (v) => v.toFixed(1) }, 'нет O₂');
    add('workPressure', 'Давление · рабочий', panels.workPressure.now?.textContent || '—',
      [{ pts: ptsOf(history.workPressure), color: '#f0abfc' }],
      { min: 980, max: 1040, format: (v) => v.toFixed(1) }, 'нет давления');
    add('co2', 'CO₂ · рабочий', panels.co2.now?.textContent || '—',
      [{ pts: ptsOf(history.co2), color: '#60a5fa' }],
      { min: 0.04, max: 0.2, format: (v) => v.toFixed(2) }, 'нет CO₂');
    add('spo2', 'Сатурация (кольцо)', panels.spo2.now?.textContent || '—',
      [{ pts: ptsOf(history.spo2), color: '#4ade80' }],
      { min: 90, max: 100, format: (v) => v.toFixed(0) }, 'нет SpO₂');
    add('hr', 'HR', panels.hr.now?.textContent || '—',
      [{ pts: ptsOf(history.hr), color: '#f87171' }],
      { min: 50, max: 140, format: (v) => v.toFixed(0) }, 'нет ЧСС');
    add('rr', 'R-R', panels.rr.now?.textContent || '—',
      [{ pts: ptsOf(history.rr), color: '#f87171' }],
      { min: 400, max: 1200, format: (v) => v.toFixed(0) }, 'нет R-R');

    if (!specs.length) {
      if (igtReviewText) igtReviewText.textContent = 'Нечего сохранять — нет графиков';
      return;
    }

    const exportH = pad + headerH + specs.length * (metaH + plotH)
      + Math.max(0, specs.length - 1) * panelGap + commentH + pad;
    const canvas = document.createElement('canvas');
    const { ctx, w, h } = deps().setupCanvas(canvas, exportW, exportH, dpr);
    ctx.fillStyle = '#1c1c1c';
    ctx.fillRect(0, 0, w, h);
    let y = pad;
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 18px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText('OxyPulse · ИГТ', pad, y);
    y += 26;
    ctx.fillStyle = '#fb923c';
    ctx.font = 'bold 14px system-ui, sans-serif';
    ctx.fillText(name, pad, y);
    y += 22;
    const outcome = igt.state === 'aborted' ? 'прервано'
      : igt.state === 'error' ? `ошибка: ${igt.error || ''}`
        : 'завершено';
    ctx.fillStyle = '#888888';
    ctx.font = '13px system-ui, sans-serif';
    const stamp = deps().trendsExportStamp?.(stampDate) || stampDate.toLocaleString();
    ctx.fillText(`${protocolMeta({
      o2_target: igt.o2_target || p.o2_target,
      hypoxia_min: (igt.hypoxia_sec || p.hypoxia_min * 60) / 60,
      normoxia_min: (igt.normoxia_sec || p.normoxia_min * 60) / 60,
      cycles: igt.cycles || p.cycles,
    })} · ${outcome} · ${stamp}`, pad, y);
    y += headerH - 48;

    const opts = { ...chartOpts(), w: contentW, h: plotH, dpr };
    for (const spec of specs) {
      ctx.fillStyle = '#dddddd';
      ctx.font = 'bold 14px system-ui, sans-serif';
      ctx.textAlign = 'left';
      ctx.fillText(spec.title, pad, y);
      ctx.fillStyle = '#aaaaaa';
      ctx.font = '13px system-ui, sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(spec.nowText, exportW - pad, y);
      y += metaH;
      const plotCanvas = document.createElement('canvas');
      deps().drawTrendChart?.(plotCanvas, spec.series, spec.fallback, spec.emptyText, {
        ...opts,
        ...(spec.extraOpts || {}),
      });
      ctx.drawImage(plotCanvas, pad, y, contentW, plotH);
      y += plotH + panelGap;
    }
    if (commentLines.length) {
      ctx.fillStyle = '#dddddd';
      ctx.font = '15px system-ui, sans-serif';
      ctx.textAlign = 'left';
      for (const line of commentLines) {
        ctx.fillText(line, pad, y);
        y += 22;
      }
    }
    const slug = String(name).toLowerCase().replace(/[^\p{L}\p{N}]+/gu, '_').replace(/^_|_$/g, '').slice(0, 40);
    const fileStamp = deps().trendsFilenameStamp?.(stampDate) || String(Date.now());
    canvas.toBlob((blob) => {
      if (!blob) return;
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `oxypulse_igt_${slug || 'session'}_${fileStamp}.png`;
      link.click();
      URL.revokeObjectURL(url);
    }, 'image/png');
  }

  function statusLine(igt, data) {
    if (!igt) return '';
    const s = sensorsOf(data);
    const o2 = s.hypoxiaO2.ok ? `${Number(s.hypoxiaO2.percent).toFixed(1)}%` : '—';
    const p = s.hypoxiaP.ok ? `${Number(s.hypoxiaP.hpa).toFixed(1)} hPa` : '—';
    switch (igt.state) {
      case 'check_sensors':
        return 'Проверка датчиков…';
      case 'prime':
        return `Ждём падение давления на порог из конфига · сейчас ${p}`;
      case 'primary_fill':
        return `Первичное наполнение буфера · осталось ${formatMmSs(igt.phase_remain_sec)}`;
      case 'recirculate':
        return `Рециркуляция ${o2} → ${Number(igt.o2_target).toFixed(1)}%`;
      case 'ready':
        return `Буфер готов (${o2}). Можно начинать практику.`;
      case 'hypoxia':
        return igt.sensor_warn ? 'Гипоксия · нет части датчиков, фазы идут по таймеру' : 'Фаза гипоксии';
      case 'normoxia': {
        if (igt.waiting_buffer) {
          return 'Нормоксия завершена · ждём готовности буфера гипоксии';
        }
        const bp = igt.buffer_prep || 'idle';
        if (bp === 'prime') {
          return `Нормоксия · опорожнение буфера (давление ${p})`;
        }
        if (bp === 'primary_fill') {
          return `Нормоксия · наполнение буфера · ${formatMmSs(igt.buffer_prep_remain_sec)}`;
        }
        if (bp === 'recirculate') {
          return `Нормоксия · рециркуляция ${o2} → ${Number(igt.o2_target).toFixed(1)}%`;
        }
        if (bp === 'ready' && Number(igt.cycle) < Number(igt.cycles)) {
          return 'Нормоксия · буфер гипоксии готов к следующему циклу';
        }
        return igt.sensor_warn
          ? 'Нормоксия · нет части датчиков, фазы идут по таймеру'
          : 'Фаза нормоксии';
      }
      case 'done':
        return 'Протокол завершён';
      case 'aborted':
        return 'Практика прервана';
      case 'error':
        return igt.error || 'Ошибка подготовки';
      default:
        return '';
    }
  }

  function applyChrome(igt, data) {
    lastIgt = igt || lastIgt;
    const state = lastIgt?.state || 'idle';
    if (igtModalTitle) {
      const name = lastIgt?.name || selectedProtocol().name;
      igtModalTitle.textContent = firmwareIdle(lastIgt) ? 'ИГТ' : `ИГТ · ${name}`;
    }

    if (firmwareIdle(lastIgt) && uiStep !== 'setup') {
      // keep review until ack; idle after ack goes setup
    }

    if (firmwarePrep(lastIgt)) uiStep = 'prep';
    else if (firmwareRun(lastIgt)) uiStep = 'run';
    else if (firmwareReview(lastIgt)) uiStep = 'review';
    else if (firmwareIdle(lastIgt)) uiStep = 'setup';

    if (igtStepSetup) igtStepSetup.hidden = uiStep !== 'setup';
    if (igtStepSession) igtStepSession.hidden = uiStep === 'setup';
    if (igtReview) igtReview.hidden = uiStep !== 'review';
    if (igtPhaseBar) igtPhaseBar.hidden = uiStep === 'setup';
    if (igtConfirmAbort) igtConfirmAbort.hidden = igtConfirmAbort.hidden;

    if (uiStep === 'setup') {
      renderSetupHints(data || deps().getLastStatus?.());
    }

    if (igtPhaseBadge) {
      igtPhaseBadge.className = 'igt-phase-badge';
      if (uiStep === 'prep') {
        igtPhaseBadge.textContent = 'Подготовка';
        igtPhaseBadge.classList.add('prep');
      } else if (state === 'hypoxia') {
        igtPhaseBadge.textContent = `Гипоксия ${lastIgt.cycle}/${lastIgt.cycles}`;
        igtPhaseBadge.classList.add('hypoxia');
      } else if (state === 'normoxia') {
        igtPhaseBadge.textContent = `Нормоксия ${lastIgt.cycle}/${lastIgt.cycles}`;
        igtPhaseBadge.classList.add('normoxia');
      } else if (state === 'done') {
        igtPhaseBadge.textContent = 'Завершено';
      } else if (state === 'aborted') {
        igtPhaseBadge.textContent = 'Прервано';
      } else if (state === 'error') {
        igtPhaseBadge.textContent = 'Ошибка';
      } else {
        igtPhaseBadge.textContent = '—';
      }
    }
    if (igtCountdown) {
      if (uiStep === 'prep' || uiStep === 'run') {
        if (lastIgt?.ready_to_start) {
          igtCountdown.textContent = 'готово';
        } else if (lastIgt?.waiting_buffer) {
          igtCountdown.textContent = 'буфер';
        } else {
          igtCountdown.textContent = formatMmSs(lastIgt?.phase_remain_sec);
        }
      } else {
        igtCountdown.textContent = '—';
      }
    }
    if (igtStatusText) igtStatusText.textContent = statusLine(lastIgt, data || deps().getLastStatus?.());
    if (igtReviewText && uiStep === 'review') {
      igtReviewText.textContent = statusLine(lastIgt, data || deps().getLastStatus?.());
    }

    if (firmwareRun(lastIgt) && practiceStartedAt == null) {
      const nowEsp = Number((data || deps().getLastStatus?.())?.uptime_ms);
      const startEsp = Number(lastIgt.practice_started_esp_ms);
      practiceStartedAt = Number.isFinite(startEsp) && startEsp > 0
        ? (deps().wallFromEsp?.(startEsp, nowEsp) ?? Date.now())
        : Date.now();
      windowSec = protocolWindowSec(selectedProtocol(), lastIgt);
    }
    if (firmwareReview(lastIgt) && sessionEndedAt == null) {
      sessionEndedAt = Date.now();
    }

    if (btnIgtPrepare) btnIgtPrepare.hidden = uiStep !== 'setup';
    if (btnIgtStart) {
      btnIgtStart.hidden = uiStep !== 'prep';
      btnIgtStart.disabled = !lastIgt?.ready_to_start;
    }
    if (btnIgtAbort) btnIgtAbort.hidden = !(uiStep === 'prep' || uiStep === 'run');
    if (btnIgtSave) btnIgtSave.hidden = uiStep !== 'review';
    if (btnIgtAgain) btnIgtAgain.hidden = uiStep !== 'review';
    if (igtComment) igtComment.hidden = uiStep !== 'review';
  }

  async function postIgt(path, body) {
    const res = await fetchWithTimeout(apiUrl(path), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : '{}',
    }, FETCH_TIMEOUT_MS);
    if (!res.ok) {
      let err = `HTTP ${res.status}`;
      try {
        const j = await res.json();
        if (j?.error) err = j.error;
      } catch (e) { /* ignore */ }
      throw new Error(err);
    }
  }

  async function fetchIgtFull() {
    try {
      const res = await fetchWithTimeout(apiUrl('/api/igt'), {}, FETCH_TIMEOUT_MS);
      if (!res.ok) return null;
      return res.json();
    } catch (e) {
      return null;
    }
  }

  async function startPrep() {
    const formOpen = !!(igtCustomForm && !igtCustomForm.hidden);
    const p = selectedProtocol();
    const err = formOpen ? validateCustom() : null;
    if (err) {
      if (igtBlockers) {
        igtBlockers.hidden = false;
        igtBlockers.textContent = err;
      }
      return;
    }
    const proto = formOpen ? {
      name: igtCustomName.value.trim(),
      o2_target: Number(igtCustomO2.value),
      hypoxia_min: Number(igtCustomHypoxia.value),
      normoxia_min: Number(igtCustomNormoxia.value),
      cycles: Number(igtCustomCycles.value),
    } : p;
    history = emptyHistory();
    practiceStartedAt = null;
    sessionEndedAt = null;
    captureShown();
    windowSec = protocolWindowSec(proto, null);
    ensureSources();
    try {
      await postIgt('/api/igt/prep', {
        name: proto.name,
        o2_target: proto.o2_target,
        hypoxia_sec: proto.hypoxia_min * 60,
        normoxia_sec: proto.normoxia_min * 60,
        cycles: proto.cycles,
      });
      uiStep = 'prep';
      applyChrome({
        state: 'check_sensors',
        name: proto.name,
        o2_target: proto.o2_target,
        hypoxia_sec: proto.hypoxia_min * 60,
        normoxia_sec: proto.normoxia_min * 60,
        cycles: proto.cycles,
      }, deps().getLastStatus?.());
    } catch (e) {
      if (igtBlockers) {
        igtBlockers.hidden = false;
        igtBlockers.textContent = `Подготовка: ${e.message}`;
      }
    }
  }

  async function startPractice() {
    try {
      await postIgt('/api/igt/start');
      practiceStartedAt = Date.now();
      sessionEndedAt = null;
      history = emptyHistory();
      uiStep = 'run';
    } catch (e) {
      if (igtStatusText) igtStatusText.textContent = `Старт: ${e.message}`;
    }
  }

  async function abortPractice() {
    try {
      await postIgt('/api/igt/abort');
      if (igtConfirmAbort) igtConfirmAbort.hidden = true;
    } catch (e) {
      if (igtStatusText) igtStatusText.textContent = `Прерывание: ${e.message}`;
    }
  }

  async function ackAndReset() {
    try {
      if (firmwareReview(lastIgt)) await postIgt('/api/igt/ack');
    } catch (e) { /* already idle */ }
    lastIgt = { state: 'idle' };
    uiStep = 'setup';
    history = emptyHistory();
    practiceStartedAt = null;
    sessionEndedAt = null;
    if (igtComment) igtComment.value = '';
    if (igtConfirmAbort) igtConfirmAbort.hidden = true;
    maybeRestoreSources();
    applyChrome(lastIgt, deps().getLastStatus?.());
    renderSetupHints(deps().getLastStatus?.());
  }

  function open() {
    if (!deps().getBaseUrl?.()) {
      alert('Сначала подключитесь к ESP32');
      return;
    }
    if (!igtModal || isOpen()) return;
    deps().closeOtherActionModals?.();
    deps().cancelMainChartRedraw?.();
    fillProtocolSelect();
    captureShown();
    if (igtComment) igtComment.value = '';
    if (igtConfirmAbort) igtConfirmAbort.hidden = true;
    igtModal.hidden = false;
    igtModal.setAttribute('aria-hidden', 'false');
    const data = deps().getLastStatus?.();
    lastIgt = data?.igt || { state: 'idle' };
    if (!firmwareIdle(lastIgt)) {
      ensureSources();
      windowSec = protocolWindowSec(selectedProtocol(), lastIgt);
      fetchIgtFull().then((full) => {
        if (full) lastIgt = { ...lastIgt, ...full };
        applyChrome(lastIgt, data);
        drawCharts();
      });
    } else {
      uiStep = 'setup';
    }
    applyChrome(lastIgt, data);
    requestAnimationFrame(() => drawCharts());
  }

  function close() {
    if (!igtModal) return;
    igtModal.hidden = true;
    igtModal.setAttribute('aria-hidden', 'true');
    if (igtConfirmAbort) igtConfirmAbort.hidden = true;
    maybeRestoreSources();
    deps().resumeMainCharts?.();
  }

  function onStatus(data) {
    const igt = data?.igt;
    if (igt) lastIgt = { ...lastIgt, ...igt };
    if (isOpen() && (uiStep === 'prep' || uiStep === 'run' || firmwarePrep(lastIgt) || firmwareRun(lastIgt))) {
      pushSample(data);
    }
    if (isOpen()) {
      applyChrome(lastIgt, data);
      drawCharts();
    }
  }

  function init() {
    if (inited) return;
    inited = true;
    fillProtocolSelect();

    igtProtocolSelect?.addEventListener('change', () => {
      selectedId = igtProtocolSelect.value;
      creatingCustom = false;
      syncProtocolForm();
    });
    btnIgtCustomNew?.addEventListener('click', () => {
      creatingCustom = true;
      selectedId = BUILTIN.id;
      if (igtProtocolSelect) igtProtocolSelect.value = BUILTIN.id;
      if (igtCustomForm) igtCustomForm.hidden = false;
      if (igtCustomName) igtCustomName.value = '';
      if (btnIgtCustomDelete) btnIgtCustomDelete.hidden = true;
    });
    btnIgtCustomSave?.addEventListener('click', () => {
      const err = validateCustom();
      if (err) {
        if (igtBlockers) {
          igtBlockers.hidden = false;
          igtBlockers.textContent = err;
        }
        return;
      }
      const list = loadCustoms();
      const current = selectedProtocol();
      const item = {
        id: current.builtin ? newId() : current.id,
        name: igtCustomName.value.trim(),
        o2_target: Number(igtCustomO2.value),
        hypoxia_min: Number(igtCustomHypoxia.value),
        normoxia_min: Number(igtCustomNormoxia.value),
        cycles: Number(igtCustomCycles.value),
      };
      const idx = list.findIndex((p) => p.id === item.id);
      if (idx >= 0) list[idx] = item;
      else list.push(item);
      saveCustoms(list);
      selectedId = item.id;
      creatingCustom = false;
      fillProtocolSelect();
    });
    btnIgtCustomDelete?.addEventListener('click', () => {
      const p = selectedProtocol();
      if (p.builtin) return;
      saveCustoms(loadCustoms().filter((x) => x.id !== p.id));
      selectedId = BUILTIN.id;
      fillProtocolSelect();
    });
    btnIgtPrepare?.addEventListener('click', () => startPrep());
    btnIgtStart?.addEventListener('click', () => startPractice());
    btnIgtAbort?.addEventListener('click', () => {
      if (igtConfirmAbort) igtConfirmAbort.hidden = false;
    });
    btnIgtAbortYes?.addEventListener('click', () => abortPractice());
    btnIgtAbortNo?.addEventListener('click', () => {
      if (igtConfirmAbort) igtConfirmAbort.hidden = true;
    });
    btnIgtSave?.addEventListener('click', () => saveImage());
    btnIgtAgain?.addEventListener('click', () => ackAndReset());
    btnIgtClose?.addEventListener('click', () => close());
    igtModalBackdrop?.addEventListener('click', () => close());
  }

  window.OxyIgt = {
    init,
    open,
    close,
    isOpen,
    isLive,
    onStatus,
    onResize() {
      if (isOpen()) drawCharts();
    },
  };
})();
