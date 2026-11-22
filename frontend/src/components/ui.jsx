import { useEffect, useState } from "react";
import { X, Star } from "lucide-react";

export function Modal({ title, subtitle, onClose, children, wide = false }) {
  useEffect(() => {
    const onKey = (e) => e.key === "Escape" && onClose();
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onClose]);

  return (
    <div className="modal-overlay" onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div className={`modal ${wide ? "modal-wide" : ""}`} role="dialog" aria-modal="true" aria-label={title}>
        <div className="modal-header">
          <div>
            <h2>{title}</h2>
            {subtitle && <p>{subtitle}</p>}
          </div>
          <button className="icon-btn" onClick={onClose} aria-label="Close">
            <X size={18} />
          </button>
        </div>
        {children}
      </div>
    </div>
  );
}

// A small form in a modal. `fields`: [{ name, label, type, options, required,
// placeholder, initial, hint }]. onSubmit(values) may throw; the message is shown.
export function FormDialog({ title, subtitle, fields, submitLabel, onSubmit, onClose }) {
  const [values, setValues] = useState(() =>
    Object.fromEntries(fields.map((f) => [f.name, f.initial ?? ""]))
  );
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  async function handleSubmit(e) {
    e.preventDefault();
    setError("");
    setBusy(true);
    try {
      await onSubmit(values);
      onClose();
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <Modal title={title} subtitle={subtitle} onClose={onClose}>
      <form className="form" onSubmit={handleSubmit}>
        {fields.map((f) => (
          <label key={f.name}>
            <span>{f.label}</span>
            {f.type === "select" ? (
              <select
                value={values[f.name]}
                required={f.required}
                onChange={(e) => setValues({ ...values, [f.name]: e.target.value })}
              >
                {f.options.map((o) => (
                  <option key={o} value={o}>{o}</option>
                ))}
              </select>
            ) : f.type === "textarea" ? (
              <textarea
                rows={4}
                value={values[f.name]}
                required={f.required}
                placeholder={f.placeholder}
                onChange={(e) => setValues({ ...values, [f.name]: e.target.value })}
              />
            ) : (
              <input
                type={f.type || "text"}
                value={values[f.name]}
                required={f.required}
                placeholder={f.placeholder}
                autoComplete={f.type === "password" ? "new-password" : "off"}
                onChange={(e) => setValues({ ...values, [f.name]: e.target.value })}
              />
            )}
            {f.hint && <small className="hint">{f.hint}</small>}
          </label>
        ))}
        {error && <p className="form-error" role="alert">{error}</p>}
        <div className="modal-actions">
          <button type="button" className="secondary-btn" onClick={onClose}>Cancel</button>
          <button type="submit" className="primary-btn" disabled={busy}>
            {busy ? "Saving…" : submitLabel}
          </button>
        </div>
      </form>
    </Modal>
  );
}

const slug = (value) => String(value || "").toLowerCase().replace(/\s+/g, "-");

export function StatusBadge({ status }) {
  return <span className={`badge status-${slug(status)}`}>{status}</span>;
}

export function PriorityBadge({ priority, effective }) {
  const aged = effective && effective !== priority;
  return (
    <span
      className={`badge priority-${slug(aged ? effective : priority)}`}
      title={aged ? `Raised from ${priority} because it has been waiting` : undefined}
    >
      {aged ? `${effective} (was ${priority})` : priority}
    </span>
  );
}

export function Stars({ value, onChange, size = 18 }) {
  return (
    <span className="stars" role={onChange ? "radiogroup" : undefined} aria-label={`${value} out of 5`}>
      {[1, 2, 3, 4, 5].map((n) =>
        onChange ? (
          <button
            key={n}
            type="button"
            className={n <= value ? "on" : ""}
            onClick={() => onChange(n)}
            aria-label={`${n} star${n > 1 ? "s" : ""}`}
          >
            <Star size={size} />
          </button>
        ) : (
          <Star key={n} size={size} className={n <= value ? "on" : ""} />
        )
      )}
    </span>
  );
}

export function EmptyState({ icon: Icon, title, text, action }) {
  return (
    <div className="empty-state">
      {Icon && <div className="empty-icon"><Icon size={26} /></div>}
      <h3>{title}</h3>
      {text && <p>{text}</p>}
      {action}
    </div>
  );
}

export function RouteStops({ route }) {
  if (!route) return null;
  if (!route.pathExists) return <p className="muted">No route found on the campus map.</p>;
  return (
    <div className="route-line">
      <b>{route.totalDistance} m</b>
      <ol>
        {(route.locations || []).map((name, i) => (
          <li key={i}>{name}</li>
        ))}
      </ol>
    </div>
  );
}
