import { useCallback, useEffect, useState } from "react";
import { ClipboardCheck, MapPin } from "lucide-react";
import {
  acceptWorkerTask,
  escalateWorkerTask,
  getCompletedWorkerTasks,
  getWorkerTasks,
  resolveWorkerTask,
  startWorkerTask
} from "../services/api";
import { EmptyState, FormDialog, PriorityBadge, StatusBadge, Stars } from "../components/ui";
import Directions from "../components/Directions";

export default function WorkerDashboard({ user, refreshKey, onTrack, onMessage }) {
  const [tasks, setTasks] = useState(null);
  const [office, setOffice] = useState("");
  const [completed, setCompleted] = useState({ tasks: [], averageRating: 0, ratingsCount: 0 });
  const [dialog, setDialog] = useState(null); // { kind: "resolve" | "escalate", task }
  const [busyId, setBusyId] = useState(null);
  const [routeFor, setRouteFor] = useState(null); // queue item with directions open

  const load = useCallback(async () => {
    try {
      const [active, done] = await Promise.all([getWorkerTasks(), getCompletedWorkerTasks()]);
      setTasks(active.tasks || []);
      setOffice(active.office || "");
      setCompleted(done);
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load, refreshKey]);

  async function act(task, action, label) {
    setBusyId(task.id);
    try {
      await action(task.id);
      onMessage(`Task #${task.id} ${label}`, "success");
      await load();
    } catch (err) {
      onMessage(err.message, "error");
    } finally {
      setBusyId(null);
    }
  }

  const list = tasks || [];
  const next = list[0];
  const counts = {
    waiting: list.filter((t) => t.status === "Assigned").length,
    accepted: list.filter((t) => t.status === "Accepted").length,
    working: list.filter((t) => t.status === "In Progress").length
  };

  const actions = (task) => (
    <div className="task-actions">
      {task.status === "Assigned" && (
        <button className="primary-btn" disabled={busyId === task.id} onClick={() => act(task, acceptWorkerTask, "accepted")}>Accept task</button>
      )}
      {task.status === "Accepted" && (
        <button className="primary-btn" disabled={busyId === task.id} onClick={() => act(task, startWorkerTask, "started")}>Start work</button>
      )}
      {task.status === "In Progress" && (
        <button className="primary-btn" onClick={() => setDialog({ kind: "resolve", task })}>Mark resolved</button>
      )}
      {(task.status === "Accepted" || task.status === "In Progress") && (
        <button className="ghost-btn danger" onClick={() => setDialog({ kind: "escalate", task })}>Escalate</button>
      )}
      <button className="ghost-btn" onClick={() => onTrack(task)}>Details</button>
    </div>
  );

  return (
    <>
      <div className="worker-hero">
        <div>
          <h2>Hello, {user.name}</h2>
          <p>{user.department} · starting point: {office || user.office}</p>
        </div>
        <dl className="hero-stats">
          <div><dt>To accept</dt><dd>{counts.waiting}</dd></div>
          <div><dt>Accepted</dt><dd>{counts.accepted}</dd></div>
          <div><dt>In progress</dt><dd>{counts.working}</dd></div>
          <div><dt>Resolved</dt><dd>{completed.count ?? completed.tasks.length}</dd></div>
          <div><dt>Rating</dt><dd>{completed.ratingsCount ? completed.averageRating.toFixed(1) : "–"}</dd></div>
        </dl>
      </div>

      {tasks === null ? (
        <p className="muted">Loading your tasks…</p>
      ) : !next ? (
        <section className="panel">
          <EmptyState icon={ClipboardCheck} title="No open tasks" text="New assignments appear here and in your notifications." />
        </section>
      ) : (
        <section className="panel next-task">
          <div className="next-label">Do this next</div>
          <div className="next-body">
            <div>
              <h3>#{next.id} {next.title}</h3>
              <p>{next.description}</p>
              <div className="chips">
                <PriorityBadge priority={next.priority} effective={next.effectivePriority} />
                <StatusBadge status={next.status} />
                <span className="chip"><MapPin size={13} /> {next.hostelName}, room {next.roomNo}</span>
              </div>
              {actions(next)}
            </div>
            <div className="next-route">
                            <span className="route-title">Getting there</span>
              <Directions key={next.id} task={next} />
            </div>
          </div>
        </section>
      )}

      {list.length > 1 && (
        <section className="panel">
          <div className="panel-heading"><div><h3>Queue</h3><p>Ordered by priority, then by how long the student has waited.</p></div></div>
          <ul className="task-list">
            {list.slice(1).map((task) => (
              <li key={task.id}>
                <div className="task-main">
                  <span className="queue-pos">{task.queuePosition}</span>
                  <div>
                    <b>#{task.id} {task.title}</b>
                    <small>{task.hostelName}, room {task.roomNo}</small>
                    </div>
                  <PriorityBadge priority={task.priority} effective={task.effectivePriority} />
                  <StatusBadge status={task.status} />
                </div>
                {actions(task)}
                <button className="text-btn" onClick={() => setRouteFor(routeFor === task.id ? null : task.id)}>
                  {routeFor === task.id ? "Hide directions" : "Directions"}
                </button>
                {routeFor === task.id && <div className="queue-directions"><Directions task={task} /></div>}
              </li>
            ))}
          </ul>
        </section>
      )}

      <section className="panel">
        <div className="panel-heading"><div><h3>Completed work</h3><p>Resolved tasks and student ratings.</p></div></div>
        {completed.tasks.length === 0 ? <p className="muted">Nothing resolved yet.</p> : (
          <ul className="task-list">
            {completed.tasks.map((task) => (
              <li key={task.id}>
                <div className="task-main">
                  <div>
                    <b>#{task.id} {task.title}</b>
                    <small>{task.feedback}</small>
                  </div>
                  {task.rating > 0 ? <Stars value={task.rating} size={14} /> : <span className="muted">Not rated</span>}
                </div>
              </li>
            ))}
          </ul>
        )}
      </section>

      {dialog && (
        <FormDialog
          title={dialog.kind === "resolve" ? `Resolve task #${dialog.task.id}` : `Escalate task #${dialog.task.id}`}
          subtitle={dialog.kind === "resolve" ? "The student sees this note and can rate the work." : "The warden will review and reassign this complaint."}
          fields={[{
            name: "note",
            label: dialog.kind === "resolve" ? "What did you fix?" : "Why does this need escalation?",
            type: "textarea",
            required: true
          }]}
          submitLabel={dialog.kind === "resolve" ? "Mark resolved" : "Escalate"}
          onClose={() => setDialog(null)}
          onSubmit={async ({ note }) => {
            const fn = dialog.kind === "resolve" ? resolveWorkerTask : escalateWorkerTask;
            await fn(dialog.task.id, note.trim());
            onMessage(`Task #${dialog.task.id} ${dialog.kind === "resolve" ? "resolved" : "escalated"}`, "success");
            await load();
          }}
        />
      )}
    </>
  );
}
