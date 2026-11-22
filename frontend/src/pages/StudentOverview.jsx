import { useCallback, useEffect, useState } from "react";
import { ClipboardList, Plus } from "lucide-react";
import { getComplaints } from "../services/api";
import ComplaintTable from "../components/ComplaintTable";
import { EmptyState } from "../components/ui";

export default function StudentOverview({ refreshKey, onTrack, onMessage, onNewComplaint }) {
  const [complaints, setComplaints] = useState(null);

  const load = useCallback(async () => {
    try {
      setComplaints((await getComplaints()).complaints || []);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  const list = complaints || [];
  const open = list.filter((c) => !["Resolved", "Rejected"].includes(c.status));
  const toRate = list.filter((c) => c.canRate);
  const stats = [
    { label: "Open complaints", value: open.length },
    { label: "With a worker", value: list.filter((c) => ["Assigned", "Accepted", "In Progress"].includes(c.status)).length },
    { label: "Resolved", value: list.filter((c) => c.status === "Resolved").length },
    { label: "Waiting for your rating", value: toRate.length }
  ];

  return (
    <>
      <div className="stats-grid">
        {stats.map((s) => (
          <div className="stat-card panel" key={s.label}>
            <div className="stat-label">{s.label}</div>
            <div className="stat-value">{complaints === null ? "–" : s.value}</div>
          </div>
        ))}
      </div>

      {toRate.length > 0 && (
        <button className="banner" onClick={() => onTrack(toRate[0])}>
          <span><b>#{toRate[0].id} {toRate[0].title}</b> was resolved. Tell the warden how it went.</span>
          <span className="banner-action">Rate now</span>
        </button>
      )}

      <section className="panel">
        <div className="panel-heading">
          <div><h3>My complaints</h3><p>Only you and the warden can see these.</p></div>
        </div>
        {complaints !== null && list.length === 0 ? (
          <EmptyState
            icon={ClipboardList}
            title="No complaints yet"
            text="Report a broken fan, a leaking tap or anything else that needs fixing."
            action={<button className="primary-btn" onClick={onNewComplaint}><Plus size={16} /> Report a problem</button>}
          />
        ) : (
          <ComplaintTable rows={list} role="student" onTrack={onTrack} onMessage={onMessage} />
        )}
      </section>
    </>
  );
}
