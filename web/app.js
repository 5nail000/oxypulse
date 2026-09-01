const DEFAULT_PINS = [13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];

const POLL_MS_BASE = 1000;
const POLL_MS_HEAVY = 1500;
const FETCH_TIMEOUT_MS = 6000;
const WIFI_SCAN_POLL_MS = 800;
const WIFI_SCAN_MAX_POLLS = 24;
const WIFI_SCAN_FETCH_TIMEOUT_MS = 8000;
const STALE_AFTER_MS = 7000;
const CHART_REDRAW_MS_LIGHT = 400;
const CHART_REDRAW_MS_HEAVY = 800;
const GPIO_TAP_PULSE_MS = 250;
const GPIO_TAP_COOLDOWN_MS = 6000;
const CHART_MAX_SEC = 4500;
const TRENDS_FETCH_TIMEOUT_MS = 5000;
const TRENDS_PAGE = 240;
const TRENDS_HYDRATE_MAX_PAGES = 32;
const TRENDS_BACKFILL_MIN_GAP_MS = 2500;
const TRENDS_BACKFILL_DEBOUNCE_MS = 3000;
const TRENDS_BACKFILL_MAX_PAGES = 8;
const TREND_EXPORT_W = 960;
const TREND_EXPORT_PAD = 16;
const TREND_EXPORT_PLOT_H = 148;
const CP_WINDOW_INITIAL_SEC = 60;
const CP_WINDOW_EXPAND_SEC = 15;
const CP_EXPAND_LEAD_SEC = 5;
const CP_AFTERGLOW_SEC = 45;
const PRACTICE_WINDOW_INITIAL_SEC = 120;
const PRACTICE_EXPAND_FILL = 0.75;
const PRACTICE_EXPAND_FACTOR = 1.5;
const PRACTICE_AFTERGLOW_SEC = 60;
const PRACTICE_MARK_SEC = 300;
const PRACTICE_MARK_BRIGHT_SEC = 600;
const HR_VIEW_BELT_KEY = 'oxypulse.hrView.belt';
const HR_VIEW_WELLUE_KEY = 'oxypulse.hrView.wellue';
const FLOW_DYNAMICS_KEY = 'oxypulse.flowDynamics';
const FLOW_DYNAMICS_POLL_MS = 250;
const FLOW_DYNAMICS_GAP_MS = FLOW_DYNAMICS_POLL_MS * 4;
const CHART_PAD = { top: 8, right: 8, bottom: 22, left: 42 };

const hostInput = document.getElementById('host');
const connStatus = document.getElementById('connStatus');
const uptimeEl = document.getElementById('uptime');
const linkBar = document.getElementById('linkBar');
const linkText = document.getElementById('linkText');
const linkMeta = document.getElementById('linkMeta');
const gpioPin = document.getElementById('gpioPin');
const servoPin = document.getElementById('servoPin');
const gpioState = document.getElementById('gpioState');
const gpioStateLabel = document.getElementById('gpioStateLabel');
const servoAngle = document.getElementById('servoAngle');
const angleValue = document.getElementById('angleValue');
const servoAuto = document.getElementById('servoAuto');
const servoStatus = document.getElementById('servoStatus');
const btnGpioApply = document.getElementById('btnGpioApply');
const btnGpioTap = document.getElementById('btnGpioTap');
const btnServoApply = document.getElementById('btnServoApply');
const btnFlowReset = document.getElementById('btnFlowReset');
const btnFlowDynamics = document.getElementById('btnFlowDynamics');
const btnCo2Frc = document.getElementById('btnCo2Frc');
const trendsHint = document.getElementById('trendsHint');
const btnTrendsSave = document.getElementById('btnTrendsSave');
const chartO2 = document.getElementById('chartO2');
const chartWorkO2 = document.getElementById('chartWorkO2');
const chartFlow = document.getElementById('chartFlow');
const chartFlowDynamics = document.getElementById('chartFlowDynamics');
const chartPressure = document.getElementById('chartPressure');
const chartWorkPressure = document.getElementById('chartWorkPressure');
const chartSpo2 = document.getElementById('chartSpo2');
const chartHr = document.getElementById('chartHr');
const chartCo2 = document.getElementById('chartCo2');
const chartRr = document.getElementById('chartRr');
const chartO2Now = document.getElementById('chartO2Now');
const chartWorkO2Now = document.getElementById('chartWorkO2Now');
const chartFlowNow = document.getElementById('chartFlowNow');
const chartFlowDynamicsNow = document.getElementById('chartFlowDynamicsNow');
const chartPressureNow = document.getElementById('chartPressureNow');
const chartWorkPressureNow = document.getElementById('chartWorkPressureNow');
const chartSpo2Now = document.getElementById('chartSpo2Now');
const chartHrNow = document.getElementById('chartHrNow');
const chartCo2Now = document.getElementById('chartCo2Now');
const chartRrNow = document.getElementById('chartRrNow');
const hrBeltTitle = document.getElementById('hrBeltTitle');
const hrLegendBelt = document.getElementById('hrLegendBelt');
const hrLegendWellue = document.getElementById('hrLegendWellue');
const hrLegendBeltWrap = document.getElementById('hrLegendBeltWrap');
const hrLegendWellueWrap = document.getElementById('hrLegendWellueWrap');
const hrViewBeltInput = document.getElementById('hrViewBelt');
const hrViewWellueInput = document.getElementById('hrViewWellue');
const btnWifiSettings = document.getElementById('btnWifiSettings');
const wifiModal = document.getElementById('wifiModal');
const wifiModalBackdrop = document.getElementById('wifiModalBackdrop');
const btnWifiClose = document.getElementById('btnWifiClose');
const wifiModalStatus = document.getElementById('wifiModalStatus');
const btnWifiScan = document.getElementById('btnWifiScan');
const wifiScanStatus = document.getElementById('wifiScanStatus');
const wifiNetworkList = document.getElementById('wifiNetworkList');
const wifiSsidManual = document.getElementById('wifiSsidManual');
const wifiPassword = document.getElementById('wifiPassword');
const btnWifiSave = document.getElementById('btnWifiSave');
const btnWifiForget = document.getElementById('btnWifiForget');
const wifiLanHint = document.getElementById('wifiLanHint');
const btnHrvMeasure = document.getElementById('btnHrvMeasure');
const btnCpMeasure = document.getElementById('btnCpMeasure');
const actionsHint = document.getElementById('actionsHint');
const cpModal = document.getElementById('cpModal');
const cpModalBackdrop = document.getElementById('cpModalBackdrop');
const btnCpClose = document.getElementById('btnCpClose');
const cpTimerWrap = document.getElementById('cpTimerWrap');
const cpTimer = document.getElementById('cpTimer');
const cpStatus = document.getElementById('cpStatus');
const cpChartSpo2 = document.getElementById('cpChartSpo2');
const cpChartHr = document.getElementById('cpChartHr');
const cpSpo2Now = document.getElementById('cpSpo2Now');
const cpHrNow = document.getElementById('cpHrNow');
const btnCpFinish = document.getElementById('btnCpFinish');
const btnCpSave = document.getElementById('btnCpSave');
const btnCpStart = document.getElementById('btnCpStart');
const btnPracticeMeasure = document.getElementById('btnPracticeMeasure');
const practiceModal = document.getElementById('practiceModal');
const practiceModalBackdrop = document.getElementById('practiceModalBackdrop');
const btnPracticeClose = document.getElementById('btnPracticeClose');
const practiceTimerWrap = document.getElementById('practiceTimerWrap');
const practiceTimer = document.getElementById('practiceTimer');
const practiceAfterglow = document.getElementById('practiceAfterglow');
const practiceEmptyHint = document.getElementById('practiceEmptyHint');
const practiceChartSpo2 = document.getElementById('practiceChartSpo2');
const practiceChartHr = document.getElementById('practiceChartHr');
const practiceChartRr = document.getElementById('practiceChartRr');
const practiceChartVe = document.getElementById('practiceChartVe');
const practiceChartFlow = document.getElementById('practiceChartFlow');
const practiceChartO2 = document.getElementById('practiceChartO2');
const practiceChartCo2 = document.getElementById('practiceChartCo2');
const practiceSpo2Now = document.getElementById('practiceSpo2Now');
const practiceHrNow = document.getElementById('practiceHrNow');
const practiceRrNow = document.getElementById('practiceRrNow');
const practiceVeNow = document.getElementById('practiceVeNow');
const practiceFlowNow = document.getElementById('practiceFlowNow');
const practiceO2Now = document.getElementById('practiceO2Now');
const practiceCo2Now = document.getElementById('practiceCo2Now');
const practiceHrLegendBelt = document.getElementById('practiceHrLegendBelt');
const practiceHrLegendBeltWrap = document.getElementById('practiceHrLegendBeltWrap');
const practiceHrLegendWellueWrap = document.getElementById('practiceHrLegendWellueWrap');
const practiceCo2LegendWrap = document.getElementById('practiceCo2LegendWrap');
const btnPracticeStart = document.getElementById('btnPracticeStart');
const btnPracticeFinish = document.getElementById('btnPracticeFinish');
const btnPracticeSave = document.getElementById('btnPracticeSave');
const btnPracticeRepeat = document.getElementById('btnPracticeRepeat');
const hrvModal = document.getElementById('hrvModal');

const formControls = [gpioPin, servoPin, gpioState, servoAngle, servoAuto];

let baseUrl = '';
let pollTimer = null;
let chartRedrawTimer = null;
let lastChartDrawMs = 0;
let linkUiTimer = null;
let lastStatus = null;
let lastPinsKey = '';
let lastOkAt = null;
let lastUptimeMs = null;
let lastError = '';
let lastPollFailed = true;
let pollInFlight = false;
let statusPollPaused = false;
let hydrateInFlight = false;
let hydrateOnNextOk = true;
let lastHydrateAt = 0;
let backfillPending = false;
let sessionSourcesSaved = null;
let sourcesWriteChain = Promise.resolve();
let linkState = 'idle';
let pollingActive = false;
let formLocked = false;
let gpioSyncLocked = false;
let chartWindowSec = 300;
let selectedWifi = null;
let beltDisplayName = 'Нагрудный HR';
let cpUi = 'idle';
let cpStartedAt = null;
let cpStopAt = null;
let cpFrozenAt = null;
let cpDurationSec = 0;
let cpWindowSec = CP_WINDOW_INITIAL_SEC;
let cpTicker = null;
const cpHistory = { t: [], espMs: [], spo2: [], hrWellue: [], hrCoospo: [] };
let practiceUi = 'idle';
let practiceStartedAt = null;
let practiceStopAt = null;
let practiceFrozenAt = null;
let practiceDurationSec = 0;
let practiceWindowSec = PRACTICE_WINDOW_INITIAL_SEC;
let practiceTicker = null;
let practiceHrBelt = false;
const practiceShown = {
  spo2: false,
  hr: false,
  rr: false,
  ve: false,
  flow: false,
  o2: false,
  co2: false,
};
const practiceHistory = {
  t: [],
  espMs: [],
  spo2: [],
  hr: [],
  rr: [],
  ve: [],
  o2: [],
  co2: [],
  co2Raw: [],
};
const practiceFlow = { t: [], espMs: [], lpm: [] };
const SESSION_SOURCES_CP = { wellue: true, hr: true };
const SESSION_SOURCES_PRACTICE = {
  wellue: true,
  hr: true,
  rr: true,
  flow: true,
  work_o2: true,
  co2: true,
};
const emptySources = {
  o2: false,
  flow: false,
  pressure: false,
  co2: false,
  wellue: false,
  hr: false,
  rr: false,
  work_o2: false,
  work_pressure: false,
};
let trendSources = { ...emptySources };
const history = {
  t: [],
  espMs: [],
  spo2: [],
  hrWellue: [],
  hrCoospo: [],
  co2: [],
  co2Raw: [],
  o2: [],
  flow: [],
  pressure: [],
  workO2: [],
  workPressure: [],
  rr: [],
};

function anyTrendSource() {
  return Object.values(trendSources).some(Boolean);
}

function parseSources(raw) {
  const s = raw && typeof raw === 'object' ? raw : {};
  return {
    o2: !!s.o2,
    flow: !!s.flow,
    pressure: !!s.pressure,
    co2: !!s.co2,
    wellue: !!s.wellue,
    hr: !!s.hr,
    rr: !!s.rr,
    work_o2: !!s.work_o2,
    work_pressure: !!s.work_pressure,
  };
}

function loadHrViewFlag(key, fallback) {
  try {
    const v = localStorage.getItem(key);
    if (v === null) return fallback;
    return v === '1' || v === 'true';
  } catch (e) {
    return fallback;
  }
}

function saveHrViewFlag(key, value) {
  try {
    localStorage.setItem(key, value ? '1' : '0');
  } catch (e) {
    /* ignore quota / private mode */
  }
}

let hrViewBelt = loadHrViewFlag(HR_VIEW_BELT_KEY, true);
let hrViewWellue = loadHrViewFlag(HR_VIEW_WELLUE_KEY, true);
let flowDynamicsActive = loadHrViewFlag(FLOW_DYNAMICS_KEY, false);
let flowDynamicsTimer = null;
let co2DynamicComp = false;
let pressureBaselineHpa = null;
let workPressureBaselineHpa = null;
const flowDynamics = { t: [], lpm: [] };

function syncHrViewInputs() {
  if (hrViewBeltInput) hrViewBeltInput.checked = hrViewBelt;
  if (hrViewWellueInput) hrViewWellueInput.checked = hrViewWellue;
}

function flowLpmFromStatus(flow) {
  const n = Number(flow?.slm ?? flow?.lpm);
  return Number.isFinite(n) ? n : null;
}

function roundPressure(v) {
  return Math.round(v * 10) / 10;
}

function formatPressureAxisTick(v, yMin, yMax) {
  const span = yMax - yMin;
  if (!Number.isFinite(span) || span <= 0) return roundPressure(v).toFixed(1);
  const tickStep = span / 4;
  if (tickStep < 0.08) return v.toFixed(2);
  if (tickStep < 0.4) return v.toFixed(1);
  return v.toFixed(0);
}

function roundCo2Pct(v) {
  return Math.round(v * 100) / 100;
}

function co2PercentFromPpm(ppm) {
  const n = Number(ppm);
  return Number.isFinite(n) ? roundCo2Pct(n / 10000) : null;
}

function co2PercentFromObj(co2) {
  const fromPpm = co2PercentFromPpm(co2?.ppm);
  if (fromPpm != null) return fromPpm;
  const pct = Number(co2?.percent);
  if (Number.isFinite(pct)) return roundCo2Pct(pct);
  return null;
}

function co2RawPercentFromObj(co2) {
  return co2PercentFromPpm(co2?.ppm_raw);
}

function interpolatePtsLinear(pts) {
  if (pts.length < 2) return pts;
  const out = [];
  for (let i = 0; i < pts.length - 1; i += 1) {
    const a = pts[i];
    const b = pts[i + 1];
    out.push(a);
    if (a.v == null || b.v == null || !Number.isFinite(a.v) || !Number.isFinite(b.v)) {
      continue;
    }
    if (a.v === b.v || b.t <= a.t) continue;
    const spanMs = b.t - a.t;
    const steps = Math.min(4, Math.max(1, Math.floor(spanMs / 1000)));
    for (let s = 1; s < steps; s += 1) {
      const f = s / steps;
      out.push({
        t: a.t + spanMs * f,
        v: a.v + (b.v - a.v) * f,
      });
    }
  }
  out.push(pts[pts.length - 1]);
  return out;
}

function ppmTrendToPercent(value) {
  const n = missingToNull(value);
  return n == null ? null : roundCo2Pct(n / 10000);
}

function mapPressurePts(pts) {
  return pts.map((p) => ({
    t: p.t,
    v: p.v == null || !Number.isFinite(p.v) ? null : roundPressure(p.v),
  }));
}

function mapCo2Pts(pts) {
  return pts.map((p) => ({
    t: p.t,
    v: p.v == null || !Number.isFinite(p.v) ? null : roundCo2Pct(p.v),
  }));
}

