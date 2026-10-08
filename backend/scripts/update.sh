#!/bin/bash
# Update the TE backend on the EC2 instance WITHOUT touching any
# settings: pull the latest code, rebuild, restart, health check.
# Email alert environment variables in the installed service file
# are left exactly as they are.
#
# Usage on the instance:
#   curl -sL <raw-url-of-this-file> -o /tmp/update.sh && sudo bash /tmp/update.sh

set -e

echo "=== TE update starting ==="

cd /opt/te
git pull --ff-only

cmake -S /opt/te/backend -B /opt/te/backend/build
cmake --build /opt/te/backend/build -j1

systemctl restart te-backend
sleep 3

systemctl is-active te-backend
curl -s -m 8 http://localhost/api/services | head -c 120
echo
echo "=== TE update finished ==="
