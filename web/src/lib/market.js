// Market hours detection - ported from src/market.h
// NYSE regular session: 09:30-16:00 America/New_York

// Returns UTC offset for America/New_York based on month and day
// DST starts second Sunday of March (Mar 8-14), ends first Sunday of November (Nov 1-7)
function utcOffset(month, day) {
  const EST = -5; // UTC-5
  const EDT = -4; // UTC-4

  // March: DST starts second Sunday (day 8-14), assume EDT from day 9 onward
  if (month === 3) return day >= 9 ? EDT : EST;

  // November: DST ends first Sunday (day 1-7), assume EST from day 2 onward
  if (month === 11) return day >= 2 ? EST : EDT;

  // April-October: EDT
  if (month >= 4 && month <= 10) return EDT;

  // December-February: EST
  return EST;
}

// Parse UTC ISO 8601 timestamp and return minutes since midnight in America/New_York
function nyMinutes(timestamp) {
  if (!timestamp || timestamp.length < 19) return -1;

  const month = parseInt(timestamp.substring(5, 7), 10);
  const day = parseInt(timestamp.substring(8, 10), 10);
  const hour = parseInt(timestamp.substring(11, 13), 10);
  const minute = parseInt(timestamp.substring(14, 16), 10);

  if (isNaN(month) || isNaN(day) || isNaN(hour) || isNaN(minute)) return -1;

  const utcMin = hour * 60 + minute;
  const offset = utcOffset(month, day);

  // Apply offset, wrapping into [0, 24h)
  let localMin = utcMin + (offset * 60);
  const dayMin = 24 * 60;

  if (localMin < 0) localMin += dayMin;
  if (localMin >= dayMin) localMin -= dayMin;

  return localMin;
}

// True while NYSE regular session is open (09:30-16:00 ET, DST-aware)
export function marketOpen(timestamp) {
  const SESSION_OPEN_ET = 9 * 60 + 30;  // 09:30 ET
  const SESSION_CLOSE_ET = 16 * 60;     // 16:00 ET

  const t = nyMinutes(timestamp);
  return t >= SESSION_OPEN_ET && t < SESSION_CLOSE_ET;
}

// True during risk-off period (opening volatility + pre-close liquidation)
export function riskOff(timestamp) {
  const SESSION_OPEN_ET = 9 * 60 + 30;
  const SESSION_CLOSE_ET = 16 * 60;
  const RISK_ON_DELAY = 15;   // Skip first 15 minutes
  const RISK_OFF_START = 45;  // Start liquidation 45 min before close

  if (!marketOpen(timestamp)) return false;

  const t = nyMinutes(timestamp);
  const riskStart = SESSION_OPEN_ET + RISK_ON_DELAY;
  const riskEnd = SESSION_CLOSE_ET - RISK_OFF_START;

  return t < riskStart || t >= riskEnd;
}
