import { useState } from "react";
import { createComplaint } from "../services/api";
import { Modal } from "./ui";

const CATEGORIES = ["Electrical", "Plumbing", "Cleaning", "Furniture", "Internet", "Security", "Other"];
const HOSTELS = ["Atlantis Hostel", "SC Bose Hostel", "Bhagat Singh Hostel", "Sardar Patel Hostel"];

export default function ComplaintForm({ student, onClose, onCreated }) {
  const hostelOptions = HOSTELS.includes(student.hostelName)
    ? HOSTELS
    : [student.hostelName, ...HOSTELS].filter(Boolean);

  const [form, setForm] = useState({
    title: "",
    category: "",
    priority: "Low",
    hostelName: student.hostelName || HOSTELS[0],
    roomNo: student.roomNo || "",
    description: ""
  });
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const set = (key) => (e) => setForm({ ...form, [key]: e.target.value });

  async function submit(e) {
    e.preventDefault();
    setBusy(true);
    setError("");
    try {
      const result = await createComplaint({
        ...form,
        title: form.title.trim(),
        description: form.description.trim(),
        roomNo: form.roomNo.trim()
      });
      const raised = result.priority !== form.priority;
      onCreated(
        `Complaint #${result.id} submitted to the ${result.department} team.` +
          (raised ? ` Priority set to ${result.priority} based on the description.` : "")
      );
      onClose();
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <Modal title="Report a problem" subtitle="The warden reviews every complaint before a worker is sent." onClose={onClose}>
      <form className="form" onSubmit={submit}>
        <label><span>Title</span>
          <input required minLength={3} maxLength={100} placeholder="e.g. Bathroom tap leaking" value={form.title} onChange={set("title")} />
        </label>
        <div className="form-row">
          <label><span>Category</span>
            <select required value={form.category} onChange={set("category")}>
              <option value="">Choose…</option>
              {CATEGORIES.map((c) => <option key={c}>{c}</option>)}
            </select>
          </label>
          <label><span>Urgency</span>
            <select value={form.priority} onChange={set("priority")}>
              <option>Low</option><option>Medium</option><option>High</option><option>Critical</option>
            </select>
          </label>
        </div>
        <div className="form-row">
          <label><span>Hostel</span>
            <select value={form.hostelName} onChange={set("hostelName")}>
              {hostelOptions.map((h) => <option key={h}>{h}</option>)}
            </select>
          </label>
          <label><span>Room / area</span>
            <input required maxLength={20} value={form.roomNo} onChange={set("roomNo")} />
          </label>
        </div>
        <label><span>What is wrong?</span>
          <textarea required minLength={5} maxLength={1000} rows={4} placeholder="Describe the problem and where exactly it is." value={form.description} onChange={set("description")} />
        </label>
        <small className="hint">Safety issues such as sparks, smoke or flooding are marked Critical automatically.</small>
        {error && <p className="form-error" role="alert">{error}</p>}
        <div className="modal-actions">
          <button type="button" className="secondary-btn" onClick={onClose}>Cancel</button>
          <button type="submit" className="primary-btn" disabled={busy}>{busy ? "Submitting…" : "Submit complaint"}</button>
        </div>
      </form>
    </Modal>
  );
}
