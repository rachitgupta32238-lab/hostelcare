import { useEffect, useState } from "react";
import { getMyAvailability, saveMyAvailability } from "../services/api";

const DAY_NAMES = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"];

export default function AvailabilityPage({ onMessage }) {
  const [form, setForm] = useState(null);
  const [status, setStatus] = useState(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    getMyAvailability()
      .then(({ availability: a }) => {
        setStatus(a);
        setForm({
          days: a.configured
            ? a.days.map((d) => ({ working: d.working, start: d.start, end: d.end }))
            : DAY_NAMES.map((_, i) => ({ working: i < 6, start: "09:00", end: "17:00" })),
          leaveFrom: a.leaveFrom,
          leaveTo: a.leaveTo,
          note: a.note
        });
      })
      .catch((err) => onMessage(err.message, "error"));
  }, [onMessage]);

  if (!form) return <p className="muted">Loading your hours…</p>;

  const setDay = (i, patch) =>
    setForm({ ...form, days: form.days.map((d, j) => (j === i ? { ...d, ...patch } : d)) });

  function copyMondayToWeekdays() {
    setForm({ ...form, days: form.days.map((d, i) => (i > 0 && i < 5 ? { ...form.days[0] } : d)) });
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    setBusy(true);
    try {
      const result = await saveMyAvailability(form);
      setStatus(result.availability);
      onMessage(result.message, "success");
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <form className="settings-grid" onSubmit={save}>
      <section className="panel">
        <div className="panel-heading">
          <div>
            <h3>Weekly working hours</h3>
            <p>The warden sees these when assigning work. If a shift ends after midnight, set the end time earlier than the start (for example 22:00 to 06:00).</p>
          </div>
        </div>
        {status && (
          <p className={`duty-pill ${status.state}`}>Right now you are shown as: <b>{status.label}</b></p>
        )}
        <table className="schedule-table">
          <tbody>
            {form.days.map((d, i) => (
              <tr key={DAY_NAMES[i]}>
                <td>
                  <label className="check">
                    <input type="checkbox" checked={d.working} onChange={(e) => setDay(i, { working: e.target.checked })} />
                    {DAY_NAMES[i]}
                  </label>
                </td>
                <td>
                  {d.working ? (
                    <span className="time-range">
                      <input type="time" aria-label={`${DAY_NAMES[i]} start`} value={d.start} required onChange={(e) => setDay(i, { start: e.target.value })} />
                      to
                      <input type="time" aria-label={`${DAY_NAMES[i]} end`} value={d.end} required onChange={(e) => setDay(i, { end: e.target.value })} />
                    </span>
                  ) : (
                    <span className="muted">Not working</span>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        <button type="button" className="text-btn" onClick={copyMondayToWeekdays}>Copy Monday's hours to Tue–Fri</button>
      </section>

      <section className="panel">
        <h3 className="chart-title">Leave</h3>
        <div className="form">
          <div className="form-row">
            <label><span>From</span><input type="date" value={form.leaveFrom} onChange={(e) => setForm({ ...form, leaveFrom: e.target.value })} /></label>
            <label><span>To</span><input type="date" value={form.leaveTo} min={form.leaveFrom} onChange={(e) => setForm({ ...form, leaveTo: e.target.value })} /></label>
          </div>
          {(form.leaveFrom || form.leaveTo) && (
            <button type="button" className="text-btn" onClick={() => setForm({ ...form, leaveFrom: "", leaveTo: "" })}>Remove leave</button>
          )}
          <label><span>Note for the warden (optional)</span>
            <textarea rows={3} maxLength={200} value={form.note} onChange={(e) => setForm({ ...form, note: e.target.value })} placeholder="e.g. Available on call for emergencies" />
          </label>
          <small className="hint">You are never assigned work while on leave. Outside your hours you only get Critical complaints when nobody else is free.</small>
          {error && <p className="form-error" role="alert">{error}</p>}
          <button className="primary-btn" type="submit" disabled={busy}>{busy ? "Saving…" : "Save availability"}</button>
        </div>
      </section>
    </form>
  );
}
