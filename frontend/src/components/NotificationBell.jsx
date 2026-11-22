import { useCallback, useEffect, useRef, useState } from "react";
import { Bell } from "lucide-react";
import { getNotifications, markNotificationsRead } from "../services/api";

const POLL_MS = 30000;

export default function NotificationBell({ onNewActivity }) {
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState([]);
  const [unread, setUnread] = useState(0);
  const lastUnread = useRef(null);
  const boxRef = useRef(null);

  const load = useCallback(async () => {
    try {
      const data = await getNotifications();
      setItems(data.notifications || []);
      setUnread(data.unread || 0);
      if (lastUnread.current !== null && data.unread > lastUnread.current) onNewActivity?.();
      lastUnread.current = data.unread;
    } catch {
      /* the API pill shows connection problems */
    }
  }, [onNewActivity]);

  useEffect(() => {
    load();
    const timer = setInterval(load, POLL_MS);
    return () => clearInterval(timer);
  }, [load]);

  useEffect(() => {
    const close = (e) => boxRef.current && !boxRef.current.contains(e.target) && setOpen(false);
    document.addEventListener("mousedown", close);
    return () => document.removeEventListener("mousedown", close);
  }, []);

  async function markAll() {
    await markNotificationsRead();
    load();
  }

  async function markOne(item) {
    if (!item.read) {
      await markNotificationsRead(item.id);
      load();
    }
  }

  return (
    <div className="bell" ref={boxRef}>
      <button
        className="icon-btn"
        aria-label={`Notifications${unread ? `, ${unread} unread` : ""}`}
        aria-expanded={open}
        onClick={() => {
          setOpen(!open);
          if (!open) load();
        }}
      >
        <Bell size={18} />
        {unread > 0 && <span className="bell-count">{unread > 99 ? "99+" : unread}</span>}
      </button>

      {open && (
        <div className="bell-panel">
          <div className="bell-head">
            <b>Notifications</b>
            {unread > 0 && <button className="text-btn" onClick={markAll}>Mark all read</button>}
          </div>
          {items.length === 0 ? (
            <p className="muted bell-empty">You're all caught up.</p>
          ) : (
            <ul>
              {items.map((n) => (
                <li key={n.id} className={n.read ? "" : "unread"} onClick={() => markOne(n)}>
                  <p>{n.message}</p>
                  <small>{n.timestamp}</small>
                </li>
              ))}
            </ul>
          )}
        </div>
      )}
    </div>
  );
}
