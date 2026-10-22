# HostelCare Integrated

Build from this folder (the working directory must contain the `data` folder):

```powershell
g++ *.cpp -o HostelCare.exe
.\HostelCare.exe
```

Worker demo accounts (password for all: `worker123`): `E101`, `P101`, `IT101`, `S101`, `G101`.

The Worker Portal reads and updates `data/complaints.txt`, the same pipe-delimited file used by Student and Warden. Only complaints with status `Forwarded` can be accepted. Category names are mapped to departments (Electrical, Plumbing, IT Support, Security, or General Maintenance). Worker actions persist status, worker ID, and feedback to the shared file.
