import { useCallback, useEffect, useMemo, useState } from "react";
import { Search } from "lucide-react";
import { fetchWorkers, getComplaints } from "../services/api";
import ComplaintTable from "../components/ComplaintTable";

const FILTERS = ["All", "Open", "Pending", "Verified", "Forwarded", "Escalated", "Assigned", "Accepted", "In Progress", "Resolved", "Rejected"];

export default function ComplaintsPage({ refreshKey, onTrack, onMessage }) {
  const [complaints, setComplaints] = useState([]);
  const [workers, setWorkers] = useState([]);
  const [search, setSearch] = useState("");
  const [filter, setFilter] = useState("Open");

  const load = useCallback(async () => {
    try {
      const [list, team] = await Promise.all([getComplaints(), fetchWorkers()]);
      setComplaints(list.complaints || []);
      setWorkers(team);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  const rows = useMemo(() => {
    const term = search.trim().toLowerCase();
    return complaints.filter((c) => {
      const statusOk =
        filter === "All" ||
        (filter === "Open" ? !["Resolved", "Rejected"].includes(c.status) : c.status === filter);
      const text = [c.id, c.title, c.studentName, c.studentId, c.category, c.hostelName, c.workerName].join(" ").toLowerCase();
      return statusOk && (!term || text.includes(term));
    });
  }, [complaints, search, filter]);

  return (
    <section className="panel">
      <div className="panel-heading wrap">
        <div>
          <h3>Complaint register</h3>
          <p>{rows.length} of {complaints.length} complaints</p>
        </div>
        <div className="toolbar">
          <select className="inline-select" value={filter} onChange={(e) => setFilter(e.target.value)} aria-label="Filter by status">
            {FILTERS.map((f) => <option key={f}>{f}</option>)}
          </select>
          <div className="search-box">
            <Search size={16} />
            <input value={search} onChange={(e) => setSearch(e.target.value)} placeholder="Search by ID, student, hostel…" aria-label="Search complaints" />
          </div>
        </div>
      </div>
      <ComplaintTable rows={rows} role="admin" workers={workers} onTrack={onTrack} onChanged={load} onMessage={onMessage} />
    </section>
  );
}
