(() => {
  'use strict';

  const HRV_COUNTDOWN_SEC = 5;
  const HRV_MIN_RR = 15;
  const HRV_FETCH_TIMEOUT_MS = 45000;
  const HRV_POLL_FAIL_MAX = 5;
  const HRV_DEFAULT_SEC = 30;

  const deps = () => window.__oxy || {};

  const btnHrvMeasure = document.getElementById('btnHrvMeasure');
  const hrvModal = document.getElementById('hrvModal');
  const hrvModalBackdrop = document.getElementById('hrvModalBackdrop');
  const btnHrvClose = document.getElementById('btnHrvClose');
  const hrvStepSetup = document.getElementById('hrvStepSetup');
  const hrvStepCountdown = document.getElementById('hrvStepCountdown');
  const hrvStepRecording = document.getElementById('hrvStepRecording');
  const hrvStepResult = document.getElementById('hrvStepResult');
  const hrvStepError = document.getElementById('hrvStepError');
  const hrvCountdownNum = document.getElementById('hrvCountdownNum');
  const hrvRecordingStatus = document.getElementById('hrvRecordingStatus');
  const hrvRecordingMeta = document.getElementById('hrvRecordingMeta');
  const hrvProgressBar = document.getElementById('hrvProgressBar');
  const hrvResultTime = document.getElementById('hrvResultTime');
  const hrvResultNote = document.getElementById('hrvResultNote');
  const hrvMetricsGrid = document.getElementById('hrvMetricsGrid');
  const hrvPoincare = document.getElementById('hrvPoincare');
  const hrvErrorText = document.getElementById('hrvErrorText');
  const btnHrvStart = document.getElementById('btnHrvStart');
  const btnHrvCancel = document.getElementById('btnHrvCancel');
  const btnHrvAgain = document.getElementById('btnHrvAgain');
  const btnHrvSaveImage = document.getElementById('btnHrvSaveImage');
  const btnHrvErrorRetry = document.getElementById('btnHrvErrorRetry');
  const hrvCommentInput = document.getElementById('hrvComment');

  let hrvUiState = 'idle';
  let hrvDurationSec = HRV_DEFAULT_SEC;
  let hrvPollTimer = null;
  let hrvCountdownTimer = null;
  let hrvCountdownLeft = HRV_COUNTDOWN_SEC;
  let hrvClosing = false;
  let hrvPollFails = 0;
  let hrvPollInFlight = false;
  let lastHrvMetrics = null;
  let lastHrvResultStamp = '';
  let lastHrvResultNote = '';
  let lastHrvRecordedSec = HRV_DEFAULT_SEC;
  let inited = false;

  function apiUrl(path) {
    return deps().getApiUrl(path);
  }

  function fetchWithTimeout(url, options, timeoutMs) {
    return deps().fetchWithTimeout(url, options, timeoutMs);
  }

  function setupCanvas(canvas) {
    return deps().setupCanvas(canvas);
  }

  function getBaseUrl() {
    return deps().getBaseUrl();
  }

  function hrvPad2(n) {
    return String(n).padStart(2, '0');
  }

  function formatHrvStamp(date) {
    const d = date instanceof Date ? date : new Date();
    return `${d.getFullYear()}-${hrvPad2(d.getMonth() + 1)}-${hrvPad2(d.getDate())} ${hrvPad2(d.getHours())}:${hrvPad2(d.getMinutes())}:${hrvPad2(d.getSeconds())}`;
  }

  function formatHrvRemain(ms) {
    const total = Math.max(0, Math.ceil(ms / 1000));
    return `${Math.floor(total / 60)}:${hrvPad2(total % 60)}`;
  }

  function formatHrvDurationLabel(sec) {
    const n = Number(sec) || 0;
    if (n < 60) return `${n} сек`;
    if (n % 60 === 0) return `${n / 60} мин`;
    return `${Math.floor(n / 60)}:${hrvPad2(n % 60)}`;
  }

  function hrvFilenameStamp(date) {
    const d = date instanceof Date ? date : new Date();
    return `${d.getFullYear()}-${hrvPad2(d.getMonth() + 1)}-${hrvPad2(d.getDate())}_${hrvPad2(d.getHours())}-${hrvPad2(d.getMinutes())}-${hrvPad2(d.getSeconds())}`;
  }

  function prepareCanvas(canvas, cssW, cssH, dpr) {
    const ratio = dpr ?? (window.devicePixelRatio || 1);
    const w = Math.max(1, cssW);
    const h = Math.max(1, cssH);
    canvas.width = Math.round(w * ratio);
    canvas.height = Math.round(h * ratio);
    const ctx = canvas.getContext('2d');
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    return { ctx, w, h };
  }

  function wrapTextLines(ctx, text, maxWidth) {
    const words = String(text).trim().split(/\s+/);
    if (words.length === 0 || words[0] === '') return [];
    const lines = [];
    let line = words[0];
    for (let i = 1; i < words.length; i += 1) {
      const next = `${line} ${words[i]}`;
      if (ctx.measureText(next).width > maxWidth) {
        lines.push(line);
        line = words[i];
      } else {
        line = next;
      }
    }
    lines.push(line);
    return lines;
  }

  function mapHrvApiError(code) {
    if (code === 'belt_not_connected') return 'Ремень не подключён';
    if (code === 'already_running' || code === 'busy') return 'Замер уже идёт, подождите секунду';
    if (code === 'invalid_duration') return 'Неверная длительность';
    if (code === 'empty_body' || code === 'bad_json') return 'Некорректный запрос';
    return code ? String(code) : 'ошибка HRV';
  }

  function clearHrvTimers() {
    if (hrvCountdownTimer) {
      clearInterval(hrvCountdownTimer);
      hrvCountdownTimer = null;
    }
    if (hrvPollTimer) {
      clearInterval(hrvPollTimer);
      hrvPollTimer = null;
    }
  }

  function setHrvStep(step) {
    hrvUiState = step;
    const map = {
      setup: hrvStepSetup,
      countdown: hrvStepCountdown,
      recording: hrvStepRecording,
      result: hrvStepResult,
      error: hrvStepError,
    };
    Object.entries(map).forEach(([name, el]) => {
      if (el) el.hidden = name !== step;
    });
  }

  function syncHrvDurationPills() {
    document.querySelectorAll('#hrvDurationPills .window-pill').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.hrvSec) === hrvDurationSec);
    });
  }

  function setStatusPollPaused(paused) {
    deps().setStatusPollPaused?.(paused);
  }

  async function stopHrvOnEsp() {
    if (!getBaseUrl()) return;
    try {
      await fetchWithTimeout(apiUrl('/api/hrv/stop'), { method: 'POST' }, 4000);
    } catch (e) {
      // ignore
    }
  }

  function openHrvModal() {
    if (!hrvModal) return;
    clearHrvTimers();
    hrvDurationSec = HRV_DEFAULT_SEC;
    lastHrvMetrics = null;
    hrvPollFails = 0;
    if (hrvCommentInput) hrvCommentInput.value = '';
    syncHrvDurationPills();
    setHrvStep('setup');
    hrvModal.hidden = false;
    hrvModal.setAttribute('aria-hidden', 'false');
  }

  async function closeHrvModal() {
    if (!hrvModal || hrvClosing) return;
    hrvClosing = true;
    clearHrvTimers();
    setStatusPollPaused(false);
    await stopHrvOnEsp();
    hrvModal.hidden = true;
    hrvModal.setAttribute('aria-hidden', 'true');
    setHrvStep('idle');
    hrvClosing = false;
  }

  async function showHrvError(message) {
    clearHrvTimers();
    setStatusPollPaused(false);
    await stopHrvOnEsp();
    if (hrvErrorText) hrvErrorText.textContent = message || 'Ошибка замера HRV';
    setHrvStep('error');
  }

  function computeHrvMetrics(rrRaw) {
    const raw = (Array.isArray(rrRaw) ? rrRaw : [])
      .map((v) => Number(v))
      .filter((v) => Number.isFinite(v));
    const filtered = [];
    for (let i = 0; i < raw.length; i += 1) {
      const v = raw[i];
      if (v < 250 || v > 2000) continue;
      if (filtered.length > 0) {
        const prev = filtered[filtered.length - 1];
        if (Math.abs(v - prev) > prev * 0.25) continue;
      }
      filtered.push(v);
    }
    const n = filtered.length;
    if (n < HRV_MIN_RR) {
      return { ok: false, rawCount: raw.length, filteredCount: n };
    }
    let sum = 0;
    for (let i = 0; i < n; i += 1) sum += filtered[i];
    const mean = sum / n;
    let varSum = 0;
    for (let i = 0; i < n; i += 1) {
      const d = filtered[i] - mean;
      varSum += d * d;
    }
    const sdnn = Math.sqrt(varSum / (n - 1));
    let diffSq = 0;
    let nn50 = 0;
    const diffs = n - 1;
    for (let i = 1; i < n; i += 1) {
      const d = filtered[i] - filtered[i - 1];
      diffSq += d * d;
      if (Math.abs(d) > 50) nn50 += 1;
    }
    const rmssd = diffs > 0 ? Math.sqrt(diffSq / diffs) : 0;
    const pnn50 = diffs > 0 ? nn50 / diffs : 0;
    const sd1 = rmssd / Math.SQRT2;
    const sd2 = Math.sqrt(Math.max(0, (2 * sdnn * sdnn) - (sd1 * sd1)));
    const pairs = [];
    for (let i = 0; i < n - 1; i += 1) {
      pairs.push({ x: filtered[i], y: filtered[i + 1] });
    }
    return {
      ok: true,
      rawCount: raw.length,
      filteredCount: n,
      meanRr: mean,
      hr: 60000 / mean,
      sdnn,
      rmssd,
      pnn50,
      sd1,
      sd2,
      pairs,
    };
  }

  function drawPoincarePlot(ctx, w, h, metrics, dotRadius = 2.2) {
    ctx.clearRect(0, 0, w, h);
    const pad = { top: 18, right: 16, bottom: 28, left: 42 };
    const innerW = Math.max(1, w - pad.left - pad.right);
    const innerH = Math.max(1, h - pad.top - pad.bottom);
    const side = Math.min(innerW, innerH);
    const x0 = pad.left + (innerW - side) / 2;
    const y0 = pad.top + (innerH - side) / 2;
    const plotW = side;
    const plotH = side;
    const mean = metrics.meanRr;
    let lo = mean * 0.85;
    let hi = mean * 1.15;
    const span = Math.max(metrics.sd2 * 2.4, 80);
    lo = Math.min(lo, mean - span);
    hi = Math.max(hi, mean + span);
    for (const p of metrics.pairs) {
      lo = Math.min(lo, p.x, p.y);
      hi = Math.max(hi, p.x, p.y);
    }
    const range = Math.max(40, hi - lo);
    lo -= range * 0.04;
    hi += range * 0.04;
    const toX = (v) => x0 + ((v - lo) / (hi - lo)) * plotW;
    const toY = (v) => y0 + plotH - ((v - lo) / (hi - lo)) * plotH;
    const pxPerMs = plotW / (hi - lo);

    ctx.fillStyle = '#101010';
    ctx.fillRect(x0, y0, plotW, plotH);

    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.font = '10px system-ui, sans-serif';
    ctx.fillStyle = '#777';
    ctx.textBaseline = 'middle';
    const ticks = 4;
    for (let i = 0; i <= ticks; i += 1) {
      const v = lo + ((hi - lo) * i) / ticks;
      const x = toX(v);
      const y = toY(v);
      ctx.beginPath();
      ctx.moveTo(x0, y);
      ctx.lineTo(x0 + plotW, y);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(x, y0);
      ctx.lineTo(x, y0 + plotH);
      ctx.stroke();
      ctx.textAlign = 'right';
      ctx.fillText(String(Math.round(v)), x0 - 6, y);
      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.fillText(String(Math.round(v)), x, y0 + plotH + 6);
      ctx.textBaseline = 'middle';
    }

    ctx.strokeStyle = '#333';
    ctx.strokeRect(x0 + 0.5, y0 + 0.5, plotW - 1, plotH - 1);

    ctx.save();
    ctx.beginPath();
    ctx.rect(x0, y0, plotW, plotH);
    ctx.clip();

    ctx.strokeStyle = 'rgba(148,163,184,0.45)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(toX(lo), toY(lo));
    ctx.lineTo(toX(hi), toY(hi));
    ctx.stroke();

    const cx = toX(mean);
    const cy = toY(mean);
    ctx.strokeStyle = 'rgba(148,163,184,0.7)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.ellipse(cx, cy, metrics.sd2 * pxPerMs, metrics.sd1 * pxPerMs, -Math.PI / 4, 0, Math.PI * 2);
    ctx.stroke();

    const s2 = (metrics.sd2 * pxPerMs) / Math.SQRT2;
    ctx.beginPath();
    ctx.moveTo(cx - s2, cy + s2);
    ctx.lineTo(cx + s2, cy - s2);
    ctx.stroke();
    const s1 = (metrics.sd1 * pxPerMs) / Math.SQRT2;
    ctx.beginPath();
    ctx.moveTo(cx - s1, cy - s1);
    ctx.lineTo(cx + s1, cy + s1);
    ctx.stroke();

    ctx.fillStyle = '#2dd4bf';
    for (const p of metrics.pairs) {
      ctx.beginPath();
      ctx.arc(toX(p.x), toY(p.y), dotRadius, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();

    ctx.fillStyle = '#94a3b8';
    ctx.font = '11px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    ctx.fillText('SD2', cx + s2 + 6, cy - s2 - 2);
    ctx.fillText('SD1', cx + s1 + 8, cy + s1 + 14);
    ctx.fillStyle = '#888';
    ctx.font = '10px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    ctx.fillText('RRn (ms)', x0 + plotW / 2, h - 2);
    ctx.save();
    ctx.translate(14, y0 + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('RRn+1 (ms)', 0, 0);
    ctx.restore();
  }

  function drawPoincare(canvas, metrics) {
    if (!canvas || !metrics || !metrics.ok) return;
    const rect = canvas.getBoundingClientRect();
    const { ctx, w, h } = prepareCanvas(
      canvas,
      Math.max(1, rect.width),
      Math.max(1, rect.height),
    );
    drawPoincarePlot(ctx, w, h, metrics);
  }

  function getHrvMetricItems(metrics) {
    return [
      ['R-R (ms)', String(Math.round(metrics.meanRr))],
      ['HR (bpm)', String(Math.round(metrics.hr))],
      ['SDNN', String(Math.round(metrics.sdnn))],
      ['rMSSD', String(Math.round(metrics.rmssd))],
      ['pNN50', metrics.pnn50.toFixed(2)],
      ['SD1 / SD2', `${Math.round(metrics.sd1)} / ${Math.round(metrics.sd2)}`],
    ];
  }

  function saveHrvResultImage() {
    if (!lastHrvMetrics?.ok) return;
    const exportW = 420;
    const pad = 16;
    const contentW = exportW - pad * 2;
    const plotH = 280;
    const comment = hrvCommentInput?.value?.trim() || '';
    const measureCanvas = document.createElement('canvas');
    const measureCtx = measureCanvas.getContext('2d');
    measureCtx.font = 'bold 18px system-ui, sans-serif';
    const commentLines = comment ? wrapTextLines(measureCtx, comment, contentW) : [];
    const commentH = commentLines.length ? 16 + commentLines.length * 24 : 0;
    const metricsH = 124;
    const headerH = 52;
    const noteH = 22;
    const exportH = pad + headerH + plotH + 10 + metricsH + noteH + commentH + pad;

    const canvas = document.createElement('canvas');
    const dpr = 2;
    const { ctx, w, h } = prepareCanvas(canvas, exportW, exportH, dpr);

    ctx.fillStyle = '#1c1c1c';
    ctx.fillRect(0, 0, w, h);

    let y = pad;
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 18px system-ui, sans-serif';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText('Замер HRV', pad, y);
    y += 26;

    ctx.fillStyle = '#888888';
    ctx.font = '13px system-ui, sans-serif';
    const timeLine = lastHrvResultStamp
      ? `Время: ${lastHrvResultStamp}`
      : `Время: ${formatHrvStamp(new Date())}`;
    ctx.fillText(`${timeLine} · ${formatHrvDurationLabel(lastHrvRecordedSec)}`, pad, y);
    y += 22;

    const plotCanvas = document.createElement('canvas');
    const plotPrepared = prepareCanvas(plotCanvas, contentW, plotH, dpr);
    plotPrepared.ctx.fillStyle = '#101010';
    plotPrepared.ctx.fillRect(0, 0, contentW, plotH);
    drawPoincarePlot(plotPrepared.ctx, contentW, plotH, lastHrvMetrics, 1.7);
    ctx.strokeStyle = '#333333';
    ctx.lineWidth = 1;
    ctx.strokeRect(pad + 0.5, y + 0.5, contentW - 1, plotH - 1);
    ctx.drawImage(plotCanvas, pad, y, contentW, plotH);
    y += plotH + 10;

    const items = getHrvMetricItems(lastHrvMetrics);
    const colW = contentW / 3;
    const rowH = 58;
    items.forEach((item, index) => {
      const col = index % 3;
      const row = Math.floor(index / 3);
      const x = pad + col * colW;
      const boxY = y + row * rowH;
      ctx.fillStyle = '#141414';
      ctx.fillRect(x + 2, boxY, colW - 4, rowH - 6);
      ctx.strokeStyle = '#333333';
      ctx.lineWidth = 1;
      ctx.strokeRect(x + 2.5, boxY + 0.5, colW - 5, rowH - 7);
      ctx.fillStyle = '#888888';
      ctx.font = '11px system-ui, sans-serif';
      ctx.fillText(item[0], x + 10, boxY + 8);
      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 18px system-ui, sans-serif';
      ctx.fillText(item[1], x + 10, boxY + 24);
    });
    y += metricsH;

    ctx.fillStyle = '#888888';
    ctx.font = '13px system-ui, sans-serif';
    ctx.fillText(lastHrvResultNote || '', pad, y);
    y += noteH;

    if (commentLines.length) {
      y += 8;
      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 18px system-ui, sans-serif';
      for (const line of commentLines) {
        ctx.fillText(line, pad, y);
        y += 24;
      }
    }

    const stamp = hrvFilenameStamp(new Date());
    canvas.toBlob((blob) => {
      if (!blob) return;
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `hrv_${stamp}.png`;
      link.click();
      URL.revokeObjectURL(url);
    }, 'image/png');
  }

  function renderHrvMetrics(metrics) {
    if (!hrvMetricsGrid) return;
    hrvMetricsGrid.replaceChildren();
    for (const [label, value] of getHrvMetricItems(metrics)) {
      const box = document.createElement('div');
      box.className = 'hrv-metric';
      const l = document.createElement('span');
      l.className = 'hrv-metric-label';
      l.textContent = label;
      const v = document.createElement('span');
      v.className = 'hrv-metric-value';
      v.textContent = value;
      box.append(l, v);
      hrvMetricsGrid.append(box);
    }
  }

  function showHrvResult(rr) {
    const metrics = computeHrvMetrics(rr);
    if (!metrics.ok) {
      showHrvError(
        `Мало данных для HRV: ${metrics.filteredCount} интервалов после фильтрации (нужно ≥ ${HRV_MIN_RR}). Исходных R-R: ${metrics.rawCount}.`,
      );
      return;
    }
    if (hrvResultTime) hrvResultTime.textContent = `Время: ${formatHrvStamp(new Date())}`;
    lastHrvResultStamp = formatHrvStamp(new Date());
    if (hrvResultNote) {
      lastHrvResultNote =
        `Интервалов: ${metrics.rawCount} (после фильтрации ${metrics.filteredCount})`;
      hrvResultNote.textContent = lastHrvResultNote;
    }
    if (hrvCommentInput) hrvCommentInput.value = '';
    renderHrvMetrics(metrics);
    lastHrvMetrics = metrics;
    setStatusPollPaused(false);
    setHrvStep('result');
    requestAnimationFrame(() => drawPoincare(hrvPoincare, metrics));
  }

  function updateHrvRecordingUi(data) {
    const durationMs = (Number(data.duration_sec) || hrvDurationSec) * 1000;
    const elapsed = Number(data.elapsed_ms) || 0;
    const left = Math.max(0, durationMs - elapsed);
    const rrCount = Number(data.rr_count) || 0;
    const frac = durationMs > 0 ? Math.min(1, elapsed / durationMs) : 0;
    if (hrvProgressBar) hrvProgressBar.style.width = `${(frac * 100).toFixed(1)}%`;
    if (hrvRecordingStatus) hrvRecordingStatus.textContent = `Идёт замер… осталось ${formatHrvRemain(left)}`;
    if (hrvRecordingMeta) hrvRecordingMeta.textContent = `R-R: ${rrCount}`;
  }

  async function fetchHrvStatus() {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), HRV_FETCH_TIMEOUT_MS);
    try {
      const res = await fetch(apiUrl('/api/hrv'), { signal: controller.signal });
      if (!res.ok) {
        let code = `HTTP ${res.status}`;
        try {
          const body = await res.json();
          if (body && body.error) code = body.error;
        } catch (e) {
          // keep HTTP status
        }
        throw new Error(mapHrvApiError(code));
      }
      return await res.json();
    } catch (e) {
      if (e.name === 'AbortError') {
        throw new Error('таймаут загрузки результата HRV');
      }
      throw e;
    } finally {
      clearTimeout(timer);
    }
  }

  async function pollHrvSession() {
    if (hrvUiState !== 'recording' || !getBaseUrl() || hrvPollInFlight) return;
    hrvPollInFlight = true;
    try {
      const data = await fetchHrvStatus();
      hrvPollFails = 0;
      const state = String(data.state || '');
      if (state === 'recording') {
        updateHrvRecordingUi(data);
        const durationMs = (Number(data.duration_sec) || hrvDurationSec) * 1000;
        const elapsed = Number(data.elapsed_ms) || 0;
        if (durationMs > 0 && elapsed >= durationMs - 500) {
          if (hrvRecordingStatus) hrvRecordingStatus.textContent = 'Завершение… загрузка R-R';
        }
        return;
      }
      clearHrvTimers();
      if (state === 'done') {
        showHrvResult(data.rr || []);
        return;
      }
      if (state === 'error') {
        await showHrvError(data.error || 'Ошибка замера HRV');
        return;
      }
      await showHrvError('Замер прерван');
    } catch (e) {
      hrvPollFails += 1;
      if (hrvPollFails < HRV_POLL_FAIL_MAX) {
        if (hrvRecordingStatus && hrvPollFails === 1) {
          hrvRecordingStatus.textContent = 'Загрузка результата…';
        }
        return;
      }
      await showHrvError(e.message || 'Нет связи с ESP32');
    } finally {
      hrvPollInFlight = false;
    }
  }

  async function startHrvRecording() {
    if (!getBaseUrl()) {
      await showHrvError('Сначала подключитесь к ESP32');
      return;
    }
    setStatusPollPaused(true);
    hrvPollFails = 0;
    lastHrvRecordedSec = hrvDurationSec;
    try {
      const res = await fetchWithTimeout(apiUrl('/api/hrv/start'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ duration_sec: hrvDurationSec }),
      }, 8000);
      if (!res.ok) {
        let code = `HTTP ${res.status}`;
        try {
          const body = await res.json();
          if (body && body.error) code = body.error;
        } catch (e) {
          // keep HTTP status
        }
        await showHrvError(mapHrvApiError(code));
        return;
      }
    } catch (e) {
      await showHrvError(e.message || 'Не удалось начать замер');
      return;
    }
    if (hrvProgressBar) hrvProgressBar.style.width = '0%';
    updateHrvRecordingUi({ duration_sec: hrvDurationSec, elapsed_ms: 0, rr_count: 0 });
    setHrvStep('recording');
    pollHrvSession();
    hrvPollTimer = setInterval(pollHrvSession, 1000);
  }

  function startHrvCountdown() {
    clearHrvTimers();
    setStatusPollPaused(true);
    hrvCountdownLeft = HRV_COUNTDOWN_SEC;
    if (hrvCountdownNum) hrvCountdownNum.textContent = String(hrvCountdownLeft);
    setHrvStep('countdown');
    hrvCountdownTimer = setInterval(() => {
      hrvCountdownLeft -= 1;
      if (hrvCountdownLeft <= 0) {
        clearHrvTimers();
        startHrvRecording();
        return;
      }
      if (hrvCountdownNum) hrvCountdownNum.textContent = String(hrvCountdownLeft);
    }, 1000);
  }

  async function cancelHrvRecording() {
    clearHrvTimers();
    setStatusPollPaused(false);
    await stopHrvOnEsp();
    setHrvStep('setup');
  }

  async function resetHrvToSetup() {
    clearHrvTimers();
    setStatusPollPaused(false);
    await stopHrvOnEsp();
    syncHrvDurationPills();
    setHrvStep('setup');
  }

  function bindEvents() {
    if (inited) return;
    inited = true;
    btnHrvClose?.addEventListener('click', () => { closeHrvModal(); });
    hrvModalBackdrop?.addEventListener('click', () => { closeHrvModal(); });
    btnHrvStart?.addEventListener('click', () => { startHrvCountdown(); });
    btnHrvCancel?.addEventListener('click', () => { cancelHrvRecording(); });
    btnHrvAgain?.addEventListener('click', resetHrvToSetup);
    btnHrvSaveImage?.addEventListener('click', () => { saveHrvResultImage(); });
    btnHrvErrorRetry?.addEventListener('click', resetHrvToSetup);
    document.querySelectorAll('#hrvDurationPills .window-pill').forEach((btn) => {
      btn.addEventListener('click', () => {
        hrvDurationSec = Number(btn.dataset.hrvSec) || HRV_DEFAULT_SEC;
        syncHrvDurationPills();
      });
    });
  }

  window.OxyHrv = {
    init: bindEvents,
    open: openHrvModal,
    onResize() {
      if (hrvUiState === 'result' && lastHrvMetrics) {
        drawPoincare(hrvPoincare, lastHrvMetrics);
      }
    },
  };
})();
