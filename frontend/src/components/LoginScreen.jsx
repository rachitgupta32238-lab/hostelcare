import { useEffect, useState } from "react";
import { Building2, GraduationCap, ShieldCheck, Wrench } from "lucide-react";
import {
  activateStudent,
  getMeta,
  loginAdmin,
  loginStudent,
  loginWorker,
  registerStudent
} from "../services/api";

const HOSTELS = ["Atlantis Hostel", "SC Bose Hostel", "Bhagat Singh Hostel", "Sardar Patel Hostel"];

const PORTALS = [
  { id: "student", label: "Student", icon: GraduationCap },
  { id: "admin", label: "Warden", icon: ShieldCheck },
  { id: "worker", label: "Worker", icon: Wrench }
];

export default function LoginScreen({ onLogin, notice }) {
  const [portal, setPortal] = useState("student");
  const [studentMode, setStudentMode] = useState("login"); // login | register | activate
  const [form, setForm] = useState({});
  const [error, setError] = useState("");
  const [info, setInfo] = useState(notice || "");
  const [busy, setBusy] = useState(false);
  const [demoAccounts, setDemoAccounts] = useState([]);

  useEffect(() => {
    getMeta().then((m) => setDemoAccounts(m.demoMode ? m.demoAccounts || [] : [])).catch(() => {});
  }, []);

  async function tryDemo(account) {
    setError("");
    setBusy(true);
    try {
      const login = { admin: loginAdmin, worker: loginWorker, student: loginStudent }[account.role];
      onLogin(await login(account.login, account.password));
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  const field = (name) => ({
    value: form[name] || "",
    onChange: (e) => setForm({ ...form, [name]: e.target.value })
  });

  function switchTo(next, mode = "login") {
    setPortal(next);
    setStudentMode(mode);
    setError("");
    setForm((f) => ({ studentId: f.studentId }));
  }

  async function submit(e) {
    e.preventDefault();
    setError("");
    setInfo("");
    setBusy(true);
    try {
      if (portal === "admin") {
        onLogin(await loginAdmin(form.username?.trim() || "", form.password || ""));
      } else if (portal === "worker") {
        onLogin(await loginWorker(form.workerId?.trim() || "", form.password || ""));
      } else if (studentMode === "login") {
        onLogin(await loginStudent(form.studentId?.trim() || "", form.password || ""));
      } else if (studentMode === "activate") {
        if (form.password !== form.confirm) throw new Error("Passwords do not match");
        onLogin(await activateStudent(form.studentId?.trim() || "", form.phone?.trim() || "", form.password));
      } else {
        if (form.password !== form.confirm) throw new Error("Passwords do not match");
        const result = await registerStudent({
          studentId: form.studentId?.trim(),
          name: form.name?.trim(),
          room: form.room?.trim(),
          phone: form.phone?.trim(),
          hostel: form.hostel || HOSTELS[0],
          password: form.password
        });
        setStudentMode("login");
        setForm({ studentId: form.studentId });
        setInfo(result.message);
      }
    } catch (err) {
      if (err.code === "PASSWORD_NOT_SET") {
        setStudentMode("activate");
        setInfo("Your account was created before passwords were introduced. Confirm your registered phone number and choose a password.");
      } else {
        setError(err.message);
      }
    } finally {
      setBusy(false);
    }
  }

  const passwordHint = "At least 8 characters, with a letter and a digit.";

  return (
    <div className="login-screen">
      <div className="login-card">
        <div className="login-brand-icon"><Building2 size={30} /></div>
        <h1>HostelCare</h1>
        <p className="login-subtitle">Hostel complaints and maintenance, tracked end to end.</p>

        <div className="portal-tabs" role="tablist">
          {PORTALS.map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              type="button"
              role="tab"
              aria-selected={portal === id}
              className={portal === id ? "active" : ""}
              onClick={() => switchTo(id)}
            >
              <Icon size={16} /> {label}
            </button>
          ))}
        </div>

        {demoAccounts.length > 0 && (
          <div className="demo-box">
            <b>Try the demo</b>
            <div>
              {demoAccounts.map((a) => (
                <button key={a.role} type="button" className="ghost-btn" disabled={busy} onClick={() => tryDemo(a)}>
                  {a.role === "admin" ? "Warden" : a.role === "worker" ? "Worker" : "Student"}
                </button>
              ))}
            </div>
            <small>Sample data only. It may be reset at any time.</small>
          </div>
        )}

        {info && <p className="login-info">{info}</p>}

        <form className="form" onSubmit={submit}>
          {portal === "admin" && (
            <>
              <label><span>Username</span><input autoComplete="username" required {...field("username")} /></label>
              <label><span>Password</span><input type="password" autoComplete="current-password" required {...field("password")} /></label>
            </>
          )}

          {portal === "worker" && (
            <>
              <label><span>Worker ID</span><input placeholder="e.g. P101" autoComplete="username" required {...field("workerId")} /></label>
              <label><span>Password</span><input type="password" autoComplete="current-password" required {...field("password")} /></label>
              <small className="hint">No password yet? Ask the warden to set one for you.</small>
            </>
          )}

          {portal === "student" && (
            <>
              <label><span>Student ID</span><input autoComplete="username" required {...field("studentId")} /></label>

              {studentMode === "register" && (
                <>
                  <label><span>Full name</span><input required {...field("name")} /></label>
                  <div className="form-row">
                    <label><span>Hostel</span>
                      <select required {...field("hostel")}>
                        {HOSTELS.map((h) => <option key={h}>{h}</option>)}
                      </select>
                    </label>
                    <label><span>Room</span><input required {...field("room")} /></label>
                  </div>
                </>
              )}

              {studentMode !== "login" && (
                <label><span>Registered phone number</span>
                  <input type="tel" inputMode="numeric" pattern="[0-9]{10}" title="10 digits" required {...field("phone")} />
                </label>
              )}

              <label><span>{studentMode === "login" ? "Password" : "New password"}</span>
                <input
                  type="password"
                  autoComplete={studentMode === "login" ? "current-password" : "new-password"}
                  required
                  {...field("password")}
                />
              </label>
              {studentMode !== "login" && (
                <>
                  <label><span>Confirm password</span><input type="password" autoComplete="new-password" required {...field("confirm")} /></label>
                  <small className="hint">{passwordHint}</small>
                </>
              )}
            </>
          )}

          {error && <p className="form-error" role="alert">{error}</p>}

          <button type="submit" className="primary-btn wide" disabled={busy}>
            {busy
              ? "Please wait…"
              : portal === "student" && studentMode === "register"
              ? "Create account"
              : portal === "student" && studentMode === "activate"
              ? "Set password and log in"
              : "Log in"}
          </button>
        </form>

        {portal === "student" && (
          <div className="login-links">
            {studentMode === "login" ? (
              <>
                <button type="button" className="text-btn" onClick={() => switchTo("student", "register")}>New student? Create an account</button>
                <button type="button" className="text-btn" onClick={() => switchTo("student", "activate")}>Registered before? Set up your password</button>
              </>
            ) : (
              <button type="button" className="text-btn" onClick={() => switchTo("student", "login")}>Back to log in</button>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
