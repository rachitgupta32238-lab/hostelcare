import { useCallback, useEffect, useState } from "react";
import { ArrowUpRight, CheckCircle2, ClipboardList, Clock3, Send, Wrench } from "lucide-react";
import { fetchWorkers, getAdminDashboard, getComplaints } from "../services/api";
import ComplaintTable from "../components/ComplaintTable";

const STATS = [
  { key: "total", label: "Total complaints", icon: ClipboardList, tone: "blue" },
  { key: "pending", label: "Waiting for action", icon: Clock3, tone: "amber" },
  { key: "inProgress", label: "With workers", icon: Wrench, tone: "violet" },
  { key: "resolved", label: "Resolved", icon: CheckCircle2, tone: "green" }
];

export default function AdminOverview({ refreshKey, onTrack, onMessage, go }) {
  const [stats, setStats] = useState({});
  const [complaints, setComplaints] = useState([]);
  const [workers, setWorkers] = useState([]);

  const load = useCallback(async () => {
    try {
      const [dash, list, team] = await Promise.all([getAdminDashboard(), getComplaints(), fetchWorkers()]);
      setStats(dash);
      setComplaints(list.complaints || []);
      setWorkers(team);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  const needsReview = complaints.filter((c) => c.status === "Pending" || c.status === "Escalated");

  return (
    <>
      <div className="stats-grid">
        {STATS.map(({ key, label, icon: Icon, tone }) => (
          <div className="stat-card panel" key={key}>
            <div className={`stat-icon ${tone}`}><Icon size={19} /></div>
            <div className="stat-label">{label}</div>
            <div className="stat-value">{stats[key] ?? "–"}</div>
          </div>
        ))}
      </div>

      {stats.dispatchQueue > 0 && (
        <button className="banner" onClick={() => go("Dispatch")}>
          <Send size={18} />
          <span><b>{stats.dispatchQueue} complaint{stats.dispatchQueue > 1 ? "s" : ""}</b> verified and waiting for a worker.</span>
          <span className="banner-action">Open dispatch queue <ArrowUpRight size={15} /></span>
        </button>
      )}

      <section className="panel">
        <div className="panel-heading">
          <div>
            <h3>Needs your review</h3>
            <p>New complaints to verify and escalations from workers.</p>
          </div>
          <button className="text-btn" onClick={() => go("Complaints")}>All complaints <ArrowUpRight size={15} /></button>
        </div>
        <ComplaintTable rows={needsReview.slice(0, 8)} role="admin" workers={workers} onTrack={onTrack} onChanged={load} onMessage={onMessage} />
      </section>
    </>
  );
}
