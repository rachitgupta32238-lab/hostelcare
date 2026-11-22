import { useCallback, useEffect, useState } from "react";
import { Send, Wand2 } from "lucide-react";
import { autoAssignComplaint, getDispatchQueue, runDispatch } from "../services/api";
import { EmptyState, PriorityBadge, StatusBadge } from "../components/ui";

export default function DispatchPage({ refreshKey, onTrack, onMessage }) {
  const [queue, setQueue] = useState(null);
  const [results, setResults] = useState(null);
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    try {
      setQueue((await getDispatchQueue()).queue || []);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  async function runAll() {
    setBusy(true);
    try {
      const r = await runDispatch();
      setResults(r.results || []);
      onMessage(r.message, "success");
      load();
    } catch (err) {
      onMessage(err.message, "error");
    } finally {
      setBusy(false);
    }
  }

  async function assignOne(c) {
    try {
      const r = await autoAssignComplaint(c.id);
      onMessage(`#${c.id}: ${r.message}`, "success");
      load();
    } catch (err) {
      onMessage(err.message, "error");
    }
  }

  const automatic = (queue || []).filter((c) => !c.needsManualReview).length;

  return (
    <>
      <section className="panel explain">
        <h3>How dispatch works</h3>
        <p>
          Verified and forwarded complaints wait in a priority queue. Higher priority goes first; a complaint that
          has waited 48 hours moves up one level so nothing is forgotten. For each complaint, the nearest available
          worker of the right department who is on duty is chosen using Dijkstra shortest paths on the campus map, then
          the fewest active tasks. Off-duty workers are only called in for Critical complaints; workers on leave never are. Escalated complaints need your decision and are never dispatched automatically.
        </p>
        <button className="primary-btn" onClick={runAll} disabled={busy || automatic === 0}>
          <Send size={16} /> {busy ? "Dispatching…" : `Dispatch ${automatic} waiting complaint${automatic === 1 ? "" : "s"}`}
        </button>
      </section>

      {results && results.length > 0 && (
        <section className="panel">
          <div className="panel-heading"><div><h3>Last dispatch run</h3></div>
            <button className="text-btn" onClick={() => setResults(null)}>Dismiss</button></div>
          <ul className="result-list">
            {results.map((r) => (
              <li key={r.complaintId} className={r.assigned ? "ok" : "skip"}>
                <b>#{r.complaintId} {r.title}</b>
                <span>{r.assigned ? `→ ${r.workerName} (${r.distance} m)` : r.message}</span>
              </li>
            ))}
          </ul>
        </section>
      )}

      <section className="panel">
        <div className="panel-heading">
          <div><h3>Queue order</h3><p>Top of the queue is dispatched first.</p></div>
        </div>
        {queue === null ? (
          <p className="muted">Loading queue…</p>
        ) : queue.length === 0 ? (
          <EmptyState icon={Send} title="Nothing waiting" text="Verify new complaints on the Overview page to add them here." />
        ) : (
          <div className="table-wrap">
            <table>
              <thead>
                <tr><th>#</th><th>Complaint</th><th>Priority</th><th>Status</th><th>Recommended worker</th><th aria-label="Actions" /></tr>
              </thead>
              <tbody>
                {queue.map((c) => (
                  <tr key={c.id}>
                    <td className="queue-pos">{c.queuePosition}</td>
                    <td><b>#{c.id} {c.title}</b><small>{c.department} · {c.locationName}</small></td>
                    <td><PriorityBadge priority={c.priority} effective={c.effectivePriority} /></td>
                    <td><StatusBadge status={c.status} /></td>
                    <td>
                      {c.recommended ? (
                        <>
                          {c.recommended.name} <small>{c.recommended.distance} m · {c.recommended.activeTasks} active · {c.recommended.duty}</small>
                          {!c.recommended.onDuty && <small className="note">Off duty: called in because the complaint is Critical</small>}
                        </>
                      ) : <span className="muted">{c.dispatchMessage}</span>}
                    </td>
                    <td className="actions-cell"><div className="actions">
                      {c.recommended && (
                        <button className="ghost-btn" onClick={() => assignOne(c)}>
                          <Wand2 size={14} /> {c.needsManualReview ? "Assign recommended" : "Assign now"}
                        </button>
                      )}
                      <button className="ghost-btn" onClick={() => onTrack(c)}>Details</button>
                    </div></td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </section>
    </>
  );
}
