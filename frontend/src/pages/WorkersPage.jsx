import { useCallback, useEffect, useState } from "react";
import { KeyRound, Pencil, Plus, Power } from "lucide-react";
import {
  addWorker,
  editWorker,
  fetchWorkers,
  setWorkerPassword,
  updateWorkerStatus
} from "../services/api";
import { FormDialog, Stars } from "../components/ui";

const DEPARTMENTS = ["Electrical", "Plumbing", "IT Support", "Security", "General Maintenance"];
function scheduleSummary(schedule) {
  if (!schedule?.configured) return "The worker has not set working hours";
  const days = schedule.days.filter((d) => d.working).map((d) => `${d.day} ${d.start}–${d.end}`);
  const leave = schedule.leaveFrom ? `\nLeave: ${schedule.leaveFrom} to ${schedule.leaveTo}` : "";
  return (days.length ? days.join("\n") : "No working days") + leave;
}

const PASSWORD_HINT = "At least 8 characters, with a letter and a digit. Share it with the worker in person.";

export default function WorkersPage({ refreshKey, onMessage }) {
  const [workers, setWorkers] = useState([]);
  const [dialog, setDialog] = useState(null); // { kind, worker }

  const load = useCallback(async () => {
    try {
      setWorkers(await fetchWorkers());
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  async function toggle(worker) {
    const action = worker.isActive ? "Deactivate" : "Activate";
    const warning = worker.isActive && worker.activeTaskCount > 0
      ? `\n\n${worker.name} still has ${worker.activeTaskCount} active task(s). They stay assigned and must be reassigned.`
      : "";
    if (!window.confirm(`${action} ${worker.name}?${warning}`)) return;
    try {
      const result = await updateWorkerStatus(worker.workerId, !worker.isActive);
      onMessage(result.message, "success");
      load();
    } catch (err) {
      onMessage(err.message, "error");
    }
  }

  const withoutLogin = workers.filter((w) => w.isActive && !w.hasLogin);

  return (
    <section className="panel">
      <div className="panel-heading">
        <div>
          <h3>Maintenance team</h3>
          <p>Availability combines each worker's hours (hover for the week), leave, and live workload. A worker with 5 active tasks is at capacity.</p>
        </div>
        <button className="primary-btn" onClick={() => setDialog({ kind: "add" })}>
          <Plus size={16} /> Add worker
        </button>
      </div>

      {withoutLogin.length > 0 && (
        <div className="alert">
          {withoutLogin.map((w) => w.name).join(", ")} cannot log in yet. Use “Set password” to give them access.
        </div>
      )}

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Worker</th>
              <th>Department</th>
              <th>Availability</th>
              <th>Active</th>
              <th>Resolved</th>
              <th>Rating</th>
              <th aria-label="Actions" />
            </tr>
          </thead>
          <tbody>
            {workers.map((w) => (
              <tr key={w.workerId} className={w.isActive ? "" : "row-muted"}>
                <td>
                  <b>{w.name}</b>
                  <small>{w.workerId}{!w.hasLogin && " · no login"}</small>
                </td>
                <td>{w.department}<small>{w.office}</small></td>
                <td>
                  <span className={`badge availability-${w.availability.toLowerCase().replace(/\s+/g, "-")}`}>{w.availability}</span>
                  <small title={scheduleSummary(w.schedule)}>{w.schedule?.label}</small>
                  {w.schedule?.note && <small className="note-muted">“{w.schedule.note}”</small>}
                </td>
                <td>{w.activeTaskCount}</td>
                <td>{w.resolvedCount}</td>
                <td>
                  {w.ratingsCount > 0 ? (
                    <span className="rating-cell"><Stars value={Math.round(w.averageRating)} size={13} /> {w.averageRating}</span>
                  ) : <span className="muted">No ratings</span>}
                </td>
                <td className="actions-cell"><div className="actions">
                  <button className="ghost-btn" onClick={() => setDialog({ kind: "edit", worker: w })}><Pencil size={14} /> Edit</button>
                  <button className="ghost-btn" onClick={() => setDialog({ kind: "password", worker: w })}><KeyRound size={14} /> {w.hasLogin ? "Reset password" : "Set password"}</button>
                  <button className={`ghost-btn ${w.isActive ? "danger" : ""}`} onClick={() => toggle(w)}><Power size={14} /> {w.isActive ? "Deactivate" : "Activate"}</button>
                </div></td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {dialog?.kind === "add" && (
        <FormDialog
          title="Add worker"
          fields={[
            { name: "workerId", label: "Worker ID", required: true, placeholder: "e.g. E102" },
            { name: "name", label: "Full name", required: true },
            { name: "department", label: "Department", type: "select", options: DEPARTMENTS, initial: DEPARTMENTS[0] },
            { name: "password", label: "Login password (optional)", type: "password", hint: PASSWORD_HINT }
          ]}
          submitLabel="Add worker"
          onClose={() => setDialog(null)}
          onSubmit={async (v) => {
            const r = await addWorker(v.workerId.trim(), v.name.trim(), v.department, v.password);
            onMessage(r.message, "success");
            load();
          }}
        />
      )}

      {dialog?.kind === "edit" && (
        <FormDialog
          title={`Edit ${dialog.worker.workerId}`}
          subtitle="The worker ID, status and assignments are kept."
          fields={[
            { name: "name", label: "Full name", required: true, initial: dialog.worker.name },
            { name: "department", label: "Department", type: "select", options: DEPARTMENTS, initial: DEPARTMENTS.includes(dialog.worker.department) ? dialog.worker.department : DEPARTMENTS[4] }
          ]}
          submitLabel="Save changes"
          onClose={() => setDialog(null)}
          onSubmit={async (v) => {
            const r = await editWorker(dialog.worker.workerId, v.name.trim(), v.department);
            onMessage(r.message, "success");
            load();
          }}
        />
      )}

      {dialog?.kind === "password" && (
        <FormDialog
          title={`${dialog.worker.hasLogin ? "Reset" : "Set"} password for ${dialog.worker.name}`}
          subtitle="Any open sessions for this worker will be logged out."
          fields={[
            { name: "password", label: "New password", type: "password", required: true, hint: PASSWORD_HINT },
            { name: "confirm", label: "Confirm password", type: "password", required: true }
          ]}
          submitLabel="Save password"
          onClose={() => setDialog(null)}
          onSubmit={async (v) => {
            if (v.password !== v.confirm) throw new Error("Passwords do not match");
            const r = await setWorkerPassword(dialog.worker.workerId, v.password);
            onMessage(r.message, "success");
            load();
          }}
        />
      )}
    </section>
  );
}
