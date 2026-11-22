# HostelCare — Smart Hostel Complaint & Maintenance Management System

Three separately authenticated portals (Student, Warden/Admin, Worker) on a
C++17 Crow backend and a React frontend. Data stays in the existing
pipe-delimited text files; the 12-field complaint format is unchanged.

## 1. Run locally (Windows + WSL Ubuntu)

Smart App Control blocks self-compiled Windows programs, so the backend is
built and run inside WSL.

**One-time setup** (Ubuntu window):
```bash
sudo apt update
sudo apt install -y build-essential cmake git libasio-dev libssl-dev libsqlite3-dev libcurl4-openssl-dev
mkdir -p ~/hostelcare-data
cd /mnt/c/Users/hp/Downloads/HostelCare_Final/HostelCare_Phase3_Starter/backend
cmake -S . -B ~/hc-build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/hc-build -j4
~/hc-build/hostelcare_tests            # expect: 105 passed, 0 failed
```

**Start the backend** (every time):
```bash
cd /mnt/c/Users/hp/Downloads/HostelCare_Final/HostelCare_Phase3_Starter/backend
HOSTELCARE_DATA_DIR=~/hostelcare-data HOSTELCARE_IMPORT_DIR=data ~/hc-build/hostelcare_api
```
The database lives in `~/hostelcare-data/hostelcare.db` (inside Linux: SQLite is
not reliable on `/mnt/c`). On the first start your `data/*.txt` files are
imported once; the text files are left untouched as a backup.

Optional Google Maps (see section 7): add
`HOSTELCARE_GOOGLE_MAPS_SERVER_KEY=your-key` in front of the command above.

**Start the frontend** (second terminal):
```powershell
cd frontend
npm install
npm run dev
```
Open http://localhost:5173. For embedded maps, create `frontend/.env.local` with
`VITE_GOOGLE_MAPS_EMBED_KEY=your-browser-key` and restart `npm run dev`.

## 2. First-time setup after upgrading

| Who | What to do |
|---|---|
| Warden | Log in as before. Open **Campus map** and paste real coordinates for each hostel and office. |
| Workers | Warden → **Workers** → **Set password**. Each worker then sets **My availability**. |
| Existing students | Log in once with Student ID; the app asks for the registered phone number and a new password. |

## 3. What each portal does

**Student:** report a problem (category, hostel, room, urgency), track every
status change on a timeline, get notifications, rate resolved complaints 1–5★,
change password.

**Warden/Admin:** dashboard of items needing review; verify / reject / forward /
reopen complaints (only valid transitions are offered); change priority; assign
or reassign workers (department mismatch or full workload needs confirmation);
one-click auto-assign; dispatch queue; worker management (add with password,
edit, set/reset password, activate/deactivate); analytics; feedback.

**Worker:** "Do this next" task chosen by the priority queue; exact Google
distance, walking time and one-tap Google Maps navigation from the worker's
live location or office (campus-map Dijkstra distance as fallback); accept →
start → resolve (note required); escalate; weekly working hours and leave;
completed work with ratings; change password.

## 4. Smart features — how they work (be ready to explain these)

- **Department matching** (`core/domain.cpp: departmentForComplaint`):
  Electrical→Electrical, Plumbing→Plumbing, Internet→IT Support,
  Security→Security, Cleaning/Furniture→General Maintenance, "Other" → keyword
  scan with whole-word matching.
- **Priority:** stored priority = max(student's choice, legacy keyword priority
  from `legacy/priorityc.cpp`). The warden can change it. Trade-off: students
  cannot under-report a fire, but can over-report — the warden override handles that.
- **Priority queue with ageing** (`priorityOrder`, a real `std::priority_queue`):
  order = effective priority → original priority → In Progress first → oldest →
  lowest ID. Every 48 h of waiting raises Low/Medium one level (max High) so
  old complaints are not starved; a genuinely High complaint still wins a tie.
- **Dispatch** (`chooseWorker`): uses the legacy
  `DispatchManager::findNearestWorker` — Dijkstra distance from the
  department office to the hostel, then fewest active tasks, then lowest ID.
  Workers with 5 active tasks, inactive workers, and the worker who escalated
  the complaint are excluded. "Run dispatch" pops the queue in order and
  updates workloads after each assignment.
- **Campus graph fix:** the original map was two disconnected parts. Added
  "Maintenance Office" (ID 11) and a Junction 1–Junction 2 path; IDs 0–10 unchanged.
- **Notifications, ratings, analytics** (resolution time from the status
  history, 14-day trend, per-worker performance).
- **Worker availability** (`dutyStatus` in `core/domain.cpp`): weekly hours
  (overnight shifts supported, e.g. 22:00–06:00) and leave dates. Auto-dispatch
  only picks on-duty workers; a Critical complaint may call in an off-duty
  worker if nobody is on duty; workers on leave are never chosen. Manual
  assignment of an off-duty worker asks the warden to confirm.