function pressurePanelConfig(rawPts, baselineHpa) {
  const pts = mapPressurePts(rawPts);
  const vals = valuesOf(pts);
  const refLine = baselineHpa != null && Number.isFinite(baselineHpa)
    ? roundPressure(baselineHpa)
    : null;
  const pool = [...vals];
  if (refLine != null) pool.push(refLine);
  const center = pool.length
    ? roundPressure(pool.reduce((a, b) => a + b, 0) / pool.length)
    : 1013;
  return {
    pts,
    fallback: {
      min: center - 0.5,
      max: center + 0.5,
      format: (v) => roundPressure(v).toFixed(1),
      formatTick: formatPressureAxisTick,
    },
    refLine,
    refLineLabel: refLine != null ? `база ${refLine.toFixed(1)}` : 'база',
    formatNow: (v) => `${roundPressure(v).toFixed(1)} hPa`,
  };
}

function countActiveTrendSources() {
  let n = 0;
  for (const key of Object.keys(trendSources)) {
    if (trendSources[key]) n += 1;
  }
  return n;
}

function computePollMs() {
  const n = countActiveTrendSources();
  if (n >= 5) return POLL_MS_HEAVY;
  if (n >= 3) return 1200;
  return POLL_MS_BASE;
}

function computeChartRedrawMs() {
  let n = countActiveTrendSources();
  if (flowDynamicsActive) n += 1;
  if (n <= 1) return CHART_REDRAW_MS_LIGHT;
  if (n <= 4) return 550;
  return CHART_REDRAW_MS_HEAVY;
}

function schedulePollInterval() {
  if (pollTimer) clearInterval(pollTimer);
  if (!pollingActive) return;
  pollTimer = setInterval(() => poll({ syncForm: false }), computePollMs());
}

function scheduleChartRedraw() {
  if (isActionModalOpen()) return;
  const minMs = computeChartRedrawMs();
  const now = Date.now();
  const elapsed = now - lastChartDrawMs;
  const run = () => {
    chartRedrawTimer = null;
    lastChartDrawMs = Date.now();
    drawAllCharts();
  };
  if (elapsed >= minMs) {
    run();
    return;
  }
  if (chartRedrawTimer) return;
  chartRedrawTimer = setTimeout(run, minMs - elapsed);
}

function buildFlowDynamicsPanel() {
  const pts = flowDynamicsPoints();
  const vals = valuesOf(pts);
  const last = lastFinite(vals);
  const peak = vals.length ? Math.max(...vals.map((v) => Math.abs(v))) : null;
  const parts = [];
  if (last != null) parts.push(`${last.toFixed(2)} л/мин`);
  if (peak != null) parts.push(`пик ${peak.toFixed(2)}`);
  return {
    panelId: 'panelFlowDynamics',
    canvas: chartFlowDynamics,
    nowEl: chartFlowDynamicsNow,
    nowText: parts.length ? parts.join(' · ') : '—',
    series: [{ pts, color: '#a78bfa', maxGapMs: FLOW_DYNAMICS_GAP_MS }],
    fallback: { min: -15, max: 15, format: (v) => v.toFixed(1) },
    emptyText: 'нет потока',
    zeroLine: true,
    seriesGapMs: FLOW_DYNAMICS_GAP_MS,
  };
}

function drawFlowDynamicsPanel() {
  if (isActionModalOpen()) return;
  if (!flowDynamicsActive || !chartFlowDynamics) return;
  setPanelHidden('panelFlowDynamics', false);
  const panel = buildFlowDynamicsPanel();
  if (panel.nowEl) panel.nowEl.textContent = panel.nowText;
  drawTrendChart(panel.canvas, panel.series, panel.fallback, panel.emptyText, {
    zeroLine: panel.zeroLine,
    seriesGapMs: panel.seriesGapMs,
  });
}

function trimFlowDynamics() {
  const cutoff = Date.now() - CHART_MAX_SEC * 1000;
  while (flowDynamics.t.length && flowDynamics.t[0] < cutoff) {
    flowDynamics.t.shift();
    flowDynamics.lpm.shift();
  }
}

function clearFlowDynamics() {
  flowDynamics.t = [];
  flowDynamics.lpm = [];
}

function pushFlowDynamicsSample(flow) {
  if (!flowDynamicsActive || !flow?.ok) return;
  const lpm = flowLpmFromStatus(flow);
  if (lpm == null) return;
  flowDynamics.t.push(Date.now());
  flowDynamics.lpm.push(lpm);
  trimFlowDynamics();
}

function flowDynamicsPoints() {
  const now = Date.now();
  const start = now - chartWindowSec * 1000;
  const pts = [];
  for (let i = 0; i < flowDynamics.t.length; i += 1) {
    if (flowDynamics.t[i] >= start) {
      pts.push({ t: flowDynamics.t[i], v: flowDynamics.lpm[i] });
    }
  }
  return pts;
}

function syncFlowDynamicsPoller() {
  if (flowDynamicsTimer) {
    clearInterval(flowDynamicsTimer);
    flowDynamicsTimer = null;
  }
  const needFast = !!baseUrl && flowDynamicsActive && !isActionModalOpen();
  if (!needFast) return;
  flowDynamicsTimer = setInterval(async () => {
    if (!baseUrl || statusPollPaused || pollInFlight || hydrateInFlight) return;
    if (!flowDynamicsActive || isActionModalOpen()) return;
    try {
      const data = await fetchStatus();
      const flow = (data.sensors?.working?.flow) || data.sensors?.flow || {};
      pushFlowDynamicsSample(flow);
      drawFlowDynamicsPanel();
    } catch (e) {
      /* ignore transient poll errors */
    }
  }, FLOW_DYNAMICS_POLL_MS);
}

function syncFlowDynamicsButton() {
  if (btnFlowDynamics) {
    btnFlowDynamics.classList.toggle('active', flowDynamicsActive);
  }
}

function syncTrendButtons() {
  document.querySelectorAll('.btn-trend').forEach((btn) => {
    const key = btn.dataset.src;
    if (!key) return;
    btn.classList.toggle('active', !!trendSources[key]);
  });
  syncFlowDynamicsButton();
}

function setPanelHidden(id, hidden) {
  const el = document.getElementById(id);
  if (el) el.hidden = hidden;
}

function setGpioUiState(on, { pulsing = false } = {}) {
  gpioState.checked = on;
  gpioStateLabel.classList.toggle('pulse', pulsing);
  if (pulsing) {
    gpioStateLabel.textContent = 'ON · импульс';
  } else {
    gpioStateLabel.textContent = on ? 'ON' : 'OFF';
  }
}

function fillPinSelects(pins, preserveSelection = true) {
  for (const sel of [gpioPin, servoPin]) {
    const prev = preserveSelection ? sel.value : '';
    sel.innerHTML = '';
    for (const p of pins) {
      const opt = document.createElement('option');
      opt.value = String(p);
      opt.textContent = `GPIO ${p}`;
      sel.appendChild(opt);
    }
    if (prev && [...sel.options].some((o) => o.value === prev)) {
      sel.value = prev;
    }
  }
}

fillPinSelects(DEFAULT_PINS, false);

servoAngle.addEventListener('input', () => {
  angleValue.textContent = servoAngle.value;
});

gpioState.addEventListener('change', () => {
  gpioStateLabel.textContent = gpioState.checked ? 'ON' : 'OFF';
});

for (const el of formControls) {
  el.addEventListener('focus', () => {
    formLocked = true;
  });
  el.addEventListener('blur', () => {
    setTimeout(() => {
      if (!formControls.some((c) => c === document.activeElement)) {
        formLocked = false;
      }
    }, 150);
  });
}

gpioPin.addEventListener('change', () => {
  if (lastStatus) syncGpioForm(lastStatus);
});

servoPin.addEventListener('change', () => {
  if (lastStatus) syncServoForm(lastStatus);
});

function apiUrl(path) {
  return `${baseUrl}${path}`;
}

async function fetchWithTimeout(url, options = {}, timeoutMs = FETCH_TIMEOUT_MS) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { ...options, signal: controller.signal });
    return res;
  } catch (e) {
    if (e.name === 'AbortError') {
      throw new Error('таймаут');
    }
    throw e;
  } finally {
    clearTimeout(timer);
  }
}

