// HostelCare API client.
// Every request sends the current session token. The server decides what each
// role may see or do; the UI only hides buttons the server would refuse.

const API_BASE = import.meta.env.VITE_API_BASE || "http://localhost:18080/api";

let authToken = null;

export function setAuthToken(token) {
  authToken = token;
}

export class ApiError extends Error {
  constructor(message, status, code) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function request(path, { method = "GET", body, auth = true } = {}) {
  const headers = {};
  if (body !== undefined) headers["Content-Type"] = "application/json";
  if (auth && authToken) headers.Authorization = `Bearer ${authToken}`;

  let response;
  try {
    response = await fetch(`${API_BASE}${path}`, {
      method,
      headers,
      body: body === undefined ? undefined : JSON.stringify(body)
    });
  } catch {
    throw new ApiError(
      "Cannot reach the HostelCare server. Check that the backend is running on port 18080.",
      0,
      "OFFLINE"
    );
  }

  let data = {};
  try {
    data = await response.json();
  } catch {
    data = {};
  }

  if (!response.ok) {
    const error = new ApiError(
      data.message || `Request failed (${response.status})`,
      response.status,
      data.code
    );
    if (response.status === 401 && auth && authToken) {
      window.dispatchEvent(new CustomEvent("hostelcare:session-expired"));
    }
    throw error;
  }
  return data;
}

const get = (path) => request(path);
const post = (path, body = {}) => request(path, { method: "POST", body });
const put = (path, body = {}) => request(path, { method: "PUT", body });

// ---------- Public ----------
export const getHealth = () => request("/health", { auth: false });
export const getMeta = () => request("/meta", { auth: false });
export const getRouteLocations = () => request("/routes/locations", { auth: false });
export const getShortestRoute = (from, to) =>
  request(`/routes/shortest/${from}/${to}`, { auth: false });

// ---------- Authentication ----------
export const loginAdmin = (username, password) =>
  request("/admin/login", { method: "POST", body: { username, password }, auth: false });
export const loginWorker = (workerId, password) =>
  request("/worker/login", { method: "POST", body: { workerId, password }, auth: false });
export const loginStudent = (studentId, password) =>
  request("/student/login", { method: "POST", body: { studentId, password }, auth: false });
export const activateStudent = (studentId, phone, password) =>
  request("/student/activate", { method: "POST", body: { studentId, phone, password }, auth: false });
export const registerStudent = (data) =>
  request("/student/register", { method: "POST", body: data, auth: false });
export const getMe = () => get("/auth/me");
export const logout = () => post("/auth/logout");
export const changePassword = (currentPassword, newPassword) =>
  put("/auth/password", { currentPassword, newPassword });

// ---------- Complaints ----------
export const getComplaints = () => get("/complaints");
export const createComplaint = (data) => post("/complaints", data);
export const getComplaintHistory = (id) => get(`/complaints/${id}/history`);
export const updateComplaintStatus = (id, status, note = "") =>
  put(`/complaints/${id}/status`, { status, note });
export const updateComplaintPriority = (id, priority) =>
  put(`/complaints/${id}/priority`, { priority });
export const assignWorker = (complaintId, workerId, override = false) =>
  post("/complaints/assign", { complaintId: Number(complaintId), workerId, override });
export const submitFeedback = (id, rating, comment) =>
  post(`/complaints/${id}/feedback`, { rating, comment });

// ---------- Admin ----------
export const getAdminDashboard = () => get("/admin/dashboard");
export const getAnalytics = () => get("/admin/analytics");
export const getDispatchOptions = (id) => get(`/complaints/${id}/dispatch`);
export const autoAssignComplaint = (id) => post(`/complaints/${id}/auto-assign`);
export const getDispatchQueue = () => get("/dispatch/queue");
export const runDispatch = () => post("/dispatch/run");
export const getFeedback = () => get("/feedback");

export const fetchWorkers = async () => (await get("/workers")).workers || [];
export const addWorker = (workerId, name, department, password) =>
  post("/workers/add", { workerId, name, department, password });
export const editWorker = (workerId, name, department) =>
  put("/workers/edit", { workerId, name, department });
export const updateWorkerStatus = (workerId, isActive) =>
  put("/workers/status", { workerId, isActive });
export const setWorkerPassword = (workerId, password) =>
  put("/workers/password", { workerId, password });

// ---------- Worker portal ----------
export const getWorkerTasks = () => get("/worker/tasks");
export const getNextWorkerTask = () => get("/worker/tasks/next");
export const getCompletedWorkerTasks = () => get("/worker/tasks/completed");
export const acceptWorkerTask = (complaintId) => post("/worker/tasks/accept", { complaintId });
export const startWorkerTask = (complaintId) => put("/worker/tasks/start", { complaintId });
export const resolveWorkerTask = (complaintId, note) =>
  put("/worker/tasks/resolve", { complaintId, note });
export const escalateWorkerTask = (complaintId, note) =>
  put("/worker/tasks/escalate", { complaintId, note });

// ---------- Notifications ----------
export const getNotifications = () => get("/notifications");
export const markNotificationsRead = (id) =>
  post("/notifications/read", id ? { id } : {});

// ---------- Availability, campus map & directions ----------
export const getMyAvailability = () => get("/worker/availability");
export const saveMyAvailability = (data) => put("/worker/availability", data);
export const getLocations = () => get("/locations");
export const setLocationCoordinates = (id, latitude, longitude) =>
  put(`/locations/${id}`, { latitude, longitude });
export const clearLocationCoordinates = (id) => put(`/locations/${id}`, { clear: true });
export const getDirections = (complaintId, position) =>
  post(`/worker/tasks/${complaintId}/directions`, position || {});

// Browser key for the free Maps Embed API (restrict it to your domain in
// Google Cloud). Leave empty to hide embedded maps; links still work.
export const MAPS_EMBED_KEY = import.meta.env.VITE_GOOGLE_MAPS_EMBED_KEY || "";