- **Google Maps** (`core/routes_client.cpp`, `taskDirections` in
  `http/api_worker.cpp`): the server calls the **Routes API** `computeRoutes`
  (Google made Directions/Distance Matrix "Legacy" in March 2025). The secret
  server key never reaches the browser; results are cached 10 minutes. The
  worker's live location comes from the browser (HTTPS or localhost only). If
  Google fails or coordinates are missing, the Dijkstra campus distance is shown
  with the reason. Dispatch still ranks workers by campus-graph distance —
  calling Google for every candidate would cost money for little benefit.

## 5. Database

SQLite (`hostelcare.db`): tables for complaints, students, workers, admins,
accounts, status_history, feedback, notifications, sessions,
worker_availability and locations. Each change and its history entry and
notifications are written in one transaction (all or nothing). Sessions are
stored as SHA-256 hashes, so users stay logged in across restarts and a copied
database cannot be used to log in. All SQL lives in `core/store_sqlite.cpp`.

## 6. Security

- Every endpoint except login/registration, health and campus routes requires a
  session token; the **server** filters data by role (students see only their
  own complaints, workers only theirs). The old UI-only filtering is gone.
- Passwords for all three roles: PBKDF2-HMAC-SHA256, 200,000 iterations,
  random salt, constant-time comparison. No plaintext passwords stored.
- 256-bit random session tokens; 30-minute idle timeout, 8-hour maximum.
- 5 failed logins in 15 minutes → 5-minute lock (HTTP 429).
- Deactivation and password changes log the user out everywhere immediately.
- Input validation (lengths, IDs, phone format, `|`/newline stripping).
- Data files are written to a temp file and swapped in, so a crash cannot leave
  a half-written `complaints.txt`; unreadable lines are kept, never dropped.

## 7. Google Maps setup

1. Google Cloud console → create a project → add a **billing account** (Google
   requires one even for the free Maps Embed API).
2. Enable **Routes API** and **Maps Embed API**. Do not enable the Legacy
   Directions / Distance Matrix APIs.
3. Create two API keys:
   - **Server key** → restrict to *Routes API* (and your server's IP when deployed).
     Put it in `HOSTELCARE_GOOGLE_MAPS_SERVER_KEY`.
   - **Browser key** → restrict to *Maps Embed API* and *HTTP referrers*
     (`http://localhost:5173/*`, `https://your-domain/*`).
     Put it in `VITE_GOOGLE_MAPS_EMBED_KEY` / `GOOGLE_MAPS_EMBED_KEY`.
4. Set a **budget alert and a daily quota** on the Routes API. Check the current
   prices on Google's pricing page before going live; the Maps Embed API is free.
5. In the app: Warden → **Campus map** → paste coordinates for every hostel and office.

Without keys everything still works: workers get the campus-map distance and
an "Open in Google Maps" link (links need no key once coordinates are set).

## 8. Deploy with HTTPS (Docker)

Needs a Linux server (any small VPS), a domain name, and ports 80/443 open.

```bash
# on the server
curl -fsSL https://get.docker.com | sh          # installs Docker
git clone <your-repo-url> HostelCare && cd HostelCare
cp .env.example .env && nano .env               # domain, admin password, keys
docker compose up -d --build                    # builds, runs tests, starts
```
Point your domain's DNS **A record** to the server IP first; Caddy then gets a
free HTTPS certificate automatically. The backend container is not exposed to
the internet; Caddy forwards `/api/*` to it.

- **Bring existing data:** `docker compose create backend`, then
  `docker compose cp backend/data/. backend:/data/` before the first
  `docker compose up` — the text files are imported on first start.
- **Backups:** `./scripts/backup.sh` (schedule daily with cron).
- **Update:** `git pull && docker compose up -d --build`.
- **Public demo:** deploy a *separate* copy with `HOSTELCARE_DEMO_MODE=1` and
  `HOSTELCARE_IMPORT_DIR=/app/demo-data` (fictional data, demo login buttons).
  Reset it with `docker compose down -v && docker compose up -d`.

GitHub: `.gitignore` excludes `backend/data/`, `.env` and databases so real
student data and keys are never published. CI (`.github/workflows/ci.yml`)
builds the backend, runs the API tests on anonymised fixtures, builds the
frontend and both Docker images on every push.

## 9. Code layout (new files)

```
backend/src/core/store_sqlite.cpp   all SQL; one-time text import
backend/src/core/routes_client.cpp  Google Routes API (libcurl) + cache
backend/tests/fixtures/             anonymised data used by the tests
backend/demo-data/                  fictional data for the public demo
backend/Dockerfile, frontend/Dockerfile, frontend/Caddyfile, docker-compose.yml
frontend/src/components/Directions.jsx, pages/AvailabilityPage.jsx, pages/CampusMapPage.jsx
```

## 10. Known limitations (honest list for your report)

- SQLite with one connection and one global lock: fine for a hostel, not for
  thousands of concurrent users (that would need PostgreSQL and a connection pool).
- Login throttling is in memory (resets on restart).
- No email verification or password-reset-by-email yet; a warden resets worker
  passwords, and existing students activate with their phone number once.
- Existing-student activation trusts the phone number once; someone who knows a
  student's phone could claim an unactivated account first.
- Admin notifications are shared by all admins (one read state).
- Campus-graph distances are illustrative; Google distances need real coordinates.
- The Docker/CI files were written carefully but could not be run in the
  environment where they were written; the first `docker compose up --build`
  is their real test.
