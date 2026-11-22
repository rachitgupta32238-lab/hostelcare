import { useState } from "react";
import { changePassword } from "../services/api";

export default function SettingsPage({ session, onTokenChanged, onMessage }) {
  const { user, role } = session;
  const [form, setForm] = useState({ current: "", next: "", confirm: "" });
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const minLength = role === "admin" ? 12 : 8;

  async function submit(e) {
    e.preventDefault();
    setError("");
    if (form.next !== form.confirm) {
      setError("New passwords do not match");
      return;
    }
    setBusy(true);
    try {
      const result = await changePassword(form.current, form.next);
      onTokenChanged(result.token);
      setForm({ current: "", next: "", confirm: "" });
      onMessage(result.message, "success");
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  const details =
    role === "student"
      ? [["Student ID", user.studentId], ["Name", user.name], ["Hostel", user.hostelName], ["Room", user.roomNo], ["Phone", user.phone]]
      : role === "worker"
      ? [["Worker ID", user.workerId], ["Name", user.name], ["Department", user.department], ["Base office", user.office]]
      : [["Admin ID", user.adminId], ["Username", user.username]];

  return (
    <div className="settings-grid">
      <section className="panel">
        <h3 className="chart-title">Your profile</h3>
        <dl className="profile-list">
          {details.map(([k, v]) => (
            <div key={k}><dt>{k}</dt><dd>{v || "–"}</dd></div>
          ))}
        </dl>
        {role !== "admin" && <p className="hint">To correct these details, contact the hostel warden.</p>}
      </section>

      <section className="panel">
        <h3 className="chart-title">Change password</h3>
        <form className="form" onSubmit={submit}>
          <label><span>Current password</span>
            <input type="password" autoComplete="current-password" required value={form.current} onChange={(e) => setForm({ ...form, current: e.target.value })} />
          </label>
          <label><span>New password</span>
            <input type="password" autoComplete="new-password" required minLength={minLength} value={form.next} onChange={(e) => setForm({ ...form, next: e.target.value })} />
          </label>
          <label><span>Confirm new password</span>
            <input type="password" autoComplete="new-password" required value={form.confirm} onChange={(e) => setForm({ ...form, confirm: e.target.value })} />
          </label>
          <small className="hint">At least {minLength} characters with a letter and a digit. Other devices will be logged out.</small>
          {error && <p className="form-error" role="alert">{error}</p>}
          <button className="primary-btn" type="submit" disabled={busy}>{busy ? "Saving…" : "Change password"}</button>
        </form>
      </section>
    </div>
  );
}
