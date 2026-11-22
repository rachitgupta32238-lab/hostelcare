import { useEffect, useState } from "react";
import { getComplaintHistory, submitFeedback } from "../services/api";
import { Modal, PriorityBadge, StatusBadge, Stars } from "./ui";

export default function TrackingDialog({ complaint, role, onClose, onRated }) {
  const [history, setHistory] = useState(null);
  const [error, setError] = useState("");
  const [rating, setRating] = useState(0);
  const [comment, setComment] = useState("");
  const [sending, setSending] = useState(false);
  const [rated, setRated] = useState(false);

  useEffect(() => {
    getComplaintHistory(complaint.id)
      .then((data) => setHistory(data.history || []))
      .catch((err) => setError(err.message));
  }, [complaint.id]);

  async function rate(e) {
    e.preventDefault();
    if (!rating) {
      setError("Choose a rating from 1 to 5 stars.");
      return;
    }
    setSending(true);
    setError("");
    try {
      await submitFeedback(complaint.id, rating, comment.trim());
      setRated(true);
      onRated?.();
    } catch (err) {
      setError(err.message);
    } finally {
      setSending(false);
    }
  }

  return (
    <Modal title={`Complaint #${complaint.id}`} subtitle={complaint.title} onClose={onClose} wide>
      <div className="detail-grid">
        <div><span>Status</span><StatusBadge status={complaint.status} /></div>
        <div><span>Priority</span><PriorityBadge priority={complaint.priority} effective={complaint.effectivePriority} /></div>
        <div><span>Department</span>{complaint.department}</div>
        <div><span>Location</span>{complaint.hostelName}, room {complaint.roomNo}</div>
        <div><span>Worker</span>{complaint.workerName || "Not assigned yet"}</div>
        <div><span>Submitted</span>{complaint.submittedAt || "Before tracking began"}</div>
      </div>

      <p className="detail-description">{complaint.description}</p>
      {complaint.feedback && (
        <p className="worker-note"><b>Worker note:</b> {complaint.feedback}</p>
      )}

      <h4>Timeline</h4>
      {error && <p className="form-error" role="alert">{error}</p>}
      {history === null ? (
        <p className="muted">Loading timeline…</p>
      ) : history.length === 0 ? (
        <p className="muted">No timeline recorded for this complaint.</p>
      ) : (
        <ol className="timeline">
          {history.map((event, i) => (
            <li key={i}>
              <b>{event.status}</b>
              <span>{event.timestamp}{event.actor ? ` · ${event.actor}` : ""}</span>
            </li>
          ))}
        </ol>
      )}

      {complaint.rating > 0 && (
        <div className="rating-box">
          <h4>Student rating</h4>
          <Stars value={complaint.rating} />
          {complaint.ratingComment && <p>“{complaint.ratingComment}”</p>}
        </div>
      )}

      {role === "student" && complaint.canRate && !rated && (
        <form className="rating-box form" onSubmit={rate}>
          <h4>How was the repair?</h4>
          <Stars value={rating} onChange={setRating} size={24} />
          <label>
            <span>Comment (optional)</span>
            <textarea rows={2} maxLength={300} value={comment} onChange={(e) => setComment(e.target.value)} />
          </label>
          <button className="primary-btn" type="submit" disabled={sending}>
            {sending ? "Sending…" : "Submit rating"}
          </button>
        </form>
      )}
      {rated && <p className="success-text">Thanks — your rating was sent to the warden and the worker.</p>}
    </Modal>
  );
}
