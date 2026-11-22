import { useState } from "react";
import { Wand2 } from "lucide-react";
import {
  assignWorker,
  autoAssignComplaint,
  updateComplaintPriority,
  updateComplaintStatus
} from "../services/api";
import { FormDialog, PriorityBadge, StatusBadge } from "./ui";

const PRIORITIES = ["Low", "Medium", "High", "Critical"];

// role: "admin" | "student". Admin rows get status, priority and assignment
// controls; the options come from the server (allowedStatuses / canAssign).
export default function ComplaintTable({ rows, role, workers = [], onTrack, onChanged, onMessage }) {
  const [rejecting, setRejecting] = useState(null);
  const [busyId, setBusyId] = useState(null);

  if (!rows.length) return <div className="no-data">No complaints to show.</div>;

  async function run(id, action, success) {
    setBusyId(id);
    try {
      const result = await action();
      onMessage?.(success || result.message, "success");
      await onChanged?.();
    } catch (err) {
      onMessage?.(err.message, "error");
    } finally {
      setBusyId(null);
    }
  }

  function changeStatus(c, status) {
    if (!status) return;
    if (status === "Rejected") {
      setRejecting(c);
      return;
    }
    run(c.id, () => updateComplaintStatus(c.id, status), `Complaint #${c.id} marked ${status}`);
  }

  async function assign(c, workerId) {
    if (!workerId) return;
    setBusyId(c.id);
    try {
      let result;
      try {
        result = await assignWorker(c.id, workerId);
      } catch (err) {
        if (err.code === "DEPARTMENT_MISMATCH" || err.code === "AT_CAPACITY" || err.code === "OFF_DUTY") {
          if (!window.confirm(err.message)) return;
          result = await assignWorker(c.id, workerId, true);
        } else throw err;
      }
      onMessage?.(`Complaint #${c.id}: ${result.message}`, "success");
      await onChanged?.();
    } catch (err) {
      onMessage?.(err.message, "error");
    } finally {
      setBusyId(null);
    }
  }

  return (
    <>
      <div className="table-wrap cards-on-mobile">
        <table>
          <thead>
            <tr>
              <th>Complaint</th>
              {role === "admin" && <th>Student</th>}
              <th>Location</th>
              <th>Priority</th>
              <th>Status</th>
              <th>Worker</th>
              <th aria-label="Actions" />
            </tr>
          </thead>
          <tbody>
            {rows.map((c) => {
              const matching = workers.filter(
                (w) => w.isActive && w.department === c.department && w.workerId !== c.workerId
              );
              const others = workers.filter(
                (w) => w.isActive && w.department !== c.department && w.workerId !== c.workerId
              );
              const busy = busyId === c.id;
              return (
                <tr key={c.id} className={busy ? "row-busy" : ""}>
                  <td data-label="Complaint">
                    <b>#{c.id} {c.title}</b>
                    <small>{c.category === c.department ? c.category : `${c.category} · ${c.department} team`}</small>
                  </td>
                  {role === "admin" && (
                    <td data-label="Student">{c.studentName}<small>ID {c.studentId}</small></td>
                  )}
                  <td data-label="Location">{c.hostelName}<small>Room {c.roomNo}</small></td>
                  <td data-label="Priority">
                    {role === "admin" && !["Resolved", "Rejected"].includes(c.status) ? (
                      <select
                        className="inline-select"
                        aria-label={`Priority of complaint ${c.id}`}
                        value={c.priority}
                        disabled={busy}
                        onChange={(e) =>
                          run(c.id, () => updateComplaintPriority(c.id, e.target.value), `Priority of #${c.id} set to ${e.target.value}`)
                        }
                      >
                        {PRIORITIES.map((p) => <option key={p}>{p}</option>)}
                      </select>
                    ) : (
                      <PriorityBadge priority={c.priority} effective={c.effectivePriority} />
                    )}
                  </td>
                  <td data-label="Status">
                    <StatusBadge status={c.status} />
                    {c.status === "Escalated" && c.feedback && <small className="note">{c.feedback}</small>}
                  </td>
                  <td data-label="Worker">
                    {c.workerName ? <>{c.workerName}<small>{c.workerId}</small></> : <span className="muted">Unassigned</span>}
                  </td>
                  <td className="actions-cell"><div className="actions">
                    {role === "admin" && (
                      <>
                        {c.allowedStatuses?.length > 0 && (
                          <select
                            className="inline-select"
                            aria-label={`Change status of complaint ${c.id}`}
                            value=""
                            disabled={busy}
                            onChange={(e) => changeStatus(c, e.target.value)}
                          >
                            <option value="">Change status…</option>
                            {c.allowedStatuses.map((s) => (
                              <option key={s} value={s}>
                                {s === "Forwarded" && c.workerId ? "Forwarded (unassign)" : s === "Pending" && !["Verified"].includes(c.status) ? "Reopen" : s}
                              </option>
                            ))}
                          </select>
                        )}
                        {c.canAssign && (
                          <select
                            className="inline-select"
                            aria-label={`Assign worker to complaint ${c.id}`}
                            value=""
                            disabled={busy}
                            onChange={(e) => assign(c, e.target.value)}
                          >
                            <option value="">{c.workerId ? "Reassign…" : "Assign worker…"}</option>
                            {matching.length > 0 && (
                              <optgroup label={c.department}>
                                {matching.map((w) => (
                                  <option key={w.workerId} value={w.workerId}>
                                    {w.schedule?.assignable === false ? "○ " : "● "}{w.name} ({w.activeTaskCount} active{w.schedule?.assignable === false ? `, ${w.availability.toLowerCase()}` : ""})
                                  </option>
                                ))}
                              </optgroup>
                            )}
                            {others.length > 0 && (
                              <optgroup label="Other departments">
                                {others.map((w) => (
                                  <option key={w.workerId} value={w.workerId}>
                                    {w.name} · {w.department}
                                  </option>
                                ))}
                              </optgroup>
                            )}
                          </select>
                        )}
                        {c.canAutoAssign && (
                          <button
                            className="ghost-btn"
                            disabled={busy}
                            title="Assign the nearest available worker of the right department"
                            onClick={() => run(c.id, () => autoAssignComplaint(c.id))}
                          >
                            <Wand2 size={14} /> Auto-assign
                          </button>
                        )}
                      </>
                    )}
                    <button className="ghost-btn" onClick={() => onTrack(c)}>
                      {role === "student" && c.canRate ? "Rate" : "Track"}
                    </button>
                  </div></td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {rejecting && (
        <FormDialog
          title={`Reject complaint #${rejecting.id}`}
          subtitle="The student will see this reason."
          fields={[{ name: "note", label: "Reason", type: "textarea", required: true }]}
          submitLabel="Reject complaint"
          onClose={() => setRejecting(null)}
          onSubmit={async ({ note }) => {
            await updateComplaintStatus(rejecting.id, "Rejected", note.trim());
            onMessage?.(`Complaint #${rejecting.id} rejected`, "success");
            await onChanged?.();
          }}
        />
      )}
    </>
  );
}
