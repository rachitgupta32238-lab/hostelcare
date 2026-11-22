import { useCallback, useEffect, useState } from "react";
import { getAnalytics, getFeedback } from "../services/api";
import { Stars } from "../components/ui";

function Bars({ title, data }) {
  const max = Math.max(1, ...data.map((d) => d.count));
  return (
    <section className="panel">
      <h3 className="chart-title">{title}</h3>
      {data.length === 0 ? <p className="muted">No data yet.</p> : (
        <ul className="hbars">
          {data.map((d) => (
            <li key={d.label}>
              <span className="hbar-label">{d.label}</span>
              <span className="hbar-track"><span className="hbar-fill" style={{ width: `${(d.count / max) * 100}%` }} /></span>
              <span className="hbar-value">{d.count}</span>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}

export default function AnalyticsPage({ refreshKey, onMessage }) {
  const [data, setData] = useState(null);
  const [feedback, setFeedback] = useState([]);

  const load = useCallback(async () => {
    try {
      const [a, f] = await Promise.all([getAnalytics(), getFeedback()]);
      setData(a);
      setFeedback(f.feedback || []);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  if (!data) return <p className="muted">Loading analytics…</p>;

  const trendMax = Math.max(1, ...data.dailySubmissions.map((d) => d.count));
  const kpis = [
    { label: "Complaints recorded", value: data.total },
    { label: "Average time to resolve", value: data.resolvedWithTiming ? `${data.averageResolutionHours} h` : "–", note: data.resolvedWithTiming ? `median ${data.medianResolutionHours} h, ${data.resolvedWithTiming} timed` : "needs timestamped complaints" },
    { label: "Open longer than 3 days", value: data.overdueOpen, warn: data.overdueOpen > 0 },
    { label: "Average student rating", value: data.ratingsCount ? `${data.averageRating} / 5` : "–", note: `${data.ratingsCount} rating${data.ratingsCount === 1 ? "" : "s"}` }
  ];

  return (
    <>
      <div className="stats-grid">
        {kpis.map((k) => (
          <div className={`stat-card panel ${k.warn ? "warn" : ""}`} key={k.label}>
            <div className="stat-label">{k.label}</div>
            <div className="stat-value">{k.value}</div>
            {k.note && <div className="stat-foot">{k.note}</div>}
          </div>
        ))}
      </div>

      <section className="panel">
        <h3 className="chart-title">New complaints, last 14 days</h3>
        <div className="vbars" role="img" aria-label="Daily complaint submissions for the last 14 days">
          {data.dailySubmissions.map((d) => (
            <div key={d.date} className="vbar" title={`${d.date}: ${d.count}`}>
              <span className="vbar-value">{d.count || ""}</span>
              <span className="vbar-fill" style={{ height: `${(d.count / trendMax) * 100}%` }} />
              <span className="vbar-label">{d.date.slice(8)}</span>
            </div>
          ))}
        </div>
      </section>

      <div className="chart-grid">
        <Bars title="By status" data={data.byStatus} />
        <Bars title="By department" data={data.byDepartment} />
        <Bars title="By priority" data={data.byPriority} />
        <Bars title="By location" data={data.byLocation} />
      </div>

      <section className="panel">
        <h3 className="chart-title">Worker performance</h3>
        <div className="table-wrap">
          <table>
            <thead><tr><th>Worker</th><th>Department</th><th>Active</th><th>Resolved</th><th>Escalated</th><th>Avg. resolve time</th><th>Rating</th></tr></thead>
            <tbody>
              {data.workers.map((w) => (
                <tr key={w.workerId} className={w.isActive ? "" : "row-muted"}>
                  <td><b>{w.name}</b><small>{w.workerId}</small></td>
                  <td>{w.department}</td>
                  <td>{w.active}</td>
                  <td>{w.resolved}</td>
                  <td>{w.escalated}</td>
                  <td>{w.averageResolutionHours ? `${w.averageResolutionHours} h` : "–"}</td>
                  <td>{w.ratingsCount ? `${w.averageRating} (${w.ratingsCount})` : "–"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section className="panel">
        <h3 className="chart-title">Student feedback</h3>
        {feedback.length === 0 ? <p className="muted">Students can rate a complaint once it is resolved.</p> : (
          <ul className="feedback-list">
            {feedback.map((f) => (
              <li key={f.complaintId}>
                <Stars value={f.rating} size={14} />
                <div>
                  <b>#{f.complaintId} {f.title}</b>
                  <small>{f.workerName || "No worker"} · {f.timestamp}</small>
                  {f.comment && <p>“{f.comment}”</p>}
                </div>
              </li>
            ))}
          </ul>
        )}
      </section>
    </>
  );
}
