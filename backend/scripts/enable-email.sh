#!/bin/bash
# Run on the EC2 instance to update the TE backend in place:
# pull the latest code, install build deps (incl. libcurl), rebuild,
# turn on Gmail quote alerts, restart the service.
#
# Usage:
#   sudo GMAIL_APP_PASSWORD='xxxx xxxx xxxx xxxx' bash enable-email.sh
# GMAIL_USER and NOTIFY_TO default to taiwanenterprisellc@gmail.com.

set -e

GMAIL_USER="${GMAIL_USER:-taiwanenterprisellc@gmail.com}"
NOTIFY_TO="${NOTIFY_TO:-$GMAIL_USER}"

if [ -z "$GMAIL_APP_PASSWORD" ]; then
    echo "ERROR: GMAIL_APP_PASSWORD is not set"
    exit 1
fi

echo "=== TE update starting ==="

apt-get update -y
apt-get install -y libcurl4-openssl-dev

cd /opt/te
git pull --ff-only

cmake -S /opt/te/backend -B /opt/te/backend/build
cmake --build /opt/te/backend/build -j1

cp /opt/te/backend/te-backend.service /etc/systemd/system/te-backend.service

# Fill in the Gmail alert settings in the service file.
sed -i "s/^#\?Environment=GMAIL_USER=.*/Environment=GMAIL_USER=$GMAIL_USER/" /etc/systemd/system/te-backend.service
sed -i "s/^#\?Environment=GMAIL_APP_PASSWORD=.*/Environment=GMAIL_APP_PASSWORD=$GMAIL_APP_PASSWORD/" /etc/systemd/system/te-backend.service
sed -i "s/^#\?Environment=NOTIFY_TO=.*/Environment=NOTIFY_TO=$NOTIFY_TO/" /etc/systemd/system/te-backend.service

systemctl daemon-reload
systemctl restart te-backend
sleep 3

systemctl is-active te-backend
curl -s -m 8 http://localhost/api/services | head -c 120
echo
echo "=== TE update finished ==="
