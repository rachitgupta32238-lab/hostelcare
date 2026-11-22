import { useCallback, useEffect, useMemo, useState } from "react";
import {
  BarChart3, Building2, CalendarClock, ClipboardList, LayoutDashboard, LogOut, MapPinned, Menu,
  Plus, RefreshCw, Send, Settings, Users, X
} from "lucide-react";
import { getHealth, getMe, logout, setAuthToken } from "./services/api";
import LoginScreen from "./components/LoginScreen";
import NotificationBell from "./components/NotificationBell";
import TrackingDialog from "./components/TrackingDialog";
import ComplaintForm from "./components/ComplaintForm";
import AdminOverview from "./pages/AdminOverview";
import ComplaintsPage from "./pages/ComplaintsPage";
import WorkersPage from "./pages/WorkersPage";
import DispatchPage from "./pages/DispatchPage";
import AnalyticsPage from "./pages/AnalyticsPage";
import WorkerDashboard from "./pages/WorkerDashboard";
import StudentOverview from "./pages/StudentOverview";
// import RoutesPage from "./pages/RoutesPage";
import SettingsPage from "./pages/SettingsPage";
import AvailabilityPage from "./pages/AvailabilityPage";
import CampusMapPage from "./pages/CampusMapPage";

const STORAGE_KEY = "hostelcareSession";

// Pages available to each role. The server enforces the same rules.
const NAV = {
  admin: [
    { page: "Overview", icon: LayoutDashboard, intro: "What needs your attention today." },
    { page: "Complaints", icon: ClipboardList, intro: "Verify, prioritise and assign every complaint." },
    { page: "Dispatch", icon: Send, intro: "Send waiting complaints to the nearest free worker." },
    { page: "Workers", icon: Users, intro: "Manage the maintenance team and their logins." },
    { page: "Analytics", icon: BarChart3, intro: "Trends, resolution times and student ratings." },
    { page: "Campus map", icon: MapPinned, intro: "Real map coordinates for exact worker directions." },
    // { page: "Campus routes", icon: Map, intro: "Shortest walking routes across campus." },
    { page: "Settings", icon: Settings, intro: "Your account." }
  ],
  worker: [
    { page: "My tasks", icon: LayoutDashboard, intro: "Your assigned work in priority order." },
    { page: "My availability", icon: CalendarClock, intro: "Your working hours and leave, shown to the warden." },
    // { page: "Campus routes", icon: Map, intro: "Shortest walking routes across campus." },
    { page: "Settings", icon: Settings, intro: "Your account." }
  ],
  student: [
    { page: "My complaints", icon: LayoutDashboard, intro: "Report problems and follow them until they are fixed." },
    { page: "Settings", icon: Settings, intro: "Your account." }
  ]
};

const PORTAL_NAME = { admin: "Warden portal", worker: "Worker portal", student: "Student portal" };

