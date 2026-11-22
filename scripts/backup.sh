#!/usr/bin/env bash
# Safe online backup of the SQLite database (run on the server, from the project folder).
# Keeps 14 daily copies in ./backups. Schedule with cron, e.g.:
#   0 3 * * * cd /opt/HostelCare && ./scripts/backup.sh
set -euo pipefail
mkdir -p backups
stamp=$(date +%Y-%m-%d_%H%M)
docker compose exec -T backend sqlite3 /data/hostelcare.db ".backup '/data/backup.db'"
docker compose cp backend:/data/backup.db "backups/hostelcare_${stamp}.db"
docker compose exec -T backend rm -f /data/backup.db
ls -1t backups/hostelcare_*.db | tail -n +15 | xargs -r rm --
echo "Backup saved: backups/hostelcare_${stamp}.db"