async function fetchStatus() {
  const res = await fetchWithTimeout(apiUrl('/api/status'));
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function sendCmd(body) {
  const res = await fetchWithTimeout(apiUrl('/api/cmd'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
}

function formatAge(ms) {
  if (ms < 1000) return 'только что';
  const sec = Math.floor(ms / 1000);
  if (sec < 60) return `${sec} с назад`;
  const min = Math.floor(sec / 60);
  return `${min} мин назад`;
}

function formatClock(ts) {
  return new Date(ts).toLocaleTimeString('ru-RU', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

function hostLabel() {
  if (!baseUrl) return '';
  return baseUrl.replace(/^https?:\/\//, '');
}

function computeLinkState() {
  if (!pollingActive || !baseUrl) return 'idle';
  if (!navigator.onLine) return 'offline';
  if (lastOkAt === null) return 'offline';
  const age = Date.now() - lastOkAt;
  if (age > STALE_AFTER_MS) return 'offline';
  if (age > computePollMs() * 2.5) return 'stale';
  return 'live';
}

function updateCommandButtons() {
  const disableCmds = linkState === 'offline' || !pollingActive;
  btnGpioApply.disabled = disableCmds;
  btnServoApply.disabled = disableCmds;
  btnFlowReset.disabled = disableCmds;
  if (btnFlowDynamics) btnFlowDynamics.disabled = disableCmds;
  if (btnCo2Frc) btnCo2Frc.disabled = disableCmds;
  if (!gpioSyncLocked) {
    btnGpioTap.disabled = disableCmds;
  }
}

function updateUptimeLine() {
  if (linkState === 'live' && lastUptimeMs !== null) {
    uptimeEl.textContent = `Uptime ESP32: ${(lastUptimeMs / 1000).toFixed(1)} s`;
    return;
  }
  if (lastOkAt !== null) {
    const parts = [`Последнее обновление: ${formatClock(lastOkAt)} (${formatAge(Date.now() - lastOkAt)})`];
    if (lastUptimeMs !== null) {
      parts.push(`Uptime было: ${(lastUptimeMs / 1000).toFixed(1)} s`);
    }
    uptimeEl.textContent = parts.join(' · ');
    return;
  }
  uptimeEl.textContent = '';
}

function renderLinkBar() {
  linkState = computeLinkState();
  linkBar.className = `link-bar ${linkState}`;

  const host = hostLabel();
  switch (linkState) {
    case 'live':
      linkText.textContent = 'Онлайн';
      linkMeta.textContent = host
        ? `${host} · обновлено ${formatAge(Date.now() - lastOkAt)}`
        : `обновлено ${formatAge(Date.now() - lastOkAt)}`;
      break;
    case 'stale':
      linkText.textContent = 'Задержка';
      linkMeta.textContent = `${formatAge(Date.now() - lastOkAt)} без ответа`;
      break;
    case 'offline':
      linkText.textContent = 'Нет связи';
      linkMeta.textContent = lastOkAt
        ? `данные устарели · ${formatAge(Date.now() - lastOkAt)}`
        : (lastError || 'ESP32 недоступен');
      break;
    default:
      linkText.textContent = 'Не подключено';
      linkMeta.textContent = host || '';
      break;
  }

  if (linkState === 'live') {
    connStatus.textContent = 'Подключено';
    connStatus.className = 'status ok';
  } else if (linkState === 'stale') {
    connStatus.textContent = 'Задержка ответа';
    connStatus.className = 'status warn';
  } else if (linkState === 'offline') {
    connStatus.textContent = lastError ? `Ошибка: ${lastError}` : 'Нет связи';
    connStatus.className = 'status err';
  } else {
    connStatus.textContent = 'Не подключено';
    connStatus.className = 'status';
  }

  updateUptimeLine();
  updateCommandButtons();
}

function startLinkUiTimer() {
  if (linkUiTimer) clearInterval(linkUiTimer);
  linkUiTimer = setInterval(renderLinkBar, 250);
}

function initPinSelects(data) {
  const pins = Array.isArray(data.whitelist) && data.whitelist.length
    ? data.whitelist
    : DEFAULT_PINS;
  const pinsKey = pins.join(',');
  if (pinsKey === lastPinsKey) return;
  fillPinSelects(pins);
  lastPinsKey = pinsKey;
}

function renderFlowPhase(phase) {
  const el = document.getElementById('flowPhase');
  if (!el) return;
  const labels = { inhale: 'Вдох', exhale: 'Выдох', idle: 'Покой' };
  const key = phase && labels[phase] ? phase : 'idle';
  el.textContent = labels[key];
  el.className = `flow-phase ${key}`;
}

function renderSensorTile(id, ok, text) {
  const tile = document.getElementById(id);
  if (!tile) return;
  tile.classList.toggle('error', !ok);
  const valueEl = tile.querySelector('.sensor-value');
  if (valueEl) valueEl.textContent = text;
}

function syncGpioForm(data) {
  if (gpioSyncLocked || data.gpio_tap_active) {
    return;
  }
  const gp = Number(gpioPin.value);
  const gpioItem = (data.gpio || []).find((g) => g.pin === gp);
  if (gpioItem) {
    setGpioUiState(gpioItem.state === 1);
  }
}

function syncServoForm(data) {
  const sp = Number(servoPin.value);
  const servoItem = (data.servos || []).find((s) => s.pin === sp);
  if (servoItem) {
    servoAngle.value = servoItem.angle;
    angleValue.textContent = String(servoItem.angle);
    servoAuto.checked = !!servoItem.auto;
  }
}

function renderServoStatus(data) {
  const sp = Number(servoPin.value);
  const servoItem = (data.servos || []).find((s) => s.pin === sp);
  if (servoItem) {
    servoStatus.textContent = `На устройстве: ${servoItem.angle}°, auto=${servoItem.auto}`;
  } else {
    servoStatus.textContent = 'Серва на этом pin ещё не настроена';
  }
}

function finiteOrNull(ok, value, extraOk = true) {
  if (!ok || !extraOk) return null;
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function lastFinite(arr) {
  for (let i = arr.length - 1; i >= 0; i -= 1) {
    if (arr[i] != null && Number.isFinite(arr[i])) return arr[i];
  }
  return null;
}

function missingToNull(value) {
  const n = Number(value);
  if (!Number.isFinite(n) || n <= -30000) return null;
  return n;
}

function fromScaled10(value) {
  const n = missingToNull(value);
  return n == null ? null : n / 10;
}

function saneSpo2(value) {
  const n = missingToNull(value);
  if (n == null || n < 50 || n > 100) return null;
  return n;
}

function saneHr(value) {
  const n = missingToNull(value);
  if (n == null || n < 25 || n > 240) return null;
  return n;
}

function saneRr(value) {
  const n = missingToNull(value);
  if (n == null || n < 250 || n > 2000) return null;
  return n;
}

function trimHistory() {
  while (history.t.length > CHART_MAX_SEC) {
    history.t.shift();
    history.espMs.shift();
    history.spo2.shift();
    history.hrWellue.shift();
    history.hrCoospo.shift();
    history.co2.shift();
    history.co2Raw.shift();
    history.o2.shift();
    history.flow.shift();
    history.pressure.shift();
    history.workO2.shift();
    history.workPressure.shift();
    history.rr.shift();
  }
}

function clearHistory() {
  history.t = [];
  history.espMs = [];
  history.spo2 = [];
  history.hrWellue = [];
  history.hrCoospo = [];
  history.co2 = [];
  history.co2Raw = [];
  history.o2 = [];
  history.flow = [];
  history.pressure = [];
  history.workO2 = [];
  history.workPressure = [];
  history.rr = [];
}

function applyTrendsPayload(data, { replace = true } = {}) {
  const count = Number(data.count || 0);
  const interval = Number(data.interval_ms || 1000);
  const nowMs = Number(data.now_ms || 0);
  const t0 = Number(data.t0_ms || 0);
  const offset = Number(data.offset || 0);
  const wallNow = Date.now();
  const spo2 = Array.isArray(data.spo2) ? data.spo2 : [];
  const hrW = Array.isArray(data.hr_wellue) ? data.hr_wellue : [];
  const hrC = Array.isArray(data.hr_coospo) ? data.hr_coospo : [];
  const co2 = Array.isArray(data.co2) ? data.co2 : [];
  const co2Raw = Array.isArray(data.co2_raw) ? data.co2_raw : [];
  const o2 = Array.isArray(data.o2) ? data.o2 : [];
  const flow = Array.isArray(data.flow) ? data.flow : [];
  const pressure = Array.isArray(data.pressure) ? data.pressure : [];
  const workO2 = Array.isArray(data.work_o2) ? data.work_o2 : [];
  const workPressure = Array.isArray(data.work_pressure) ? data.work_pressure : [];
  const rr = Array.isArray(data.rr) ? data.rr : [];
  if (data.sources) {
    trendSources = parseSources(data.sources);
    syncTrendButtons();
  }
  if (co2Raw.length) {
    co2DynamicComp = true;
  }

  if (replace) {
    clearHistory();
  }

  const lastEsp = lastHistoryEspMs();
  for (let i = 0; i < count; i += 1) {
    const sampleMs = t0 + (offset + i) * interval;
    if (lastEsp != null && Number.isFinite(sampleMs) && sampleMs <= lastEsp) {
      continue;
    }
    history.espMs.push(sampleMs);
    history.t.push(wallNow - (nowMs - sampleMs));
    history.spo2.push(saneSpo2(spo2[i]));
    history.hrWellue.push(saneHr(hrW[i]));
    history.hrCoospo.push(saneHr(hrC[i]));
    history.co2.push(ppmTrendToPercent(co2[i]));
    history.co2Raw.push(
      co2Raw.length > i ? ppmTrendToPercent(co2Raw[i]) : null,
    );
    history.o2.push(fromScaled10(o2[i]));
    history.flow.push(fromScaled10(flow[i]));
    history.pressure.push(fromScaled10(pressure[i]));
    history.workO2.push(fromScaled10(workO2[i]));
    history.workPressure.push(fromScaled10(workPressure[i]));
    history.rr.push(saneRr(rr[i]));
  }
  trimHistory();
}

function lastArrEspMs(espArr) {
  if (!espArr || !espArr.length) return null;
  for (let i = espArr.length - 1; i >= 0; i -= 1) {
    if (espArr[i] != null && Number.isFinite(espArr[i])) return espArr[i];
  }
  return null;
}

function lastKnownEspMs() {
  const candidates = [
    lastHistoryEspMs(),
    lastArrEspMs(cpHistory.espMs),
    lastArrEspMs(practiceHistory.espMs),
  ];
  let best = null;
  for (const v of candidates) {
    if (v == null) continue;
    if (best == null || v > best) best = v;
  }
  return best;
}

function wallFromEsp(espMs, nowEspMs) {
  if (!Number.isFinite(espMs)) return Date.now();
  if (!Number.isFinite(nowEspMs)) return Date.now();
  return Date.now() - Math.max(0, nowEspMs - espMs);
}

function sessionInWindow(startedAt, t) {
  if (startedAt == null) return false;
  if (t < startedAt) return false;
  return true;
}

function insertParallel(store, point) {
  const n = store.t.length;
  let idx = n;
  if (point.espMs != null && Number.isFinite(point.espMs) && store.espMs) {
    for (let i = 0; i < store.espMs.length; i += 1) {
      if (store.espMs[i] == null || !Number.isFinite(store.espMs[i])) continue;
      if (store.espMs[i] === point.espMs) return false;
      if (store.espMs[i] > point.espMs) {
        idx = i;
        break;
      }
    }
  }
  for (const key of Object.keys(store)) {
    store[key].splice(idx, 0, point[key] !== undefined ? point[key] : null);
  }
  return true;
}

function overlaySessionsFromHistory(fromIndex = 0) {
  const startIdx = Math.max(0, fromIndex);
  if (startIdx >= history.t.length) return;
  const cpLive = isCpLive() && cpStartedAt != null;
  const practiceLive = isPracticeLive() && practiceStartedAt != null;
  if (!cpLive && !practiceLive) return;

  for (let i = startIdx; i < history.t.length; i += 1) {
    const t = history.t[i];
    const espMs = history.espMs[i];
    if (cpLive && sessionInWindow(cpStartedAt, t)) {
      insertParallel(cpHistory, {
        t,
        espMs,
        spo2: history.spo2[i],
        hrWellue: history.hrWellue[i],
        hrCoospo: history.hrCoospo[i],
      });
    }
    if (practiceLive && sessionInWindow(practiceStartedAt, t)) {
      const hr = practiceHrBelt ? history.hrCoospo[i] : history.hrWellue[i];
      insertParallel(practiceHistory, {
        t,
        espMs,
        spo2: history.spo2[i],
        hr,
        rr: history.rr[i],
        ve: history.flow[i],
        o2: history.workO2[i],
        co2: history.co2[i],
        co2Raw: history.co2Raw[i],
      });
    }
  }

  if (cpLive) {
    expandCpWindow(cpElapsedSec());
    updateCpChrome();
    drawCpCharts();
  }
  if (practiceLive) {
    expandPracticeWindow(practiceElapsedSec());
    updatePracticeChrome();
    drawPracticeCharts();
  }
}

function sourcesEqual(a, b) {
  const keys = Object.keys(emptySources);
  for (const k of keys) {
    if (!!a?.[k] !== !!b?.[k]) return false;
  }
  return true;
}

function enqueueSourcesWrite(task) {
  sourcesWriteChain = sourcesWriteChain.then(task, task);
  return sourcesWriteChain;
}

function enableSessionTrendSources(extra) {
  return enqueueSourcesWrite(async () => {
    if (!sessionSourcesSaved) {
      sessionSourcesSaved = { ...trendSources };
    }
    const next = { ...trendSources, ...extra };
    if (sourcesEqual(trendSources, next)) return;
    try {
      await postTrendSources(next);
      trendSources = next;
      syncTrendButtons();
      schedulePollInterval();
    } catch (e) {
      /* live poll continues; backfill may miss this session */
    }
  });
}

function restoreSessionTrendSources() {
  return enqueueSourcesWrite(async () => {
    const saved = sessionSourcesSaved;
    if (!saved) return;
    sessionSourcesSaved = null;
    if (sourcesEqual(trendSources, saved)) return;
    try {
      await postTrendSources(saved);
      trendSources = parseSources(saved);
      syncTrendButtons();
      schedulePollInterval();
    } catch (e) {
      /* keep current mask */
    }
  });
}

function shouldBackfillAfterGap(data) {
  if (!anyTrendSource() && !isCpLive() && !isPracticeLive()) return false;
  const espMs = Number(data.uptime_ms);
  const last = lastKnownEspMs();
  if (!history.t.length && !isCpLive() && !isPracticeLive()) return true;
  if (!Number.isFinite(espMs) || last == null) return isCpLive() || isPracticeLive() || !history.t.length;
  if (espMs + 2000 < last) return true;
  return (espMs - last) > TRENDS_BACKFILL_MIN_GAP_MS;
}

async function fetchTrendsPage(offset, limit) {
  const res = await fetchWithTimeout(
    apiUrl(`/api/trends?offset=${offset}&limit=${limit}`),
    {},
    TRENDS_FETCH_TIMEOUT_MS,
  );
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function hydrateTrends(opts = {}) {
  if (!baseUrl || hydrateInFlight) return false;
  const fromEspMs = opts.fromEspMs;
  const maxPages = opts.maxPages ?? TRENDS_HYDRATE_MAX_PAGES;
  const replace = opts.replace === true || (opts.replace !== false && fromEspMs == null);
  hydrateInFlight = true;
  const histFrom = replace ? 0 : history.t.length;
  try {
    let offset = 0;
    if (fromEspMs != null && Number.isFinite(fromEspMs)) {
      const probe = await fetchTrendsPage(0, 1);
      const t0 = Number(probe.t0_ms || 0);
      const interval = Number(probe.interval_ms || 1000) || 1000;
      const total = Number(probe.total || 0);
      offset = Math.floor((fromEspMs - t0) / interval) + 1;
      if (!Number.isFinite(offset) || offset < 0) offset = 0;
      if (offset > total) offset = total;
    } else if (!replace) {
      const probe = await fetchTrendsPage(0, 1);
      const total = Number(probe.total || 0);
      offset = Math.max(0, total - maxPages * TRENDS_PAGE);
    }
    for (let page = 0; page < maxPages; page += 1) {
      const data = await fetchTrendsPage(offset, TRENDS_PAGE);
      applyTrendsPayload(data, { replace: replace && page === 0 && offset === 0 });
      const total = Number(data.total || 0);
      const count = Number(data.count || 0);
      offset += count;
      if (count === 0 || offset >= total) break;
    }
    lastHydrateAt = Date.now();
    overlaySessionsFromHistory(histFrom);
    updateTrendsHint();
    drawAllCharts();
    return true;
  } catch (e) {
    if (trendsHint) {
      trendsHint.textContent = `История ESP32 недоступна: ${e.message}`;
    }
    return false;
  } finally {
    hydrateInFlight = false;
  }
}

function lastHistoryEspMs() {
  if (!history.espMs.length) return null;
  return history.espMs[history.espMs.length - 1];
}

function pushHistory(data) {
  const s = data.sensors || {};
  const wellue = s.wellue || {};
  const coospo = s.coospo || {};
  const co2 = (s.working && s.working.co2) || s.co2 || {};
  const o2 = (s.hypoxia && s.hypoxia.o2) || s.o2 || {};
  const flow = (s.working && s.working.flow) || s.flow || {};
  const pressure = (s.hypoxia && s.hypoxia.pressure) || s.pressure || {};
  const workO2 = (s.working && s.working.o2) || {};
  const workPressure = (s.working && s.working.pressure) || {};
  const wellueLive = !!trendSources.wellue && !!wellue.ok && !!wellue.contact;
  const coospoLive = !!trendSources.hr && !!coospo.ok && (coospo.contact !== false);
  const rrLive = !!trendSources.rr && !!coospo.ok && (coospo.contact !== false);
  const espMs = Number(data.uptime_ms);
  const lastEsp = lastHistoryEspMs();
  if (Number.isFinite(espMs) && lastEsp != null && (espMs - lastEsp) < 500) {
    return;
  }

  const wall = Number.isFinite(espMs)
    ? Date.now() - Math.max(0, (Number(data.uptime_ms) || 0) - espMs)
    : Date.now();
  history.t.push(wall);
  history.espMs.push(Number.isFinite(espMs) ? espMs : null);
  history.spo2.push(wellueLive ? saneSpo2(wellue.spo2) : null);
  history.hrWellue.push(wellueLive ? saneHr(wellue.hr) : null);
  history.hrCoospo.push(coospoLive ? saneHr(coospo.bpm) : null);
  history.co2.push(
    trendSources.co2 ? finiteOrNull(!!co2.ok, co2PercentFromObj(co2)) : null,
  );
  history.co2Raw.push(
    trendSources.co2 && co2DynamicComp
      ? finiteOrNull(!!co2.ok, co2RawPercentFromObj(co2))
      : null,
  );
  history.o2.push(trendSources.o2 ? finiteOrNull(!!o2.ok, o2.percent) : null);
  history.flow.push(trendSources.flow ? finiteOrNull(!!flow.ok, flow.ve_lpm) : null);
  history.pressure.push(trendSources.pressure ? finiteOrNull(!!pressure.ok, pressure.hpa) : null);
  history.workO2.push(trendSources.work_o2 ? finiteOrNull(!!workO2.ok, workO2.percent) : null);
  history.workPressure.push(
    trendSources.work_pressure ? finiteOrNull(!!workPressure.ok, workPressure.hpa) : null,
  );
  history.rr.push(rrLive ? saneRr(coospo.rr_ms) : null);
  trimHistory();
}

function pointsInWindow(values) {
  const now = Date.now();
  const start = now - chartWindowSec * 1000;
  const pts = [];
  for (let i = 0; i < history.t.length; i += 1) {
    if (history.t[i] >= start) {
      pts.push({ t: history.t[i], v: values[i] });
    }
  }
  return pts;
}

function valuesOf(pts) {
  return pts.map((p) => p.v).filter((v) => v != null && Number.isFinite(v));
}

function niceRange(vals, fallbackMin, fallbackMax) {
  if (!vals.length) return { min: fallbackMin, max: fallbackMax };
  let min = Math.min(...vals);
  let max = Math.max(...vals);
  if (min === max) {
    const span = Math.max(1, (fallbackMax - fallbackMin) * 0.08);
    min -= span;
    max += span;
  }
  const pad = (max - min) * 0.12;
  return { min: min - pad, max: max + pad };
}

function formatRange(vals, format) {
  if (!vals.length) return '';
  const min = Math.min(...vals);
  const max = Math.max(...vals);
  const last = vals[vals.length - 1];
  if (min === max) return format(last);
  return `${format(last)}  ·  ${format(min)}–${format(max)}`;
}

function xOf(t, now, x0, plotW, windowSec) {
  const win = windowSec ?? chartWindowSec;
  const start = now - win * 1000;
  const span = Math.max(1, now - start);
  return x0 + ((t - start) / span) * plotW;
}

function plotX(t, now, x0, plotW, windowSec, opts) {
  if (opts?.xMode === 'fromOrigin' && opts.timeOrigin != null && Number.isFinite(opts.timeOrigin)) {
    const span = Math.max(1, windowSec * 1000);
    return x0 + ((t - opts.timeOrigin) / span) * plotW;
  }
  return xOf(t, now, x0, plotW, windowSec);
}

function yOf(v, min, max, y0, plotH) {
  const span = max - min || 1;
  return y0 + plotH - ((v - min) / span) * plotH;
}

function setupCanvas(canvas, cssW, cssH, dpr) {
  const ratio = dpr ?? (window.devicePixelRatio || 1);
  const width = Math.max(1, cssW ?? (canvas.clientWidth || canvas.parentElement?.clientWidth || 320));
  const height = Math.max(1, cssH ?? (canvas.clientHeight || 148));
  const w = Math.round(width * ratio);
  const h = Math.round(height * ratio);
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  const ctx = canvas.getContext('2d');
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  return { ctx, w: width, h: height };
}

window.__oxy = {
  getApiUrl: (path) => apiUrl(path),
  fetchWithTimeout,
  setupCanvas,
  getBaseUrl: () => baseUrl,
  setStatusPollPaused(paused) {
    statusPollPaused = !!paused;
  },
  cancelMainChartRedraw,
  resumeMainCharts,
};

function loadHrvModule() {
  if (!window.OxyHrv) {
    return Promise.reject(
      new Error('hrv.js не найден на ESP32. Выполните: cd firmware && pio run -t uploadfs'),
    );
  }
  window.OxyHrv.init();
  return Promise.resolve();
}

function drawSeries(
  ctx,
  pts,
  now,
  x0,
  y0,
  plotW,
  plotH,
  yMin,
  yMax,
  color,
  dashed,
  windowSec,
  maxGapMs,
  opts,
) {
  ctx.save();
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.6;
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';
  if (dashed) ctx.setLineDash([5, 4]);
  const gapMs = maxGapMs ?? POLL_MS_BASE * 2.5;
  let open = false;
  let prevT = null;
  ctx.beginPath();
  for (const p of pts) {
    if (p.v == null || !Number.isFinite(p.v)) {
      open = false;
      prevT = null;
      continue;
    }
    const x = plotX(p.t, now, x0, plotW, windowSec, opts);
    const y = yOf(p.v, yMin, yMax, y0, plotH);
    if (!open || (prevT != null && p.t - prevT > gapMs)) {
      ctx.moveTo(x, y);
      open = true;
    } else {
      ctx.lineTo(x, y);
    }
    prevT = p.t;
  }
  ctx.stroke();
  ctx.restore();
}

function drawVerticalTimeGrid(ctx, opts, now, windowSec, x0, y0, plotW, plotH) {
  const gridSec = opts?.timeGridSec;
  const origin = opts?.timeOrigin;
  if (!gridSec || gridSec <= 0 || origin == null || !Number.isFinite(origin)) {
    return;
  }
  const fromOrigin = opts?.xMode === 'fromOrigin';
  const step = gridSec * 1000;
  const winStart = fromOrigin ? origin : now - windowSec * 1000;
  const winEnd = fromOrigin ? origin + windowSec * 1000 : now + step;
  let t = origin;
  if (t < winStart) {
    t += Math.ceil((winStart - t) / step) * step;
  }
  ctx.save();
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 1;
  for (; t <= winEnd; t += step) {
    const x = plotX(t, now, x0, plotW, windowSec, opts);
    if (x < x0 || x > x0 + plotW) continue;
    ctx.beginPath();
    ctx.moveTo(x, y0);
    ctx.lineTo(x, y0 + plotH);
    ctx.stroke();
  }
  ctx.restore();
}

function drawChartMarkers(ctx, opts, now, windowSec, x0, y0, plotW, plotH) {
  const list = [];
  if (opts?.markerT != null && Number.isFinite(opts.markerT)) {
    list.push({
      t: opts.markerT,
      color: opts.markerColor ?? '#38bdf8',
      width: 1.6,
      dash: [4, 3],
    });
  }
  if (Array.isArray(opts?.markers)) {
    for (const m of opts.markers) {
      if (m && Number.isFinite(m.t)) list.push(m);
    }
  }
  for (const m of list) {
    const mx = plotX(m.t, now, x0, plotW, windowSec, opts);
    if (mx < x0 || mx > x0 + plotW) continue;
    ctx.save();
    ctx.strokeStyle = m.color ?? '#38bdf8';
    ctx.lineWidth = m.width ?? 1.6;
    ctx.globalAlpha = m.alpha ?? 1;
    if (m.dash) ctx.setLineDash(m.dash);
    ctx.beginPath();
    ctx.moveTo(mx, y0);
    ctx.lineTo(mx, y0 + plotH);
    ctx.stroke();
    ctx.restore();
  }
}

function drawTrendChart(canvas, series, yFallback, emptyText, opts) {
  if (!canvas) return;
  const { ctx, w, h } = opts && opts.w
    ? setupCanvas(canvas, opts.w, opts.h, opts.dpr)
    : setupCanvas(canvas);
  ctx.clearRect(0, 0, w, h);

  const windowSec = opts?.windowSec ?? chartWindowSec;
  const now = opts?.now ?? Date.now();
  const x0 = CHART_PAD.left;
  const y0 = CHART_PAD.top;
  const plotW = Math.max(1, w - CHART_PAD.left - CHART_PAD.right);
  const plotH = Math.max(1, h - CHART_PAD.top - CHART_PAD.bottom);
  const allVals = series.flatMap((s) => valuesOf(s.pts));
  if (opts?.refLine != null && Number.isFinite(opts.refLine)) {
    allVals.push(opts.refLine);
  }
  const { min: yMin, max: yMax } = niceRange(allVals, yFallback.min, yFallback.max);
  const formatTick = (v) => {
    if (typeof yFallback.formatTick === 'function') {
      return yFallback.formatTick(v, yMin, yMax);
    }
    return yFallback.format(v);
  };

  ctx.fillStyle = '#101010';
  ctx.fillRect(x0, y0, plotW, plotH);

  ctx.font = '10px system-ui, sans-serif';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  const ticks = 4;
  let lastTickLabel = null;
  for (let i = 0; i <= ticks; i += 1) {
    const frac = i / ticks;
    const y = y0 + plotH * (1 - frac);
    const v = yMin + (yMax - yMin) * frac;
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x0, y);
    ctx.lineTo(x0 + plotW, y);
    ctx.stroke();
    const label = formatTick(v);
    if (label !== lastTickLabel) {
      ctx.fillStyle = '#777';
      ctx.fillText(label, x0 - 6, y);
      lastTickLabel = label;
    }
  }

  drawVerticalTimeGrid(ctx, opts, now, windowSec, x0, y0, plotW, plotH);

  ctx.strokeStyle = '#333';
  ctx.strokeRect(x0 + 0.5, y0 + 0.5, plotW - 1, plotH - 1);

  if (opts?.zeroLine && yMin < 0 && yMax > 0) {
    const yZero = yOf(0, yMin, yMax, y0, plotH);
    ctx.save();
    ctx.strokeStyle = 'rgba(255,255,255,0.18)';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(x0, yZero);
    ctx.lineTo(x0 + plotW, yZero);
    ctx.stroke();
    ctx.restore();
  }

  if (opts?.refLine != null && Number.isFinite(opts.refLine)) {
    const yRef = yOf(opts.refLine, yMin, yMax, y0, plotH);
    if (yRef >= y0 - 1 && yRef <= y0 + plotH + 1) {
      ctx.save();
      ctx.strokeStyle = opts.refLineColor ?? 'rgba(56, 189, 248, 0.55)';
      ctx.lineWidth = 1.4;
      ctx.setLineDash([6, 4]);
      ctx.beginPath();
      ctx.moveTo(x0, yRef);
      ctx.lineTo(x0 + plotW, yRef);
      ctx.stroke();
      ctx.fillStyle = opts.refLineColor ?? 'rgba(56, 189, 248, 0.85)';
      ctx.font = '10px system-ui, sans-serif';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'bottom';
      const refLabel = opts.refLineLabel ?? 'база';
      ctx.fillText(refLabel, x0 + 4, yRef - 2);
      ctx.restore();
    }
  }

  const hasLine = series.some((s) => valuesOf(s.pts).length >= 1);
  const seriesGapMs = opts?.seriesGapMs;
  if (!hasLine) {
    ctx.fillStyle = '#666';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.font = '12px system-ui, sans-serif';
    ctx.fillText(emptyText, x0 + plotW / 2, y0 + plotH / 2);
  } else {
    for (const s of series) {
      drawSeries(
        ctx,
        s.pts,
        now,
        x0,
        y0,
        plotW,
        plotH,
        yMin,
        yMax,
        s.color,
        !!s.dashed,
        windowSec,
        s.maxGapMs ?? seriesGapMs,
        opts,
      );
    }
  }

  drawChartMarkers(ctx, opts, now, windowSec, x0, y0, plotW, plotH);

  ctx.fillStyle = '#666';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.font = '10px system-ui, sans-serif';
  let leftLabel;
  let rightLabel;
  if (opts?.xMode === 'fromOrigin') {
    leftLabel = opts.leftLabel ?? '0:00';
    rightLabel = opts.rightLabel ?? formatMmSs(windowSec);
  } else {
    leftLabel = windowSec >= 120
      ? `−${Math.round(windowSec / 60)} мин`
      : `−${windowSec} с`;
    rightLabel = opts?.rightLabel ?? 'сейчас';
  }
  ctx.fillText(leftLabel, x0, y0 + plotH + 6);
  ctx.textAlign = 'right';
  ctx.fillText(rightLabel, x0 + plotW, y0 + plotH + 6);
}

function updateTrendsHint() {
  if (trendsHint) {
    if (!anyTrendSource()) {
      trendsHint.textContent = 'Включите «Тренд» на плитке датчика — набор хранится на ESP32';
    } else {
      const min = chartWindowSec / 60;
      const filled = history.t.length;
      const filledMin = Math.max(1, Math.round(filled / 60));
      const span = filled < 60 ? `${filled} с` : `${filledMin} мин`;
      const ringMin = Math.round(CHART_MAX_SEC / 60);
      trendsHint.textContent =
        `Окно ${min} мин · 1 Гц · история на ESP32 (${span} из ~${ringMin} мин, RAM)`;
    }
  }
  syncTrendsSaveButton();
}

function syncTrendsSaveButton() {
  if (!btnTrendsSave) return;
  btnTrendsSave.disabled = !anyTrendSource() || history.t.length === 0;
}

function pad2(n) {
  return String(n).padStart(2, '0');
}

function formatMmSs(totalSec) {
  const sec = Math.max(0, Math.floor(Number(totalSec) || 0));
  return `${pad2(Math.floor(sec / 60))}:${pad2(sec % 60)}`;
}

function trendsFilenameStamp(date) {
  const d = date instanceof Date ? date : new Date();
  return `${d.getFullYear()}${pad2(d.getMonth() + 1)}${pad2(d.getDate())}_${pad2(d.getHours())}${pad2(d.getMinutes())}`;
}

function trendsExportStamp(date) {
  const d = date instanceof Date ? date : new Date();
  return `${pad2(d.getDate())}.${pad2(d.getMonth() + 1)}.${d.getFullYear()} ${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
}

function listVisibleTrendPanels() {
  const showO2 = !!trendSources.o2;
  const showWorkO2 = !!trendSources.work_o2;
  const showFlow = !!trendSources.flow;
  const showFlowDynamics = flowDynamicsActive;
  const showPressure = !!trendSources.pressure;
  const showWorkPressure = !!trendSources.work_pressure;
  const showCo2 = !!trendSources.co2;
  const showWellue = !!trendSources.wellue;
  const showHrBelt = !!trendSources.hr;
  const showRr = !!trendSources.rr;
  const showHr = showWellue || showHrBelt;
  const dualHr = showWellue && showHrBelt;
  const drawBeltHr = showHrBelt && (!dualHr || hrViewBelt);
  const drawWellueHr = showWellue && (!dualHr || hrViewWellue);
  const panels = [];

  const pushPanel = (
    panelId,
    canvas,
    nowEl,
    title,
    pts,
    color,
    fallback,
    formatNow,
    emptyText,
    extra = {},
  ) => {
    panels.push({
      panelId,
      canvas,
      nowEl,
      title,
      nowText: formatRange(valuesOf(pts), formatNow) || '—',
      series: [{ pts, color }],
      fallback,
      emptyText,
      ...extra,
    });
  };

  if (showO2) {
    pushPanel('panelO2', chartO2, chartO2Now, 'O₂ · гипоксия',
      pointsInWindow(history.o2), '#22d3ee',
      { min: 18, max: 22, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} %`, 'нет O₂');
  }
  if (showWorkO2) {
    pushPanel('panelWorkO2', chartWorkO2, chartWorkO2Now, 'O₂ · рабочий',
      pointsInWindow(history.workO2), '#67e8f9',
      { min: 18, max: 22, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} %`, 'нет O₂');
  }
  if (showFlow) {
    pushPanel(
      'panelFlow',
      chartFlow,
      chartFlowNow,
      'Минутная вентиляция · рабочий',
      pointsInWindow(history.flow),
      '#a78bfa',
      { min: 0, max: 30, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} л/мин`,
      'нет данных',
    );
  }
  if (showFlowDynamics) {
    panels.push(buildFlowDynamicsPanel());
  }
  if (showPressure) {
    const cfg = pressurePanelConfig(pointsInWindow(history.pressure), pressureBaselineHpa);
    panels.push({
      panelId: 'panelPressure',
      canvas: chartPressure,
      nowEl: chartPressureNow,
      title: 'Давление · гипоксия',
      nowText: formatRange(valuesOf(cfg.pts), cfg.formatNow) || '—',
      series: [{ pts: cfg.pts, color: '#e879f9' }],
      fallback: cfg.fallback,
      emptyText: 'нет давления',
      refLine: cfg.refLine,
      refLineLabel: cfg.refLineLabel,
    });
  }
  if (showWorkPressure) {
    const cfg = pressurePanelConfig(pointsInWindow(history.workPressure), workPressureBaselineHpa);
    panels.push({
      panelId: 'panelWorkPressure',
      canvas: chartWorkPressure,
      nowEl: chartWorkPressureNow,
      title: 'Давление · рабочий',
      nowText: formatRange(valuesOf(cfg.pts), cfg.formatNow) || '—',
      series: [{ pts: cfg.pts, color: '#f0abfc' }],
      fallback: cfg.fallback,
      emptyText: 'нет давления',
      refLine: cfg.refLine,
      refLineLabel: cfg.refLineLabel,
    });
  }
  if (showCo2) {
    const co2EstPts = interpolatePtsLinear(
      mapCo2Pts(pointsInWindow(history.co2)),
    );
    const co2RawPts = co2DynamicComp
      ? interpolatePtsLinear(mapCo2Pts(pointsInWindow(history.co2Raw)))
      : [];
    const co2Series = [{ pts: co2EstPts, color: '#60a5fa' }];
    if (co2DynamicComp && co2RawPts.length) {
      co2Series.push({ pts: co2RawPts, color: '#93c5fd', dashed: true });
    }
    const estVals = valuesOf(co2EstPts);
    panels.push({
      panelId: 'panelCo2',
      canvas: chartCo2,
      nowEl: chartCo2Now,
      title: 'CO₂ · рабочий',
      nowText: formatRange(estVals, (v) => `${roundCo2Pct(v).toFixed(2)} %`) || '—',
      series: co2Series,
      fallback: { min: 0.04, max: 0.2, format: (v) => v.toFixed(2) },
      emptyText: 'нет CO₂',
      co2DynamicComp,
    });
  }
  if (showWellue) {
    pushPanel('panelSpo2', chartSpo2, chartSpo2Now, 'SpO₂',
      pointsInWindow(history.spo2), '#4ade80',
      { min: 90, max: 100, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(0)} %`, 'нет SpO₂');
  }
  if (showHr) {
    const hrCoospoPts = drawBeltHr ? pointsInWindow(history.hrCoospo) : [];
    const hrWelluePts = drawWellueHr ? pointsInWindow(history.hrWellue) : [];
    const parts = [];
    if (drawBeltHr) {
      const last = lastFinite(hrCoospoPts.map((p) => p.v));
      if (last != null) parts.push(`${beltDisplayName} ${last.toFixed(0)}`);
    }
    if (drawWellueHr) {
      const last = lastFinite(hrWelluePts.map((p) => p.v));
      if (last != null) parts.push(`Wellue ${last.toFixed(0)}`);
    }
    const hrSeries = [];
    if (drawBeltHr) hrSeries.push({ pts: hrCoospoPts, color: '#f87171' });
    if (drawWellueHr) hrSeries.push({ pts: hrWelluePts, color: '#fb923c', dashed: true });
    panels.push({
      panelId: 'panelHr',
      canvas: chartHr,
      nowEl: chartHrNow,
      title: 'ЧСС',
      nowText: parts.length ? `${parts.join(' · ')} bpm` : '—',
      series: hrSeries,
      fallback: { min: 50, max: 140, format: (v) => v.toFixed(0) },
      emptyText: 'нет ЧСС',
    });
  }
  if (showRr) {
    pushPanel('panelRr', chartRr, chartRrNow, 'R-R',
      pointsInWindow(history.rr), '#f87171',
      { min: 400, max: 1200, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(0)} мс`, 'нет R-R');
  }

  return {
    panels,
    showO2,
    showWorkO2,
    showFlow,
    showFlowDynamics,
    showPressure,
    showWorkPressure,
    showCo2,
    showWellue,
    showHrBelt,
    showRr,
    showHr,
    dualHr,
  };
}

function drawAllCharts() {
  if (isActionModalOpen()) return;
  const vis = listVisibleTrendPanels();

  setPanelHidden('panelO2', !vis.showO2);
  setPanelHidden('panelWorkO2', !vis.showWorkO2);
  setPanelHidden('panelFlow', !vis.showFlow);
  setPanelHidden('panelFlowDynamics', !vis.showFlowDynamics);
  setPanelHidden('panelPressure', !vis.showPressure);
  setPanelHidden('panelWorkPressure', !vis.showWorkPressure);
  setPanelHidden('panelCo2', !vis.showCo2);
  setPanelHidden('panelSpo2', !vis.showWellue);
  setPanelHidden('panelHr', !vis.showHr);
  setPanelHidden('panelRr', !vis.showRr);

  if (hrLegendBelt) hrLegendBelt.textContent = beltDisplayName;
  if (hrLegendBeltWrap) {
    hrLegendBeltWrap.hidden = !vis.showHrBelt;
    hrLegendBeltWrap.classList.toggle('plain', !vis.dualHr);
  }
  if (hrLegendWellueWrap) {
    hrLegendWellueWrap.hidden = !vis.showWellue;
    hrLegendWellueWrap.classList.toggle('plain', !vis.dualHr);
  }
  syncHrViewInputs();

  const co2LegendWrap = document.getElementById('co2LegendWrap');
  if (co2LegendWrap) {
    co2LegendWrap.hidden = !vis.showCo2 || !co2DynamicComp;
  }

  for (const panel of vis.panels) {
    if (panel.nowEl) panel.nowEl.textContent = panel.nowText;
    drawTrendChart(panel.canvas, panel.series, panel.fallback, panel.emptyText, {
      zeroLine: panel.zeroLine,
      seriesGapMs: panel.seriesGapMs,
      refLine: panel.refLine,
      refLineLabel: panel.refLineLabel,
    });
  }
  syncTrendsSaveButton();
}

function saveTrendsImage() {
  const vis = listVisibleTrendPanels();
  if (!vis.panels.length || history.t.length === 0) {
    if (trendsHint) {
      trendsHint.textContent = 'Нечего сохранять — включите тренд и дождитесь точек';
    }
    return;
  }

  const now = new Date();
  const windowMin = Math.max(1, Math.round(chartWindowSec / 60));
  const pad = TREND_EXPORT_PAD;
  const headerH = 56;
  const metaH = 26;
  const plotH = TREND_EXPORT_PLOT_H;
  const panelGap = 12;
  const dpr = 2;
  const n = vis.panels.length;
  const exportW = TREND_EXPORT_W;
  const exportH = pad + headerH + n * (metaH + plotH) + Math.max(0, n - 1) * panelGap + pad;
  const canvas = document.createElement('canvas');
  const { ctx, w, h } = setupCanvas(canvas, exportW, exportH, dpr);

  ctx.fillStyle = '#1c1c1c';
  ctx.fillRect(0, 0, w, h);

  let y = pad;
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 18px system-ui, sans-serif';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText('OxyPulse · Тренды', pad, y);
  y += 26;
  ctx.fillStyle = '#888888';
  ctx.font = '13px system-ui, sans-serif';
  ctx.fillText(`Окно ${windowMin} мин · ${trendsExportStamp(now)}`, pad, y);
  y += headerH - 26;

  const contentW = exportW - pad * 2;
  for (const panel of vis.panels) {
    ctx.fillStyle = '#dddddd';
    ctx.font = 'bold 14px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(panel.title, pad, y);
    ctx.fillStyle = '#aaaaaa';
    ctx.font = '13px system-ui, sans-serif';
    ctx.textAlign = 'right';
    ctx.fillText(panel.nowText, exportW - pad, y);
    y += metaH;

    const plotCanvas = document.createElement('canvas');
    drawTrendChart(plotCanvas, panel.series, panel.fallback, panel.emptyText, {
      w: contentW,
      h: plotH,
      dpr,
      zeroLine: panel.zeroLine,
      seriesGapMs: panel.seriesGapMs,
      refLine: panel.refLine,
      refLineLabel: panel.refLineLabel,
    });
    ctx.drawImage(plotCanvas, pad, y, contentW, plotH);
    y += plotH + panelGap;
  }

  canvas.toBlob((blob) => {
    if (!blob) {
      if (trendsHint) {
        trendsHint.textContent = 'Не удалось сохранить картинку';
      }
      return;
    }
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `oxypulse_trends_${windowMin}min_${trendsFilenameStamp(now)}.png`;
    link.click();
    URL.revokeObjectURL(url);
  }, 'image/png');
}

function setChartWindow(sec) {
  chartWindowSec = sec;
  document.querySelectorAll('#trendsSection .window-pill').forEach((btn) => {
    btn.classList.toggle('active', Number(btn.dataset.window) === sec);
  });
  updateTrendsHint();
  drawAllCharts();
}

function isCpOpen() {
  return cpUi !== 'idle';
}

function isActionModalOpen() {
  return isCpOpen() || isPracticeOpen() || isHrvOpen();
}

function cancelMainChartRedraw() {
  if (chartRedrawTimer) {
    clearTimeout(chartRedrawTimer);
    chartRedrawTimer = null;
  }
}

function resumeMainCharts() {
  if (isActionModalOpen()) return;
  cancelMainChartRedraw();
  drawAllCharts();
  if (flowDynamicsActive) drawFlowDynamicsPanel();
}

function isCpLive() {
  return cpUi === 'recording' || cpUi === 'afterglow';
}

function clearCpHistory() {
  cpHistory.t = [];
  cpHistory.espMs = [];
  cpHistory.spo2 = [];
  cpHistory.hrWellue = [];
  cpHistory.hrCoospo = [];
}

function cpNowMs() {
  if (cpUi === 'done' && cpFrozenAt != null) return cpFrozenAt;
  return Date.now();
}

function cpElapsedSec(now = cpNowMs()) {
  if (cpStartedAt == null) return 0;
  return Math.max(0, Math.floor((now - cpStartedAt) / 1000));
}

function expandCpWindow(elapsedSec) {
  while (elapsedSec >= cpWindowSec - CP_EXPAND_LEAD_SEC) {
    cpWindowSec += CP_WINDOW_EXPAND_SEC;
  }
}

function cpHasSpo2() {
  return cpHistory.spo2.some((v) => v != null && Number.isFinite(v));
}

function stopCpTicker() {
  if (cpTicker) {
    clearInterval(cpTicker);
    cpTicker = null;
  }
}

function cpPoints(values) {
  return slidingPoints(cpHistory.t, values, cpNowMs(), cpWindowSec);
}

function slidingPoints(timestamps, values, nowMs, windowSec) {
  const start = nowMs - windowSec * 1000;
  const pts = [];
  for (let i = 0; i < timestamps.length; i += 1) {
    if (timestamps[i] >= start) {
      pts.push({ t: timestamps[i], v: values[i] });
    }
  }
  return pts;
}

function updateCpChrome() {
  const elapsed = cpElapsedSec();
  if (cpTimerWrap) {
    cpTimerWrap.hidden = cpUi === 'ready' || cpUi === 'idle';
  }
  if (cpTimer) {
    if (cpUi === 'afterglow' && cpStopAt != null) {
      const rec = Math.min(
        CP_AFTERGLOW_SEC,
        Math.max(0, Math.floor((Date.now() - cpStopAt) / 1000)),
      );
      cpTimer.textContent = `КП ${cpDurationSec} сек · восстановление ${rec} / ${CP_AFTERGLOW_SEC}`;
    } else if (cpUi === 'done') {
      cpTimer.textContent = `КП ${cpDurationSec} сек`;
    } else if (cpUi === 'recording') {
      cpTimer.textContent = `КП ${elapsed} сек`;
    }
  }
  if (cpStatus) {
    if (cpUi === 'ready') {
      cpStatus.textContent = 'Нажмите СТАРТ — замер начнётся после задержки дыхания. Тренды на странице не обновляются.';
    } else if (cpUi === 'recording') {
      cpStatus.textContent = 'Задержите дыхание. Тренды на странице не обновляются.';
    } else if (cpUi === 'afterglow') {
      cpStatus.textContent = 'Конец замера отмечен (голубая линия). Ещё 45 с восстановления — можно сохранить.';
    } else if (cpUi === 'done') {
      cpStatus.textContent = 'Замер завершён. Сохраните картинку или закройте окно.';
    } else {
      cpStatus.textContent = '—';
    }
  }
  if (btnCpStart) btnCpStart.hidden = cpUi !== 'ready';
  if (btnCpFinish) btnCpFinish.hidden = cpUi !== 'recording';
  const after = cpUi === 'afterglow' || cpUi === 'done';
  if (btnCpSave) {
    btnCpSave.hidden = !after;
    btnCpSave.disabled = cpHistory.t.length === 0;
  }
}

function drawCpCharts() {
  const now = cpNowMs();
  const optsBase = {
    windowSec: cpWindowSec,
    now,
    timeGridSec: 10,
    timeOrigin: cpStartedAt,
    markerT: cpStopAt,
    markerColor: '#38bdf8',
  };
  const spo2Pts = cpPoints(cpHistory.spo2);
  const spo2Vals = valuesOf(spo2Pts);
  if (cpSpo2Now) cpSpo2Now.textContent = formatRange(spo2Vals, (v) => `${v.toFixed(0)} %`) || '—';
  drawTrendChart(
    cpChartSpo2,
    [{ pts: spo2Pts, color: '#4ade80' }],
    { min: 90, max: 100, format: (v) => v.toFixed(0) },
    'нет SpO₂',
    optsBase,
  );

  const hrW = cpPoints(cpHistory.hrWellue);
  const hrC = cpPoints(cpHistory.hrCoospo);
  const parts = [];
  const lastC = lastFinite(hrC.map((p) => p.v));
  const lastW = lastFinite(hrW.map((p) => p.v));
  if (lastC != null) parts.push(`ремень ${lastC.toFixed(0)}`);
  if (lastW != null) parts.push(`Wellue ${lastW.toFixed(0)}`);
  if (cpHrNow) cpHrNow.textContent = parts.length ? `${parts.join(' · ')} bpm` : '—';
  const hrSeries = [];
  if (hrC.some((p) => p.v != null)) hrSeries.push({ pts: hrC, color: '#f87171' });
  if (hrW.some((p) => p.v != null)) hrSeries.push({ pts: hrW, color: '#fb923c', dashed: true });
  drawTrendChart(
    cpChartHr,
    hrSeries,
    { min: 50, max: 140, format: (v) => v.toFixed(0) },
    'нет ЧСС',
    optsBase,
  );
}

function tickCpModal() {
  if (!isCpOpen()) {
    stopCpTicker();
    return;
  }
  if (cpUi === 'afterglow' && cpStopAt != null
      && (Date.now() - cpStopAt) >= CP_AFTERGLOW_SEC * 1000) {
    cpUi = 'done';
    cpFrozenAt = Date.now();
    stopCpTicker();
  }
  if (isCpLive()) {
    expandCpWindow(cpElapsedSec());
  }
  updateCpChrome();
  drawCpCharts();
}

function feedCpFromStatus(data) {
  if (!isCpLive()) return;
  const s = data.sensors || {};
  const wellue = s.wellue || {};
  const coospo = s.coospo || {};
  const wellueLive = !!wellue.ok && wellue.contact !== false;
  const coospoLive = !!coospo.ok && coospo.connected !== false && coospo.contact !== false;
  const espMs = Number(data.uptime_ms);
  const nowEsp = Number.isFinite(espMs) ? espMs : null;
  const lastEsp = lastArrEspMs(cpHistory.espMs);
  if (nowEsp != null && lastEsp != null && (nowEsp - lastEsp) < 500) return;
  const t = wallFromEsp(nowEsp, nowEsp);
  insertParallel(cpHistory, {
    t,
    espMs: nowEsp,
    spo2: wellueLive ? saneSpo2(wellue.spo2) : null,
    hrWellue: wellueLive ? saneHr(wellue.hr) : null,
    hrCoospo: coospoLive ? saneHr(coospo.bpm) : null,
  });
  expandCpWindow(cpElapsedSec(t));
  updateCpChrome();
  drawCpCharts();
}

function openCpModal() {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  if (!cpModal || isCpOpen()) return;
  if (isPracticeOpen()) closePracticeModal();
  closeHrvIfOpen();
  clearCpHistory();
  cancelMainChartRedraw();
  cpUi = 'ready';
  cpStartedAt = null;
  cpStopAt = null;
  cpFrozenAt = null;
  cpDurationSec = 0;
  cpWindowSec = CP_WINDOW_INITIAL_SEC;
  cpModal.hidden = false;
  cpModal.setAttribute('aria-hidden', 'false');
  stopCpTicker();
  updateCpChrome();
  requestAnimationFrame(() => {
    drawCpCharts();
  });
}

function startCpMeasure() {
  if (cpUi !== 'ready') return;
  cpUi = 'recording';
  cpStartedAt = Date.now();
  cpStopAt = null;
  cpFrozenAt = null;
  cpDurationSec = 0;
  cpWindowSec = CP_WINDOW_INITIAL_SEC;
  stopCpTicker();
  cpTicker = setInterval(tickCpModal, 1000);
  enableSessionTrendSources(SESSION_SOURCES_CP);
  updateCpChrome();
  requestAnimationFrame(() => {
    drawCpCharts();
  });
}

function closeCpModal() {
  stopCpTicker();
  cpUi = 'idle';
  cpStartedAt = null;
  cpStopAt = null;
  cpFrozenAt = null;
  cpDurationSec = 0;
  cpWindowSec = CP_WINDOW_INITIAL_SEC;
  clearCpHistory();
  if (cpModal) {
    cpModal.hidden = true;
    cpModal.setAttribute('aria-hidden', 'true');
  }
  restoreSessionTrendSources();
  resumeMainCharts();
}

function finishCpMeasure() {
  if (cpUi !== 'recording') return;
  cpDurationSec = cpElapsedSec();
  cpStopAt = Date.now();
  if (cpHasSpo2()) {
    cpUi = 'afterglow';
  } else {
    cpFrozenAt = Date.now();
    cpUi = 'done';
    stopCpTicker();
  }
  updateCpChrome();
  drawCpCharts();
}

function saveCpImage() {
  if (!cpHistory.t.length) {
    if (cpStatus) cpStatus.textContent = 'Нечего сохранять — нет точек';
    return;
  }
  const stampDate = new Date();
  const dur = cpUi === 'recording' ? cpElapsedSec() : cpDurationSec;
  const now = cpNowMs();
  const pad = TREND_EXPORT_PAD;
  const headerH = 72;
  const metaH = 26;
  const plotH = 120;
  const panelGap = 12;
  const dpr = 2;
  const exportW = 560;
  const cpChartOpts = {
    w: contentW,
    h: plotH,
    dpr,
    windowSec: cpWindowSec,
    now,
    timeGridSec: 10,
    timeOrigin: cpStartedAt,
    markerT: cpStopAt,
    markerColor: '#38bdf8',
  };
  const panels = [
    {
      title: 'SpO₂',
      nowText: cpSpo2Now?.textContent || '—',
      series: [{ pts: cpPoints(cpHistory.spo2), color: '#4ade80' }],
      fallback: { min: 90, max: 100, format: (v) => v.toFixed(0) },
      emptyText: 'нет SpO₂',
    },
    {
      title: 'ЧСС',
      nowText: cpHrNow?.textContent || '—',
      series: [
        { pts: cpPoints(cpHistory.hrCoospo), color: '#f87171' },
        { pts: cpPoints(cpHistory.hrWellue), color: '#fb923c', dashed: true },
      ],
      fallback: { min: 50, max: 140, format: (v) => v.toFixed(0) },
      emptyText: 'нет ЧСС',
    },
  ];
  const exportH = pad + headerH + panels.length * (metaH + plotH) + panelGap + pad;
  const canvas = document.createElement('canvas');
  const { ctx, w, h } = setupCanvas(canvas, exportW, exportH, dpr);
  ctx.fillStyle = '#1c1c1c';
  ctx.fillRect(0, 0, w, h);
  let y = pad;
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 18px system-ui, sans-serif';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText('OxyPulse · КП Бутейко', pad, y);
  y += 26;
  ctx.fillStyle = '#fbbf24';
  ctx.font = 'bold 14px system-ui, sans-serif';
  ctx.fillText(`КП Бутейко: ${dur} сек`, pad, y);
  y += 22;
  ctx.fillStyle = '#888888';
  ctx.font = '13px system-ui, sans-serif';
  ctx.fillText(trendsExportStamp(stampDate), pad, y);
  y += headerH - 48;
  const contentW = exportW - pad * 2;
  for (const panel of panels) {
    ctx.fillStyle = '#dddddd';
    ctx.font = 'bold 14px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(panel.title, pad, y);
    ctx.fillStyle = '#aaaaaa';
    ctx.font = '13px system-ui, sans-serif';
    ctx.textAlign = 'right';
    ctx.fillText(panel.nowText, exportW - pad, y);
    y += metaH;
    const plotCanvas = document.createElement('canvas');
    drawTrendChart(plotCanvas, panel.series, panel.fallback, panel.emptyText, cpChartOpts);
    ctx.drawImage(plotCanvas, pad, y, contentW, plotH);
    y += plotH + panelGap;
  }
  canvas.toBlob((blob) => {
    if (!blob) {
      if (cpStatus) cpStatus.textContent = 'Не удалось сохранить картинку';
      return;
    }
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `oxypulse_kp_${dur}s_${trendsFilenameStamp(stampDate)}.png`;
    link.click();
    URL.revokeObjectURL(url);
  }, 'image/png');
}

function emptyPracticeShown() {
  return {
    spo2: false,
    hr: false,
    rr: false,
    ve: false,
    flow: false,
    o2: false,
    co2: false,
  };
}

function isPracticeOpen() {
  return practiceUi !== 'idle';
}

function isPracticeLive() {
  return practiceUi === 'recording' || practiceUi === 'afterglow';
}

function isHrvOpen() {
  return !!(hrvModal && !hrvModal.hidden);
}

function closeHrvIfOpen() {
  if (!isHrvOpen()) return;
  document.getElementById('btnHrvClose')?.click();
}

function sensorsFromStatus(data) {
  const s = data?.sensors || {};
  return {
    wellue: s.wellue || {},
    coospo: s.coospo || {},
    flow: (s.working && s.working.flow) || s.flow || {},
    workO2: (s.working && s.working.o2) || {},
    co2: (s.working && s.working.co2) || s.co2 || {},
  };
}

function isWellueLive(wellue) {
  return !!wellue?.ok && !!wellue.contact;
}

function isBeltLive(coospo) {
  return !!coospo?.ok && coospo.connected !== false && coospo.contact !== false;
}

function anyPracticeShown() {
  return Object.values(practiceShown).some(Boolean);
}

function resetPracticeSession() {
  practiceStartedAt = null;
  practiceStopAt = null;
  practiceFrozenAt = null;
  practiceDurationSec = 0;
  practiceWindowSec = PRACTICE_WINDOW_INITIAL_SEC;
  practiceHrBelt = false;
  Object.assign(practiceShown, emptyPracticeShown());
  practiceHistory.t = [];
  practiceHistory.espMs = [];
  practiceHistory.spo2 = [];
  practiceHistory.hr = [];
  practiceHistory.rr = [];
  practiceHistory.ve = [];
  practiceHistory.o2 = [];
  practiceHistory.co2 = [];
  practiceHistory.co2Raw = [];
  practiceFlow.t = [];
  practiceFlow.espMs = [];
  practiceFlow.lpm = [];
}

function practiceNowMs() {
  if (practiceUi === 'done' && practiceFrozenAt != null) return practiceFrozenAt;
  return Date.now();
}

function practiceElapsedSec(now = practiceNowMs()) {
  if (practiceStartedAt == null) return 0;
  return Math.max(0, Math.floor((now - practiceStartedAt) / 1000));
}

function expandPracticeWindow(elapsedSec) {
  while (elapsedSec >= practiceWindowSec * PRACTICE_EXPAND_FILL) {
    practiceWindowSec = Math.max(
      practiceWindowSec + 1,
      Math.round(practiceWindowSec * PRACTICE_EXPAND_FACTOR),
    );
  }
}

function formatPracticeTimer(sec) {
  if (sec < 60) return String(sec);
  return formatMmSs(sec);
}

function stopPracticeTicker() {
  if (practiceTicker) {
    clearInterval(practiceTicker);
    practiceTicker = null;
  }
}

function practicePts(values, timestamps = practiceHistory.t) {
  if (!values) return [];
  return slidingPoints(timestamps, values, practiceNowMs(), practiceWindowSec);
}

function practiceFlowPts() {
  return slidingPoints(practiceFlow.t, practiceFlow.lpm, practiceNowMs(), practiceWindowSec);
}

function practiceMarkers() {
  const markers = [];
  if (practiceStartedAt == null) return markers;
  const elapsedMs = practiceNowMs() - practiceStartedAt;
  const fiveMs = PRACTICE_MARK_SEC * 1000;
  const tenMs = PRACTICE_MARK_BRIGHT_SEC * 1000;
  for (let t = fiveMs; t <= elapsedMs + 1; t += fiveMs) {
    const isTen = t % tenMs === 0;
    markers.push({
      t: practiceStartedAt + t,
      color: isTen ? 'rgba(255,255,255,0.55)' : 'rgba(255,255,255,0.28)',
      width: isTen ? 1.5 : 1,
    });
  }
  if (practiceStopAt != null) {
    markers.push({
      t: practiceStopAt,
      color: '#38bdf8',
      width: 1.6,
      dash: [4, 3],
    });
  }
  return markers;
}

function practiceChartOpts() {
  const now = practiceNowMs();
  return {
    windowSec: practiceWindowSec,
    now,
    timeGridSec: 10,
    timeOrigin: practiceStartedAt,
    markerT: practiceStopAt,
    markerColor: '#38bdf8',
    markers: practiceMarkers(),
  };
}

function updatePracticeVisibility(data) {
  const { wellue, coospo, flow, workO2, co2 } = sensorsFromStatus(data);
  if (isWellueLive(wellue)) practiceShown.spo2 = true;
  if (isBeltLive(coospo) || isWellueLive(wellue)) practiceShown.hr = true;
  if (isBeltLive(coospo)) {
    practiceShown.rr = true;
    practiceHrBelt = true;
  }
  if (flow.ok) {
    practiceShown.ve = true;
    practiceShown.flow = true;
  }
  if (workO2.ok) practiceShown.o2 = true;
  if (co2.ok) practiceShown.co2 = true;
}

function updatePracticeChrome() {
  const recording = practiceUi === 'recording';
  const after = practiceUi === 'afterglow' || practiceUi === 'done';
  if (practiceTimerWrap) {
    practiceTimerWrap.hidden = practiceUi === 'ready' || practiceUi === 'idle';
  }
  if (practiceTimer) {
    const shown = after ? practiceDurationSec : practiceElapsedSec();
    practiceTimer.textContent = formatPracticeTimer(shown);
  }
  if (practiceAfterglow) {
    if (practiceUi === 'afterglow' && practiceStopAt != null) {
      const rec = Math.min(
        PRACTICE_AFTERGLOW_SEC,
        Math.max(0, Math.floor((Date.now() - practiceStopAt) / 1000)),
      );
      const left = Math.max(0, PRACTICE_AFTERGLOW_SEC - rec);
      practiceAfterglow.hidden = false;
      practiceAfterglow.textContent = `ещё ${left} с`;
    } else if (practiceUi === 'done') {
      practiceAfterglow.hidden = false;
      practiceAfterglow.textContent = 'Практика завершена';
    } else {
      practiceAfterglow.hidden = true;
      practiceAfterglow.textContent = '';
    }
  }
  if (btnPracticeStart) btnPracticeStart.hidden = practiceUi !== 'ready';
  if (btnPracticeFinish) btnPracticeFinish.hidden = !recording;
  if (btnPracticeSave) {
    btnPracticeSave.hidden = !after;
    btnPracticeSave.disabled = practiceHistory.t.length === 0 && practiceFlow.t.length === 0;
  }
  if (btnPracticeRepeat) btnPracticeRepeat.hidden = !after;
  if (practiceEmptyHint) {
    practiceEmptyHint.hidden = anyPracticeShown();
  }
}

function drawPracticeCharts() {
  const opts = practiceChartOpts();
  setPanelHidden('practicePanelSpo2', !practiceShown.spo2);
  setPanelHidden('practicePanelHr', !practiceShown.hr);
  setPanelHidden('practicePanelRr', !practiceShown.rr);
  setPanelHidden('practicePanelVe', !practiceShown.ve);
  setPanelHidden('practicePanelFlow', !practiceShown.flow);
  setPanelHidden('practicePanelO2', !practiceShown.o2);
  setPanelHidden('practicePanelCo2', !practiceShown.co2);
  if (practiceEmptyHint) practiceEmptyHint.hidden = anyPracticeShown();

  if (practiceShown.spo2) {
    const pts = practicePts(practiceHistory.spo2);
    const vals = valuesOf(pts);
    if (practiceSpo2Now) {
      practiceSpo2Now.textContent = formatRange(vals, (v) => `${v.toFixed(0)} %`) || '—';
    }
    drawTrendChart(
      practiceChartSpo2,
      [{ pts, color: '#4ade80' }],
      { min: 90, max: 100, format: (v) => v.toFixed(0) },
      'нет SpO₂',
      opts,
    );
  }

  if (practiceShown.hr) {
    const pts = practicePts(practiceHistory.hr);
    const last = lastFinite(pts.map((p) => p.v));
    const useBelt = practiceHrBelt;
    if (practiceHrLegendBeltWrap) practiceHrLegendBeltWrap.hidden = !useBelt;
    if (practiceHrLegendWellueWrap) practiceHrLegendWellueWrap.hidden = useBelt;
    if (practiceHrLegendBelt) practiceHrLegendBelt.textContent = beltDisplayName;
    if (practiceHrNow) {
      practiceHrNow.textContent = last != null ? `${last.toFixed(0)} bpm` : '—';
    }
    drawTrendChart(
      practiceChartHr,
      [{ pts, color: useBelt ? '#f87171' : '#fb923c', dashed: !useBelt }],
      { min: 50, max: 140, format: (v) => v.toFixed(0) },
      'нет ЧСС',
      opts,
    );
  }

  if (practiceShown.rr) {
    const pts = practicePts(practiceHistory.rr);
    const vals = valuesOf(pts);
    if (practiceRrNow) {
      practiceRrNow.textContent = formatRange(vals, (v) => `${v.toFixed(0)} мс`) || '—';
    }
    drawTrendChart(
      practiceChartRr,
      [{ pts, color: '#f87171' }],
      { min: 400, max: 1200, format: (v) => v.toFixed(0) },
      'нет R-R',
      opts,
    );
  }

  if (practiceShown.ve) {
    const pts = practicePts(practiceHistory.ve);
    const vals = valuesOf(pts);
    if (practiceVeNow) {
      practiceVeNow.textContent = formatRange(vals, (v) => `${v.toFixed(1)} л/мин`) || '—';
    }
    drawTrendChart(
      practiceChartVe,
      [{ pts, color: '#a78bfa' }],
      { min: 0, max: 30, format: (v) => v.toFixed(1) },
      'нет данных',
      opts,
    );
  }

  if (practiceShown.flow) {
    const pts = practiceFlowPts();
    const vals = valuesOf(pts);
    const last = lastFinite(vals);
    const peak = vals.length ? Math.max(...vals.map((v) => Math.abs(v))) : null;
    const parts = [];
    if (last != null) parts.push(`${last.toFixed(2)} л/мин`);
    if (peak != null) parts.push(`пик ${peak.toFixed(2)}`);
    if (practiceFlowNow) practiceFlowNow.textContent = parts.length ? parts.join(' · ') : '—';
    drawTrendChart(
      practiceChartFlow,
      [{ pts, color: '#a78bfa', maxGapMs: POLL_MS_BASE * 2.5 }],
      { min: -15, max: 15, format: (v) => v.toFixed(1) },
      'нет потока',
      { ...opts, zeroLine: true, seriesGapMs: POLL_MS_BASE * 2.5 },
    );
  }

  if (practiceShown.o2) {
    const pts = practicePts(practiceHistory.o2);
    const vals = valuesOf(pts);
    if (practiceO2Now) {
      practiceO2Now.textContent = formatRange(vals, (v) => `${v.toFixed(1)} %`) || '—';
    }
    drawTrendChart(
      practiceChartO2,
      [{ pts, color: '#67e8f9' }],
      { min: 18, max: 22, format: (v) => v.toFixed(1) },
      'нет O₂',
      opts,
    );
  }

  if (practiceShown.co2) {
    const co2EstPts = interpolatePtsLinear(mapCo2Pts(practicePts(practiceHistory.co2)));
    const co2RawPts = co2DynamicComp
      ? interpolatePtsLinear(mapCo2Pts(practicePts(practiceHistory.co2Raw)))
      : [];
    const series = [{ pts: co2EstPts, color: '#60a5fa' }];
    if (co2DynamicComp && co2RawPts.length) {
      series.push({ pts: co2RawPts, color: '#93c5fd', dashed: true });
    }
    const estVals = valuesOf(co2EstPts);
    if (practiceCo2Now) {
      practiceCo2Now.textContent = formatRange(estVals, (v) => `${roundCo2Pct(v).toFixed(2)} %`) || '—';
    }
    if (practiceCo2LegendWrap) {
      practiceCo2LegendWrap.hidden = !co2DynamicComp;
    }
    drawTrendChart(
      practiceChartCo2,
      series,
      { min: 0.04, max: 0.2, format: (v) => v.toFixed(2) },
      'нет CO₂',
      opts,
    );
  }
}

function tickPracticeModal() {
  if (!isPracticeOpen()) {
    stopPracticeTicker();
    return;
  }
  if (practiceUi === 'afterglow' && practiceStopAt != null
      && (Date.now() - practiceStopAt) >= PRACTICE_AFTERGLOW_SEC * 1000) {
    practiceUi = 'done';
    practiceFrozenAt = Date.now();
    stopPracticeTicker();
    syncFlowDynamicsPoller();
  }
  if (isPracticeLive()) {
    expandPracticeWindow(practiceElapsedSec());
  }
  updatePracticeChrome();
  drawPracticeCharts();
}

function practiceHrSample(wellue, coospo) {
  if (practiceHrBelt || isBeltLive(coospo)) {
    if (isBeltLive(coospo)) practiceHrBelt = true;
    return isBeltLive(coospo) ? saneHr(coospo.bpm) : null;
  }
  return isWellueLive(wellue) ? saneHr(wellue.hr) : null;
}

function feedPracticeFromStatus(data) {
  if (!isPracticeOpen()) return;
  updatePracticeVisibility(data);
  if (!isPracticeLive()) {
    updatePracticeChrome();
    drawPracticeCharts();
    return;
  }
  const { wellue, coospo, flow, workO2, co2 } = sensorsFromStatus(data);
  const espMs = Number(data.uptime_ms);
  const nowEsp = Number.isFinite(espMs) ? espMs : null;
  const lastEsp = lastArrEspMs(practiceHistory.espMs);
  if (nowEsp != null && lastEsp != null && (nowEsp - lastEsp) < 500) {
    updatePracticeChrome();
    drawPracticeCharts();
    return;
  }
  const t = wallFromEsp(nowEsp, nowEsp);
  insertParallel(practiceHistory, {
    t,
    espMs: nowEsp,
    spo2: isWellueLive(wellue) ? saneSpo2(wellue.spo2) : null,
    hr: practiceHrSample(wellue, coospo),
    rr: isBeltLive(coospo) ? saneRr(coospo.rr_ms) : null,
    ve: finiteOrNull(!!flow.ok, flow.ve_lpm),
    o2: finiteOrNull(!!workO2.ok, workO2.percent),
    co2: finiteOrNull(!!co2.ok, co2PercentFromObj(co2)),
    co2Raw: co2DynamicComp ? finiteOrNull(!!co2.ok, co2RawPercentFromObj(co2)) : null,
  });
  pushPracticeFlowSample(flow, t, nowEsp);
  expandPracticeWindow(practiceElapsedSec(t));
  updatePracticeChrome();
  drawPracticeCharts();
}

function pushPracticeFlowSample(flow, t, espMs) {
  if (!isPracticeLive()) return;
  if (flow?.ok) {
    practiceShown.flow = true;
    practiceShown.ve = true;
  }
  const nowEsp = Number.isFinite(espMs) ? espMs : null;
  const lastEsp = lastArrEspMs(practiceFlow.espMs);
  if (nowEsp != null && lastEsp != null && (nowEsp - lastEsp) < 500) return;
  const wall = t != null ? t : wallFromEsp(nowEsp, nowEsp);
  insertParallel(practiceFlow, {
    t: wall,
    espMs: nowEsp,
    lpm: flow?.ok ? flowLpmFromStatus(flow) : null,
  });
}

function openPracticeModal() {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  if (!practiceModal || isPracticeOpen()) return;
  if (isCpOpen()) closeCpModal();
  closeHrvIfOpen();
  resetPracticeSession();
  cancelMainChartRedraw();
  practiceUi = 'ready';
  practiceModal.hidden = false;
  practiceModal.setAttribute('aria-hidden', 'false');
  if (lastStatus) updatePracticeVisibility(lastStatus);
  updatePracticeChrome();
  requestAnimationFrame(() => {
    drawPracticeCharts();
  });
}

function closePracticeModal() {
  stopPracticeTicker();
  practiceUi = 'idle';
  resetPracticeSession();
  if (practiceModal) {
    practiceModal.hidden = true;
    practiceModal.setAttribute('aria-hidden', 'true');
  }
  syncFlowDynamicsPoller();
  restoreSessionTrendSources();
  resumeMainCharts();
}

function startPractice() {
  if (practiceUi !== 'ready') return;
  practiceUi = 'recording';
  practiceStartedAt = Date.now();
  practiceStopAt = null;
  practiceFrozenAt = null;
  practiceDurationSec = 0;
  practiceWindowSec = PRACTICE_WINDOW_INITIAL_SEC;
  stopPracticeTicker();
  practiceTicker = setInterval(tickPracticeModal, 1000);
  syncFlowDynamicsPoller();
  enableSessionTrendSources(SESSION_SOURCES_PRACTICE);
  updatePracticeChrome();
  requestAnimationFrame(() => {
    drawPracticeCharts();
  });
}

function finishPractice() {
  if (practiceUi !== 'recording') return;
  practiceDurationSec = practiceElapsedSec();
  practiceStopAt = Date.now();
  practiceUi = 'afterglow';
  updatePracticeChrome();
  drawPracticeCharts();
}

function repeatPractice() {
  if (practiceUi !== 'afterglow' && practiceUi !== 'done') return;
  stopPracticeTicker();
  resetPracticeSession();
  practiceUi = 'ready';
  if (lastStatus) updatePracticeVisibility(lastStatus);
  syncFlowDynamicsPoller();
  updatePracticeChrome();
  drawPracticeCharts();
}

function savePracticeImage() {
  if (practiceHistory.t.length === 0 && practiceFlow.t.length === 0) {
    if (practiceAfterglow) {
      practiceAfterglow.hidden = false;
      practiceAfterglow.textContent = 'Нечего сохранять — нет точек';
    }
    return;
  }
  const stampDate = new Date();
  const dur = practiceUi === 'recording' ? practiceElapsedSec() : practiceDurationSec;
  const pad = TREND_EXPORT_PAD;
  const headerH = 72;
  const metaH = 26;
  const plotH = 112;
  const panelGap = 12;
  const dpr = 2;
  const exportW = 720;
  const contentW = exportW - pad * 2;
  const chartOpts = {
    ...practiceChartOpts(),
    w: contentW,
    h: plotH,
    dpr,
  };
  const panels = [];
  if (practiceShown.spo2) {
    panels.push({
      title: 'Сатурация (кольцо)',
      nowText: practiceSpo2Now?.textContent || '—',
      series: [{ pts: practicePts(practiceHistory.spo2), color: '#4ade80' }],
      fallback: { min: 90, max: 100, format: (v) => v.toFixed(0) },
      emptyText: 'нет SpO₂',
    });
  }
  if (practiceShown.hr) {
    panels.push({
      title: 'HR',
      nowText: practiceHrNow?.textContent || '—',
      series: [{
        pts: practicePts(practiceHistory.hr),
        color: practiceHrBelt ? '#f87171' : '#fb923c',
        dashed: !practiceHrBelt,
      }],
      fallback: { min: 50, max: 140, format: (v) => v.toFixed(0) },
      emptyText: 'нет ЧСС',
    });
  }
  if (practiceShown.rr) {
    panels.push({
      title: 'R-R',
      nowText: practiceRrNow?.textContent || '—',
      series: [{ pts: practicePts(practiceHistory.rr), color: '#f87171' }],
      fallback: { min: 400, max: 1200, format: (v) => v.toFixed(0) },
      emptyText: 'нет R-R',
    });
  }
  if (practiceShown.ve) {
    panels.push({
      title: 'Минутная вентиляция',
      nowText: practiceVeNow?.textContent || '—',
      series: [{ pts: practicePts(practiceHistory.ve), color: '#a78bfa' }],
      fallback: { min: 0, max: 30, format: (v) => v.toFixed(1) },
      emptyText: 'нет данных',
    });
  }
  if (practiceShown.flow) {
    panels.push({
      title: 'Поток',
      nowText: practiceFlowNow?.textContent || '—',
      series: [{ pts: practiceFlowPts(), color: '#a78bfa', maxGapMs: POLL_MS_BASE * 2.5 }],
      fallback: { min: -15, max: 15, format: (v) => v.toFixed(1) },
      emptyText: 'нет потока',
      extraOpts: { zeroLine: true, seriesGapMs: POLL_MS_BASE * 2.5 },
    });
  }
  if (practiceShown.o2) {
    panels.push({
      title: 'O₂ · рабочий',
      nowText: practiceO2Now?.textContent || '—',
      series: [{ pts: practicePts(practiceHistory.o2), color: '#67e8f9' }],
      fallback: { min: 18, max: 22, format: (v) => v.toFixed(1) },
      emptyText: 'нет O₂',
    });
  }
  if (practiceShown.co2) {
    const co2EstPts = interpolatePtsLinear(mapCo2Pts(practicePts(practiceHistory.co2)));
    const co2RawPts = co2DynamicComp
      ? interpolatePtsLinear(mapCo2Pts(practicePts(practiceHistory.co2Raw)))
      : [];
    const series = [{ pts: co2EstPts, color: '#60a5fa' }];
    if (co2DynamicComp && co2RawPts.length) {
      series.push({ pts: co2RawPts, color: '#93c5fd', dashed: true });
    }
    panels.push({
      title: 'CO₂ · рабочий',
      nowText: practiceCo2Now?.textContent || '—',
      series,
      fallback: { min: 0.04, max: 0.2, format: (v) => v.toFixed(2) },
      emptyText: 'нет CO₂',
    });
  }
  if (!panels.length) {
    if (practiceAfterglow) {
      practiceAfterglow.hidden = false;
      practiceAfterglow.textContent = 'Нечего сохранять — нет графиков';
    }
    return;
  }
  const exportH = pad + headerH + panels.length * (metaH + plotH)
    + Math.max(0, panels.length - 1) * panelGap + pad;
  const canvas = document.createElement('canvas');
  const { ctx, w, h } = setupCanvas(canvas, exportW, exportH, dpr);
  ctx.fillStyle = '#1c1c1c';
  ctx.fillRect(0, 0, w, h);
  let y = pad;
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 18px system-ui, sans-serif';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText('OxyPulse · Практика Бутейко', pad, y);
  y += 26;
  ctx.fillStyle = '#fbbf24';
  ctx.font = 'bold 14px system-ui, sans-serif';
  ctx.fillText(`Практика: ${formatPracticeTimer(dur)}`, pad, y);
  y += 22;
  ctx.fillStyle = '#888888';
  ctx.font = '13px system-ui, sans-serif';
  ctx.fillText(trendsExportStamp(stampDate), pad, y);
  y += headerH - 48;
  for (const panel of panels) {
    ctx.fillStyle = '#dddddd';
    ctx.font = 'bold 14px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(panel.title, pad, y);
    ctx.fillStyle = '#aaaaaa';
    ctx.font = '13px system-ui, sans-serif';
    ctx.textAlign = 'right';
    ctx.fillText(panel.nowText, exportW - pad, y);
    y += metaH;
    const plotCanvas = document.createElement('canvas');
    drawTrendChart(
      plotCanvas,
      panel.series,
      panel.fallback,
      panel.emptyText,
      { ...chartOpts, ...(panel.extraOpts || {}) },
    );
    ctx.drawImage(plotCanvas, pad, y, contentW, plotH);
    y += plotH + panelGap;
  }
  const m = Math.floor(dur / 60);
  const s = dur % 60;
  const durStamp = m > 0 ? `${m}m${pad2(s)}s` : `${s}s`;
  canvas.toBlob((blob) => {
    if (!blob) {
      if (practiceAfterglow) {
        practiceAfterglow.hidden = false;
        practiceAfterglow.textContent = 'Не удалось сохранить картинку';
      }
      return;
    }
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `oxypulse_buteyko_practice_${durStamp}_${trendsFilenameStamp(stampDate)}.png`;
    link.click();
    URL.revokeObjectURL(url);
  }, 'image/png');
}

function renderWifiLanHint(wifi) {
  if (!wifiLanHint) return;
  const sta = wifi?.sta || {};
  const apIp = wifi?.ap_ip || '192.168.4.1';
  const host = hostLabel();
  const onAp = !host || host === apIp || host.startsWith('192.168.4.');

  if (sta.connected && sta.ip) {
    wifiLanHint.hidden = false;
    if (onAp) {
      wifiLanHint.innerHTML = `В LAN: <a href="http://${sta.ip}/" target="_blank" rel="noopener">${sta.ip}</a> (${sta.ssid || 'WiFi'})`;
    } else {
      wifiLanHint.textContent = `STA: ${sta.ssid || '—'} · ${sta.ip} · RSSI ${sta.rssi ?? '—'} dBm`;
    }
    return;
  }
  if (sta.configured && sta.error) {
    wifiLanHint.hidden = false;
    wifiLanHint.textContent = `STA «${sta.ssid || '—'}»: ${sta.error}`;
    return;
  }
  wifiLanHint.hidden = true;
  wifiLanHint.textContent = '';
}

function formatWifiModalStatus(wifi) {
  if (!wifi) return 'AP для настройки · STA не задана';
  const sta = wifi.sta || {};
  const parts = [`AP ${wifi.ap_ssid || 'ESP32-Tester'} → ${wifi.ap_ip || '192.168.4.1'}`];
  if (sta.configured) {
    if (sta.connected) {
      parts.push(`STA «${sta.ssid}» → ${sta.ip} (${sta.rssi} dBm)`);
    } else {
      parts.push(`STA «${sta.ssid}»: ${sta.error || 'не подключена'}`);
    }
  } else {
    parts.push('STA не настроена');
  }
  return parts.join(' · ');
}

function openWifiModal() {
  if (!wifiModal) return;
  wifiModal.hidden = false;
  wifiModal.setAttribute('aria-hidden', 'false');
  if (wifiModalStatus && lastStatus) {
    wifiModalStatus.textContent = formatWifiModalStatus(lastStatus.wifi);
  }
  updateWifiSaveButton();
  renderWifiSelection();
}

function closeWifiModal() {
  if (!wifiModal) return;
  wifiModal.hidden = true;
  wifiModal.setAttribute('aria-hidden', 'true');
}

function getWifiTargetSsid() {
  const manual = wifiSsidManual?.value.trim() || '';
  if (manual) return manual;
  return selectedWifi?.ssid || '';
}

function updateWifiSaveButton() {
  if (!btnWifiSave) return;
  btnWifiSave.disabled = !getWifiTargetSsid() || !baseUrl;
}

function renderWifiSelection() {
  if (!wifiNetworkList) return;
  wifiNetworkList.querySelectorAll('.wifi-item').forEach((btn) => {
    const ssid = btn.dataset.ssid || '';
    btn.classList.toggle('selected', selectedWifi && selectedWifi.ssid === ssid);
  });
  updateWifiSaveButton();
}

function selectWifiNetwork(net) {
  selectedWifi = net;
  if (wifiSsidManual) wifiSsidManual.value = net.ssid;
  updateWifiSaveButton();
}

function renderWifiNetworkList(networks) {
  if (!wifiNetworkList) return;
  wifiNetworkList.innerHTML = '';
  for (const net of networks) {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'wifi-item';
    btn.dataset.ssid = net.ssid;
    const lock = net.secure ? '<span class="wifi-lock">защищена</span>' : 'открытая';
    btn.innerHTML = `<div>${net.ssid}</div><div class="wifi-item-meta"><span>${lock}</span><span>${net.rssi} dBm · ch ${net.channel ?? '—'}</span></div>`;
    btn.addEventListener('click', () => selectWifiNetwork({ ssid: net.ssid, secure: !!net.secure }));
    li.appendChild(btn);
    wifiNetworkList.appendChild(li);
  }
  renderWifiSelection();
}

async function fetchWifiScanStatus() {
  const res = await fetchWithTimeout(apiUrl('/api/wifi/scan'), {}, WIFI_SCAN_FETCH_TIMEOUT_MS);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function scanWifiNetworks() {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  btnWifiScan.disabled = true;
  if (wifiScanStatus) {
    wifiScanStatus.textContent = 'Запуск скана… AP может кратко моргнуть';
  }

  try {
    const startRes = await fetchWithTimeout(apiUrl('/api/wifi/scan'), { method: 'POST' }, FETCH_TIMEOUT_MS);
    if (!startRes.ok) throw new Error(`HTTP ${startRes.status}`);

    for (let i = 0; i < WIFI_SCAN_MAX_POLLS; i += 1) {
      await sleep(WIFI_SCAN_POLL_MS);
      let data;
      try {
        data = await fetchWifiScanStatus();
      } catch (e) {
        if (wifiScanStatus) {
          wifiScanStatus.textContent = `Скан… (переподключитесь к AP при обрыве) · ${e.message}`;
        }
        continue;
      }

      if (data.scanning) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Сканирование… ${i + 1}/${WIFI_SCAN_MAX_POLLS}`;
        continue;
      }

      const networks = Array.isArray(data.networks) ? data.networks : [];
      if (data.error) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Ошибка: ${data.error}`;
      } else if (networks.length) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Найдено: ${networks.length}`;
      } else {
        if (wifiScanStatus) wifiScanStatus.textContent = 'Сети не найдены — введите SSID вручную';
      }
      renderWifiNetworkList(networks);
      return;
    }
    if (wifiScanStatus) wifiScanStatus.textContent = 'Таймаут скана — введите SSID вручную или перезагрузите ESP';
  } catch (e) {
    if (wifiScanStatus) wifiScanStatus.textContent = `Ошибка: ${e.message}. Введите SSID вручную.`;
  } finally {
    btnWifiScan.disabled = false;
  }
}

async function postWifiConfig(body) {
  const res = await fetchWithTimeout(apiUrl('/api/wifi/config'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }, 8000);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function saveWifiConfig() {
  const ssid = getWifiTargetSsid();
  if (!ssid) return;
  try {
    btnWifiSave.disabled = true;
    btnWifiSave.textContent = 'Сохранение…';
    await postWifiConfig({
      ssid,
      password: wifiPassword?.value || '',
    });
    if (wifiScanStatus) wifiScanStatus.textContent = 'Перезагрузка ESP32…';
    closeWifiModal();
    pollingActive = false;
    if (pollTimer) clearInterval(pollTimer);
    linkState = 'offline';
    renderLinkBar();
  } catch (e) {
    alert(`WiFi: ${e.message}`);
    updateWifiSaveButton();
    btnWifiSave.textContent = 'Сохранить и перезагрузить';
  }
}

async function forgetWifiConfig() {
  if (!confirm('Удалить сохранённую WiFi-сеть и перезагрузить ESP32?')) return;
  try {
    btnWifiForget.disabled = true;
    await postWifiConfig({ clear: true });
    closeWifiModal();
    pollingActive = false;
    if (pollTimer) clearInterval(pollTimer);
  } catch (e) {
    alert(`WiFi: ${e.message}`);
  } finally {
    btnWifiForget.disabled = false;
  }
}

function renderBleTile(id, data, formatOk) {
  const connected = !!data.connected;
  const ok = !!data.ok;
  const error = data.error ? String(data.error) : '';
  if (!connected) {
    const text = error ? `поиск…\n${error}` : 'поиск…';
    renderSensorTile(id, false, text);
    return;
  }
  if (!ok) {
    const text = error ? `подключено — нет данных\n${error}` : 'подключено — нет данных';
    renderSensorTile(id, false, text);
    return;
  }
  renderSensorTile(id, true, formatOk(data));
}

function renderTelemetry(data) {
  lastUptimeMs = data.uptime_ms;
  renderServoStatus(data);

  const s = data.sensors || {};
  const hypoxia = s.hypoxia || {};
  const working = s.working || {};
  const o2 = hypoxia.o2 || s.o2 || {};
  const flow = working.flow || s.flow || {};
  const pressure = hypoxia.pressure || s.pressure || {};
  const co2 = working.co2 || s.co2 || {};
  co2DynamicComp = !!co2.dynamic_comp;
  const workO2 = working.o2 || {};
  const workPressure = working.pressure || {};

  if (pressure.baseline_ok && Number.isFinite(pressure.baseline_hpa)) {
    pressureBaselineHpa = roundPressure(Number(pressure.baseline_hpa));
  }
  if (workPressure.baseline_ok && Number.isFinite(workPressure.baseline_hpa)) {
    workPressureBaselineHpa = roundPressure(Number(workPressure.baseline_hpa));
  }

  const flowLpm = flowLpmFromStatus(flow);
  renderSensorTile(
    'sensorO2',
    !!o2.ok,
    o2.ok
      ? `O₂: ${Number(o2.percent || 0).toFixed(1)} % (${Number(o2.mv || 0).toFixed(2)} mV)`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorWorkO2',
    !!workO2.ok,
    workO2.ok
      ? `O₂: ${Number(workO2.percent || 0).toFixed(1)} % (${Number(workO2.mv || 0).toFixed(2)} mV)`
      : 'нет данных',
  );
  renderFlowPhase(flow.phase);
  pushFlowDynamicsSample(flow);
  renderSensorTile(
    'sensorFlow',
    !!flow.ok,
    flow.ok
      ? `Поток: ${(flowLpm ?? 0).toFixed(2)} л/мин\nВдох (>0): ${Number(flow.inhale_l || 0).toFixed(3)} л\nВыдох (<0): ${Number(flow.exhale_l || 0).toFixed(3)} л\nМинутная вентиляция (30 с): ${Number(flow.ve_lpm || 0).toFixed(1)} л/мин`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorPressure',
    !!pressure.ok,
    pressure.ok
      ? `${roundPressure(Number(pressure.hpa || 0)).toFixed(1)} hPa, ${Number(pressure.temp_c || 0).toFixed(1)} °C`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorWorkPressure',
    !!workPressure.ok,
    workPressure.ok
      ? `${roundPressure(Number(workPressure.hpa || 0)).toFixed(1)} hPa, ${Number(workPressure.temp_c || 0).toFixed(1)} °C`
      : 'нет данных',
  );
  const co2Warmup = co2.warming_up ? ' (прогрев)' : '';
  const co2Pct = co2PercentFromObj(co2);
  const co2RawPct = co2RawPercentFromObj(co2);
  const co2RawLine = co2DynamicComp && co2RawPct != null
    ? `\nДатчик: ${co2RawPct.toFixed(2)} % (${Number(co2.ppm_raw || 0)} ppm)`
    : '';
  const tempShow = co2DynamicComp && Number.isFinite(Number(co2.temp_c_est))
    ? Number(co2.temp_c_est)
    : Number(co2.temp_c || 0);
  const rhShow = co2DynamicComp && Number.isFinite(Number(co2.rh_est))
    ? Number(co2.rh_est)
    : Number(co2.rh || 0);
  const tempRawLine = co2DynamicComp
    ? ` (датчик ${Number(co2.temp_c || 0).toFixed(1)})`
    : '';
  const rhRawLine = co2DynamicComp
    ? ` (датчик ${Number(co2.rh || 0).toFixed(1)})`
    : '';
  renderSensorTile(
    'sensorCo2',
    !!co2.ok,
    co2.ok
      ? `CO₂: ${Number(co2Pct || 0).toFixed(2)} % (${Number(co2.ppm || 0)} ppm${co2DynamicComp ? ', оценка' : ''})${co2Warmup}${co2RawLine}\nT: ${tempShow.toFixed(1)} °C${tempRawLine}\nRH: ${rhShow.toFixed(1)} %${rhRawLine}`
      : 'нет данных',
  );

  const wellue = s.wellue || {};
  const coospo = s.coospo || {};
  beltDisplayName = (coospo.name && String(coospo.name).trim()) || 'Нагрудный HR';
  if (hrBeltTitle) hrBeltTitle.textContent = beltDisplayName;
  renderBleTile('sensorWellue', wellue, (d) => (
    `${d.name || 'Wellue'}\nSpO₂: ${Number(d.spo2 || 0)} %\nЧСС: ${Number(d.hr || 0)} bpm\nБатарея: ${Number(d.battery || 0)} %\nКонтакт: ${d.contact ? 'да' : 'нет'}\nMotion: ${Number(d.motion || 0)}`
  ));
  renderBleTile('sensorCoospo', coospo, (d) => {
    const rr = Number(d.rr_ms || 0);
    const rrLine = rr > 0 ? `\nR-R: ${rr} мс` : '';
    return `${d.name || beltDisplayName}\nЧСС: ${Number(d.bpm || 0)} bpm${rrLine}\nКонтакт: ${d.contact ? 'да' : 'нет'}`;
  });
  if (btnHrvMeasure) {
    btnHrvMeasure.disabled = !coospo.connected;
  }
  if (anyTrendSource()) {
    pushHistory(data);
  }
  if (flowDynamicsActive) {
    const flow = (data.sensors?.working?.flow) || data.sensors?.flow || {};
    pushFlowDynamicsSample(flow);
  }
  syncFlowDynamicsPoller();
  feedCpFromStatus(data);
  feedPracticeFromStatus(data);
  updateTrendsHint();
  scheduleChartRedraw();
}

function applyStatus(data, { syncForm = false } = {}) {
  lastStatus = data;
  if (data.trends && data.trends.sources) {
    trendSources = parseSources(data.trends.sources);
    syncTrendButtons();
  }
  if (data.sensors) {
    const wc = data.sensors.working?.co2 || data.sensors.co2;
    if (wc && wc.dynamic_comp) co2DynamicComp = true;
  }
  initPinSelects(data);
  renderTelemetry(data);
  renderWifiLanHint(data.wifi);
  if (wifiModal && !wifiModal.hidden && wifiModalStatus) {
    wifiModalStatus.textContent = formatWifiModalStatus(data.wifi);
  }
  if (syncForm && !formLocked) {
    syncGpioForm(data);
    syncServoForm(data);
  }
}

async function poll({ syncForm = false } = {}) {
  if (!baseUrl || pollInFlight || statusPollPaused) return;
  pollInFlight = true;
  try {
    const recovered = lastPollFailed;
    const data = await fetchStatus();
    lastPollFailed = false;
    lastOkAt = Date.now();
    lastError = '';
    if (recovered) backfillPending = true;
    if (hydrateOnNextOk) {
      hydrateOnNextOk = false;
      const ok = await hydrateTrends();
      if (ok) backfillPending = false;
    } else if (
      backfillPending
      && shouldBackfillAfterGap(data)
      && (
        (Date.now() - lastHydrateAt) >= TRENDS_BACKFILL_DEBOUNCE_MS
        || isCpLive()
        || isPracticeLive()
      )
    ) {
      const ok = await hydrateTrends({
        fromEspMs: lastKnownEspMs(),
        maxPages: TRENDS_BACKFILL_MAX_PAGES,
        replace: false,
      });
      if (ok) backfillPending = false;
    } else if (backfillPending && !shouldBackfillAfterGap(data)) {
      backfillPending = false;
    }
    applyStatus(data, { syncForm });
  } catch (e) {
    lastPollFailed = true;
    lastError = e.message;
  } finally {
    pollInFlight = false;
    renderLinkBar();
  }
}

function startPolling() {
  pollingActive = true;
  lastOkAt = null;
  lastError = '';
  lastPollFailed = true;
  hydrateOnNextOk = true;
  if (pollTimer) clearInterval(pollTimer);
  startLinkUiTimer();
  renderLinkBar();
  poll({ syncForm: true });
  schedulePollInterval();
  syncFlowDynamicsPoller();
}

document.getElementById('btnConnect').addEventListener('click', () => {
  const host = hostInput.value.trim().replace(/\/$/, '');
  baseUrl = host.startsWith('http') ? host : `http://${host}`;
  lastPinsKey = '';
  updateWifiSaveButton();
  startPolling();
});

btnWifiSettings?.addEventListener('click', openWifiModal);
btnWifiClose?.addEventListener('click', closeWifiModal);
wifiModalBackdrop?.addEventListener('click', closeWifiModal);
wifiSsidManual?.addEventListener('input', () => {
  selectedWifi = null;
  renderWifiSelection();
});
btnWifiScan?.addEventListener('click', scanWifiNetworks);
btnWifiSave?.addEventListener('click', saveWifiConfig);
btnWifiForget?.addEventListener('click', forgetWifiConfig);

btnGpioApply.addEventListener('click', async () => {
  try {
    await sendCmd({
      cmd: 'gpio_set',
      pin: Number(gpioPin.value),
      state: gpioState.checked ? 1 : 0,
    });
    await poll({ syncForm: true });
  } catch (e) {
    alert(`GPIO: ${e.message}`);
  }
});

async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function cooldownTapButton(ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const leftSec = Math.max(1, Math.ceil((end - Date.now()) / 1000));
    btnGpioTap.textContent = `${leftSec} с…`;
    await sleep(250);
  }
}

btnGpioTap.addEventListener('click', async () => {
  if (btnGpioTap.disabled) return;

  btnGpioTap.disabled = true;
  gpioSyncLocked = true;
  gpioState.disabled = true;
  const tapLabel = btnGpioTap.textContent;

  try {
    await sendCmd({
      cmd: 'gpio_tap',
      pin: Number(gpioPin.value),
      pulse_ms: GPIO_TAP_PULSE_MS,
    });
    setGpioUiState(true, { pulsing: true });
    btnGpioTap.textContent = 'Тап…';

    await sleep(GPIO_TAP_PULSE_MS);
    setGpioUiState(false);
    btnGpioTap.textContent = 'OFF…';

    await cooldownTapButton(GPIO_TAP_COOLDOWN_MS);
  } catch (e) {
    alert(`GPIO tap: ${e.message}`);
  } finally {
    gpioState.disabled = false;
    gpioSyncLocked = false;
    btnGpioTap.textContent = tapLabel;
    renderLinkBar();
    await poll({ syncForm: true });
  }
});

btnFlowReset.addEventListener('click', async () => {
  try {
    await sendCmd({ cmd: 'flow_reset' });
    await poll({ syncForm: false });
  } catch (e) {
    alert(`Поток: ${e.message}`);
  }
});

btnCo2Frc?.addEventListener('click', async () => {
  if (!confirm(
    'FRC: датчик должен 3+ минуты стоять на свежем воздухе (~400 ppm). Выполнить калибровку?',
  )) {
    return;
  }
  try {
    btnCo2Frc.disabled = true;
    await sendCmd({ cmd: 'co2_frc', target_ppm: 400 });
    await poll({ syncForm: false });
  } catch (e) {
    alert(`CO₂ FRC: ${e.message}`);
  } finally {
    renderLinkBar();
  }
});

btnServoApply.addEventListener('click', async () => {
  try {
    if (servoAuto.checked) {
      await sendCmd({
        cmd: 'servo_auto',
        pin: Number(servoPin.value),
        enabled: true,
        pause_ms: 5000,
      });
    } else {
      await sendCmd({
        cmd: 'servo_set',
        pin: Number(servoPin.value),
        angle: Number(servoAngle.value),
      });
    }
    await poll({ syncForm: true });
  } catch (e) {
    alert(`Servo: ${e.message}`);
  }
});

window.addEventListener('offline', renderLinkBar);
window.addEventListener('online', () => {
  if (pollingActive) {
    poll({ syncForm: false });
  }
});

btnHrvMeasure?.addEventListener('click', async () => {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  if (btnHrvMeasure.disabled) return;
  if (isCpOpen()) closeCpModal();
  if (isPracticeOpen()) closePracticeModal();
  try {
    await loadHrvModule();
    window.OxyHrv?.open();
  } catch (e) {
    alert(`HRV: ${e.message}`);
  }
});

document.querySelectorAll('#trendsSection .window-pill').forEach((btn) => {
  if (!btn.dataset.window) return;
  btn.addEventListener('click', () => {
    setChartWindow(Number(btn.dataset.window) || 300);
  });
});

btnTrendsSave?.addEventListener('click', () => {
  saveTrendsImage();
});

btnCpMeasure?.addEventListener('click', () => {
  openCpModal();
});
btnCpClose?.addEventListener('click', () => {
  closeCpModal();
});
cpModalBackdrop?.addEventListener('click', () => {
  closeCpModal();
});
btnCpStart?.addEventListener('click', () => {
  startCpMeasure();
});
btnCpFinish?.addEventListener('click', () => {
  finishCpMeasure();
});
btnCpSave?.addEventListener('click', () => {
  saveCpImage();
});

btnPracticeMeasure?.addEventListener('click', () => {
  openPracticeModal();
});
btnPracticeClose?.addEventListener('click', () => {
  closePracticeModal();
});
practiceModalBackdrop?.addEventListener('click', () => {
  closePracticeModal();
});
btnPracticeStart?.addEventListener('click', () => {
  startPractice();
});
btnPracticeFinish?.addEventListener('click', () => {
  finishPractice();
});
btnPracticeSave?.addEventListener('click', () => {
  savePracticeImage();
});
btnPracticeRepeat?.addEventListener('click', () => {
  repeatPractice();
});

btnFlowDynamics?.addEventListener('click', () => {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  flowDynamicsActive = !flowDynamicsActive;
  saveHrViewFlag(FLOW_DYNAMICS_KEY, flowDynamicsActive);
  if (!flowDynamicsActive) clearFlowDynamics();
  syncFlowDynamicsPoller();
  scheduleChartRedraw();
});

async function postTrendSources(next) {
  const res = await fetchWithTimeout(apiUrl('/api/trends/sources'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(next),
  }, 8000);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

document.querySelectorAll('.btn-trend').forEach((btn) => {
  btn.addEventListener('click', async () => {
    if (!btn.dataset.src) return;
    if (!baseUrl) {
      alert('Сначала подключитесь к ESP32');
      return;
    }
    const key = btn.dataset.src;
    const next = { ...trendSources, [key]: !trendSources[key] };
    btn.disabled = true;
    try {
      await postTrendSources(next);
      trendSources = next;
      syncTrendButtons();
      schedulePollInterval();
      if (!anyTrendSource()) {
        clearHistory();
      } else {
        await hydrateTrends();
      }
      updateTrendsHint();
      drawAllCharts();
    } catch (e) {
      alert(`Тренд: ${e.message}`);
    } finally {
      btn.disabled = false;
    }
  });
});

if (hrViewBeltInput) {
  hrViewBeltInput.addEventListener('change', () => {
    hrViewBelt = !!hrViewBeltInput.checked;
    saveHrViewFlag(HR_VIEW_BELT_KEY, hrViewBelt);
    drawAllCharts();
  });
}
if (hrViewWellueInput) {
  hrViewWellueInput.addEventListener('change', () => {
    hrViewWellue = !!hrViewWellueInput.checked;
    saveHrViewFlag(HR_VIEW_WELLUE_KEY, hrViewWellue);
    drawAllCharts();
  });
}

syncHrViewInputs();

window.addEventListener('resize', () => {
  drawAllCharts();
  if (isCpOpen()) drawCpCharts();
  if (isPracticeOpen()) drawPracticeCharts();
  window.OxyHrv?.onResize();
});

updateTrendsHint();
drawAllCharts();

renderLinkBar();

if (location.hostname && location.hostname !== '') {
  hostInput.value = location.hostname;
  baseUrl = location.origin;
  startPolling();
}