export default function App() {
  const [session, setSession] = useState(null); // { token, role, user }
  const [restoring, setRestoring] = useState(true);
  const [loginNotice, setLoginNotice] = useState("");
  const [page, setPage] = useState("");
  const [refreshKey, setRefreshKey] = useState(0);
  const [message, setMessage] = useState(null); // { text, type }
  const [apiState, setApiState] = useState("checking");
  const [tracking, setTracking] = useState(null);
  const [showForm, setShowForm] = useState(false);
  const [navOpen, setNavOpen] = useState(false);

  const refresh = useCallback(() => setRefreshKey((k) => k + 1), []);
  const showMessage = useCallback((text, type = "info") => setMessage({ text, type, at: Date.now() }), []);

  const startSession = useCallback((data) => {
    const next = { token: data.token, role: data.user.role, user: data.user };
    setAuthToken(next.token);
    sessionStorage.setItem(STORAGE_KEY, JSON.stringify({ token: next.token }));
    setSession(next);
    setPage(NAV[next.role][0].page);
    setLoginNotice("");
  }, []);

  const clearSession = useCallback((notice = "") => {
    setAuthToken(null);
    sessionStorage.removeItem(STORAGE_KEY);
    setSession(null);
    setTracking(null);
    setShowForm(false);
    setMessage(null); // never show one user's messages to the next
    setLoginNotice(notice);
  }, []);

  // Restore a session after a page reload. The server validates the token.
  useEffect(() => {
    const saved = JSON.parse(sessionStorage.getItem(STORAGE_KEY) || "null");
    if (!saved?.token) {
      setRestoring(false);
      return;
    }
    setAuthToken(saved.token);
    getMe()
      .then((data) => startSession({ token: saved.token, user: data.user }))
      .catch(() => clearSession())
      .finally(() => setRestoring(false));
  }, [startSession, clearSession]);

  useEffect(() => {
    const expired = () => clearSession("Your session expired. Please log in again.");
    window.addEventListener("hostelcare:session-expired", expired);
    return () => window.removeEventListener("hostelcare:session-expired", expired);
  }, [clearSession]);

  useEffect(() => {
    const check = () => getHealth().then(() => setApiState("online")).catch(() => setApiState("offline"));
    check();
    const timer = setInterval(check, 60000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!message) return;
    const timer = setTimeout(() => setMessage(null), message.type === "error" ? 8000 : 4500);
    return () => clearTimeout(timer);
  }, [message]);

  const nav = useMemo(() => (session ? NAV[session.role] : []), [session]);
  const current = nav.find((n) => n.page === page) || nav[0];

  async function handleLogout() {
    try {
      await logout();
    } catch {
      /* token may already be invalid */
    }
    clearSession("You have been logged out.");
  }

  if (restoring) return <div className="splash">Loading HostelCare…</div>;
  if (!session) return <LoginScreen onLogin={startSession} notice={loginNotice} />;

  const { role, user } = session;
  const pageProps = { refreshKey, onTrack: setTracking, onMessage: showMessage };

  return (
    <div className="app-shell">
      <aside className={`sidebar ${navOpen ? "open" : ""}`}>
        <div className="brand">
          <div className="brand-mark"><Building2 size={21} /></div>
          <div><b>HostelCare</b><span>{PORTAL_NAME[role]}</span></div>
          <button className="icon-btn nav-close" onClick={() => setNavOpen(false)} aria-label="Close menu"><X size={18} /></button>
        </div>
        <nav>
          {nav.map(({ page: p, icon: Icon }) => (
            <button
              key={p}
              className={`nav-item ${current.page === p ? "active" : ""}`}
              onClick={() => { setPage(p); setNavOpen(false); }}
            >
              <Icon size={18} />{p}
            </button>
          ))}
        </nav>
        <div className="sidebar-bottom">
          <div className="profile">
            <div className="avatar">{(user.name || "?").charAt(0).toUpperCase()}</div>
            <div>
              <b>{user.name}</b>
              <span>
                {role === "student" && `${user.studentId} · Room ${user.roomNo}`}
                {role === "worker" && `${user.workerId} · ${user.department}`}
                {role === "admin" && "Hostel warden"}
              </span>
            </div>
          </div>
          <button className="logout-btn" onClick={handleLogout}><LogOut size={16} /> Log out</button>
        </div>
      </aside>
      {navOpen && <div className="nav-backdrop" onClick={() => setNavOpen(false)} />}

      <main className="main-area">
        <header className="topbar">
          <div className="topbar-left">
            <button className="icon-btn nav-toggle" onClick={() => setNavOpen(true)} aria-label="Open menu"><Menu size={20} /></button>
            <div className="breadcrumb"><span>{PORTAL_NAME[role]}</span><b>/</b>{current.page}</div>
          </div>
          <div className="top-actions">
            <span className={`api-pill ${apiState}`}><i />{apiState === "online" ? "Connected" : apiState === "checking" ? "Connecting…" : "Server offline"}</span>
            <button className="icon-btn" title="Refresh" aria-label="Refresh" onClick={refresh}><RefreshCw size={17} /></button>
            <NotificationBell onNewActivity={refresh} />
          </div>
        </header>

        <div className="content">
          <div className="page-heading">
            <div>
              <h1>{current.page}</h1>
              <p>{current.intro}</p>
            </div>
            {role === "student" && current.page === "My complaints" && (
              <button className="primary-btn" onClick={() => setShowForm(true)}><Plus size={17} /> Report a problem</button>
            )}
          </div>

          {message && (
            <div className={`toast ${message.type}`} role={message.type === "error" ? "alert" : "status"}>
              <span>{message.text}</span>
              <button className="icon-btn" onClick={() => setMessage(null)} aria-label="Dismiss"><X size={15} /></button>
            </div>
          )}

          {role === "admin" && current.page === "Overview" && <AdminOverview {...pageProps} go={setPage} />}
          {role === "admin" && current.page === "Complaints" && <ComplaintsPage {...pageProps} />}
          {role === "admin" && current.page === "Dispatch" && <DispatchPage {...pageProps} />}
          {role === "admin" && current.page === "Workers" && <WorkersPage {...pageProps} />}
          {role === "admin" && current.page === "Analytics" && <AnalyticsPage {...pageProps} />}
          {role === "worker" && current.page === "My tasks" && <WorkerDashboard {...pageProps} user={user} />}
          {role === "student" && current.page === "My complaints" && (
            <StudentOverview {...pageProps} onNewComplaint={() => setShowForm(true)} />
          )}
          {role === "admin" && current.page === "Campus map" && <CampusMapPage onMessage={showMessage} />}
          {role === "worker" && current.page === "My availability" && <AvailabilityPage onMessage={showMessage} />}
          {/* {current.page === "Campus routes" && <RoutesPage onMessage={showMessage} />} */}
          {current.page === "Settings" && (
            <SettingsPage
              session={session}
              onMessage={showMessage}
              onTokenChanged={(token) => {
                setAuthToken(token);
                sessionStorage.setItem(STORAGE_KEY, JSON.stringify({ token }));
                setSession({ ...session, token });
              }}
            />
          )}
        </div>
        <footer>HostelCare · C++ Crow backend · React frontend{user.demo ? " · Demo account" : ""}</footer>
      </main>

      {tracking && (
        <TrackingDialog complaint={tracking} role={role} onClose={() => setTracking(null)} onRated={refresh} />
      )}
      {showForm && (
        <ComplaintForm
          student={user}
          onClose={() => setShowForm(false)}
          onCreated={(text) => { showMessage(text, "success"); refresh(); }}
        />
      )}
    </div>
  );
}
